const { onCall, HttpsError, onRequest } = require("firebase-functions/v2/https");
const { logger } = require("firebase-functions");
const admin = require("firebase-admin");
const express = require("express");
const cors = require("cors");
const axios = require("axios");
const SpotifyWebApi = require("spotify-web-api-node");
const multer = require("multer");

if (!admin.apps.length) {
  admin.initializeApp();
}

const db = admin.firestore();
const app = express();
app.use(cors({ origin: true }));
app.use(express.json());

// Legacy configuration
let spotifyApi;
let refreshTime = 0;
const audioShakeToken = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJjbGllbnRJZCI6ImNsdnpva20zYjBieHVvYzh4cXQ2anl5NzEiLCJsaWNlbnNlSWQiOiJjbHZ6b2t2dTkwNjUzOHhvYzZuODllMjR0IiwiaWF0IjoxNzE1Mjg1Mjk5LCJleHAiOjE4NzI5NjUyOTl9.FAmiatEj00tIx0yWKD3bSzYoPrzJ6h2B5THBVCmlnyE';

const COLLECTION_METADATA = "metadataSongs";
const COLLECTION_QUEUE = "metadataFetchQueue";
const DEFAULT_STALE_DAYS = 30;

function normalizeText(input) {
  return String(input || "")
    .toLowerCase()
    .trim()
    .replace(/,\s*the$/i, "")
    .replace(/^the\s+/i, "")
    .replace(/[^a-z0-9\s]/g, "")
    .replace(/\s+/g, " ")
    .trim();
}

function normalizeKey(artistName, songName) {
  return `${normalizeText(artistName)}|${normalizeText(songName)}`;
}

function nowMinusDays(days) {
  const d = new Date();
  d.setUTCDate(d.getUTCDate() - days);
  return d;
}

exports.enqueueMetadataFetch = onCall(
  {
    region: "us-central1",
    enforceAppCheck: false,
    cors: true
  },
  async (request) => {
    const data = request.data || {};
    const artistName = String(data.artistName || "").trim();
    const songName = String(data.songName || "").trim();
    const requestedKey = String(data.normalizedKey || "").trim();
    const appVersion = String(data.appVersion || "").trim();

    if (!artistName || !songName) {
      throw new HttpsError("invalid-argument", "artistName and songName are required.");
    }

    const normalizedKey = requestedKey || normalizeKey(artistName, songName);
    if (!normalizedKey || normalizedKey === "|") {
      throw new HttpsError("invalid-argument", "Unable to build normalized key.");
    }

    const metadataRef = db.collection(COLLECTION_METADATA).doc(normalizedKey);
    const queueRef = db.collection(COLLECTION_QUEUE).doc(normalizedKey);

    const staleBefore = admin.firestore.Timestamp.fromDate(nowMinusDays(DEFAULT_STALE_DAYS));
    const metadataSnap = await metadataRef.get();

    if (metadataSnap.exists) {
      const doc = metadataSnap.data() || {};
      const updatedAt = doc.updatedAt;
      if (updatedAt && updatedAt.toMillis && updatedAt.toMillis() >= staleBefore.toMillis()) {
        return {
          status: "already_fresh",
          normalizedKey,
          queued: false
        };
      }
    }

    await db.runTransaction(async (tx) => {
      const currentQueue = await tx.get(queueRef);
      const queueData = currentQueue.exists ? currentQueue.data() || {} : null;
      const status = queueData ? String(queueData.status || "") : "";

      if (status === "queued" || status === "processing") {
        tx.set(
          queueRef,
          {
            requestedAt: admin.firestore.FieldValue.serverTimestamp(),
            requestCount: admin.firestore.FieldValue.increment(1)
          },
          { merge: true }
        );
        return;
      }

      tx.set(
        queueRef,
        {
          normalizedKey,
          artistName,
          songName,
          status: "queued",
          priority: Number.isFinite(data.priority) ? Number(data.priority) : 50,
          retries: 0,
          source: "client",
          appVersion,
          createdAt: admin.firestore.FieldValue.serverTimestamp(),
          updatedAt: admin.firestore.FieldValue.serverTimestamp(),
          requestedAt: admin.firestore.FieldValue.serverTimestamp(),
          requestCount: admin.firestore.FieldValue.increment(1)
        },
        { merge: true }
      );
    });

    logger.info("metadata fetch enqueued", { normalizedKey, artistName, songName, appVersion });

    return {
      status: "queued",
      normalizedKey,
      queued: true
    };
  }
);

exports._normalizeKeyForTests = normalizeKey;

// ============================================================
// Customer Admin: legacy-user venue assignment + support tool
// (EnterpriseAdmin-only; see firebase/functions/adminUsers.js)
// ============================================================
Object.assign(exports, require("./adminUsers")(admin, db));

// ============================================================
// LEGACY FUNCTIONS (Migrated)
// ============================================================

// Middleware for authorization
const isAuthorized = (req, res, next) => {
  const authHeader = req.headers.authorization;
  if (authHeader === 'secretvalue') {
    next();
  } else {
    res.status(401).json({ msg: 'No Access' });
  }
};

// Middleware to refresh Spotify token if needed
const checkForTokenRefresh = async () => {
  if (Date.now() - refreshTime > 3600000 || refreshTime === 0) {
    await getNewSpotifyToken();
  }
};

const getNewSpotifyToken = async () => {
  const clientId = 'ac9878c776b043d39b35c9cda574b4e7';
  const clientSecret = '65043a7175d04b5c8a609bd703fa8ee1';

  spotifyApi = new SpotifyWebApi({ clientId, clientSecret });

  try {
    const response = await spotifyApi.clientCredentialsGrant();
    refreshTime = Date.now() + response.body.expires_in * 1000;
    spotifyApi.setAccessToken(response.body.access_token);
  } catch (err) {
    logger.error('Error retrieving Spotify access token', err);
  }
};

// Expose the Express app as a Cloud Function
exports.app = onRequest({ region: "us-central1" }, app);

// Process audio with AudioShake
exports.processAudio = onCall(
  {
    region: "us-central1",
    enforceAppCheck: false,
    cors: true
  },
  async (data, context) => {
    // Upload file to temporary storage
    const file = data.file;
    const bucket = admin.storage().bucket();
    const tempFilePath = `temp/${Date.now()}_${file.name}`;
    await bucket.file(tempFilePath).save(file.buffer);

    // Get download URL
    const [url] = await bucket.file(tempFilePath).getSignedUrl({
      action: 'read',
      expires: Date.now() + 15 * 60 * 1000, // 15 minutes
    });

    // Call AudioShake API to split audio
    const splitResponse = await axios.post('https://api.audioshake.ai/v1/split', {
      url: url,
      stems: ['vocals', 'accompaniment'],
    }, {
      headers: { 'Authorization': `Bearer ${audioShakeToken}` }
    });

    const jobId = splitResponse.data.job_id;

    // Poll for job completion
    let jobComplete = false;
    while (!jobComplete) {
      await new Promise(resolve => setTimeout(resolve, 5000)); // Wait 5 seconds
      const statusResponse = await axios.get(`https://api.audioshake.ai/v1/job/${jobId}`, {
        headers: { 'Authorization': `Bearer ${audioShakeToken}` }
      });
      jobComplete = statusResponse.data.status === 'complete';
    }

    // Get vocals URL
    const vocalsResponse = await axios.get(`https://api.audioshake.ai/v1/job/${jobId}/stem/vocals`, {
      headers: { 'Authorization': `Bearer ${audioShakeToken}` }
    });
    const vocalsUrl = vocalsResponse.data.url;

    // Call transcription API
    const transcriptionResponse = await axios.post('https://api.audioshake.ai/v1/transcribe', {
      url: vocalsUrl,
    }, {
      headers: { 'Authorization': `Bearer ${audioShakeToken}` }
    });

    const transcriptionJobId = transcriptionResponse.data.job_id;

    // Poll for transcription job completion
    jobComplete = false;
    while (!jobComplete) {
      await new Promise(resolve => setTimeout(resolve, 5000)); // Wait 5 seconds
      const statusResponse = await axios.get(`https://api.audioshake.ai/v1/job/${transcriptionJobId}`, {
        headers: { 'Authorization': `Bearer ${audioShakeToken}` }
      });
      jobComplete = statusResponse.data.status === 'complete';
    }

    // Get transcription URL
    const transcriptionResultResponse = await axios.get(`https://api.audioshake.ai/v1/job/${transcriptionJobId}/result`, {
      headers: { 'Authorization': `Bearer ${audioShakeToken}` }
    });
    const transcriptionUrl = transcriptionResultResponse.data.url;

    // Clean up temporary file
    await bucket.file(tempFilePath).delete();

    return { vocalsUrl, transcriptionUrl };
  }
);

// ============================================================
// EXPRESS APP ENDPOINTS (Legacy)
// ============================================================

// Endpoint to upload a song link to AudioShake
app.post('/upload', async (req, res) => {
  logger.log('Upload Link Call');
  const apiUrl = 'https://groovy.audioshake.ai/upload/link';
  const token = audioShakeToken;

  try {
    const response = await axios.post(apiUrl, req.body, {
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${token}`
      }
    });
    res.json(response.data);
  } catch (error) {
    res.status(error.response?.status || 500).json(error.response?.data || { error: 'Unknown error' });
    logger.error('Error calling AudioShake API:', error);
  }
});

// Endpoint to check job status on AudioShake
app.get('/jobStatus/:jobId', async (req, res) => {
  const jobId = req.params.jobId;
  const apiUrl = `https://groovy.audioshake.ai/job/${jobId}`;
  const token = audioShakeToken;

  try {
    const response = await axios.get(apiUrl, {
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${token}`
      }
    });
    res.status(200).json(response.data);
  } catch (error) {
    res.status(error.response?.status || 500).json(error.response?.data || { error: 'Unknown error' });
  }
});

// Endpoint to create a job on AudioShake
app.post('/createJob', async (req, res) => {
  const apiUrl = 'https://groovy.audioshake.ai/job/';
  const token = audioShakeToken;
  logger.log('Create Job Call. Req = ', req.body);

  try {
    const response = await axios.post(apiUrl, req.body, {
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${token}`
      }
    });
    res.status(200).json(response.data);
  } catch (error) {
    res.status(error.response?.status || 500).json(error.response?.data || { error: 'Unknown error' });
  }
});

// Ping endpoint to refresh Spotify token
app.get('/ping', (req, res) => {
  checkForTokenRefresh().then(() => {
    res.json('Return from PING');
  });
});

// Search artist by name
app.get('/searchArtistName/:artistName', isAuthorized, (req, res) => {
  checkForTokenRefresh().then(() => {
    const artistName = req.params.artistName;
    spotifyApi.searchArtists(artistName, { limit: 1 })
      .then(data => res.json(data.body))
      .catch(err => res.json(err));
  });
});

// Search song by name
app.get('/searchSongName/:songName', isAuthorized, (req, res) => {
  checkForTokenRefresh().then(() => {
    const songName = req.params.songName;
    spotifyApi.searchTracks(songName, { limit: 1 })
      .then(data => res.json(data.body))
      .catch(err => res.json(err));
  });
});

// Get track by ID
app.get('/getTrack/:id', isAuthorized, (req, res) => {
  checkForTokenRefresh().then(() => {
    const trackID = req.params.id;
    spotifyApi.getTrack(trackID)
      .then(data => res.json(data.body))
      .catch(err => res.json(err));
  });
});

// Search both artist and song
app.get('/searchArtistAndSong/:artistName/:songName', isAuthorized, (req, res) => {
  checkForTokenRefresh().then(() => {
    const { artistName, songName } = req.params;
    const search = `track:${songName} artist:${artistName}`;

    spotifyApi.searchTracks(search)
      .then(data => {
        const items = data.body.tracks.items;
        if (items.length > 0) {
          const artistId = items[0].artists[0].id;
          const trackId = items[0].id;
          Promise.all([
            spotifyApi.getArtist(artistId),
            spotifyApi.getTrack(trackId),
            spotifyApi.getAudioFeaturesForTrack(trackId)
          ]).then(([artistData, trackData, featuresData]) => {
            res.json([data.body, artistData.body, trackData.body, featuresData.body]);
          }).catch(err => res.json(err));
        } else {
          res.status(404).json('No results found');
        }
      }).catch(err => res.json(err));
  });
});
