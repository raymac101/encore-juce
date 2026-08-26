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

// Extracts a clean {status, message} out of a spotify-web-api-node error
// object so callers can res.status(...).json(...) a real error instead of
// silently res.json(err)'ing it with an implicit 200 -- which is what made
// every Spotify-side failure look like a successful (but empty) response to
// the JUCE client.
function spotifyErrorDetails(err) {
  const status =
    (err && err.statusCode) ||
    (err && err.body && err.body.error && err.body.error.status) ||
    502;
  const message =
    (err && err.body && err.body.error && err.body.error.message) ||
    (err && err.message) ||
    'Unknown Spotify API error';
  return { status: (status >= 400 && status < 600) ? status : 502, message };
}

function nowMinusDays(days) {
  const d = new Date();
  d.setUTCDate(d.getUTCDate() - days);
  return d;
}

// Mirrors ApiService::getKeySignature (Source/Services/ApiService.cpp) --
// Spotify's integer pitch class (0-11) + mode (1=major, 0=minor).
function keySignatureFromSpotify(key, mode) {
  const letters = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];
  const k = Number(key);
  if (!Number.isFinite(k) || k < 0 || k > 11) return "";
  let label = letters[k];
  if (String(mode) === "0") label += " minor";
  return label;
}

// Reduces the raw Spotify search/artist/track/audioFeatures payloads down to
// the canonical metadataSongs field set (METADATA_MIGRATION_DESIGN.md).
// Mirrors the same fallback order ApiService::doSpotifyApiCall uses client-
// side (search-result track first, then the directly-fetched track/artist).
function buildCanonicalMetadata(artistName, songName, searchResult, artistData, trackData, featuresData) {
  const items = (searchResult && searchResult.tracks && searchResult.tracks.items) || [];
  const firstTrack = items.length > 0 ? items[0] : null;

  let imageUrl = "";
  const trackImages = (firstTrack && firstTrack.album && firstTrack.album.images) || [];
  if (trackImages.length > 0) imageUrl = trackImages[0].url || "";
  if (!imageUrl) {
    const artistImages = (artistData && artistData.images) || [];
    if (artistImages.length > 0) imageUrl = artistImages[0].url || "";
  }

  const durationMS = (trackData && trackData.duration_ms) || (firstTrack && firstTrack.duration_ms) || 0;

  const resolvedSongName = (trackData && trackData.name) || (firstTrack && firstTrack.name) || songName;

  const trackArtists = (trackData && trackData.artists) || [];
  const resolvedArtistName =
    (trackArtists.length > 0 && trackArtists[0].name) || (artistData && artistData.name) || artistName;

  const releaseDate =
    (trackData && trackData.album && trackData.album.release_date) ||
    (firstTrack && firstTrack.album && firstTrack.album.release_date) ||
    "";

  const genres = (artistData && artistData.genres) || [];

  let tempo = 0;
  let keySignature = "";
  if (featuresData) {
    if (featuresData.tempo) tempo = Math.round(featuresData.tempo);
    if (featuresData.key !== undefined && featuresData.key !== null) {
      keySignature = keySignatureFromSpotify(featuresData.key, featuresData.mode);
    }
  }

  return {
    artistName: resolvedArtistName,
    songName: resolvedSongName,
    imageUrl,
    durationMS,
    tempo,
    keySignature,
    releaseDate,
    genres
  };
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
// Shared daily Spotify-call quota. Every caller that can trigger a real
// Spotify lookup through /searchArtistAndSong shares ONE quota, since they
// all ride on the same Spotify app credentials: the TAGG-request auto-fetch
// (MainComponent::enrichSongMetadataIfMissing), the manual "Get Metadata"
// button, and the Viracicom Admin bulk metadata tool. A per-machine counter
// wouldn't protect the real shared limit against any of the others.
// ============================================================
const COLLECTION_QUOTA = "metadataQuota";
const QUOTA_DOC_ID = "daily";
const DAILY_QUOTA_CAP = 1000;

function nextMidnightUtc(from) {
  return new Date(Date.UTC(from.getUTCFullYear(), from.getUTCMonth(), from.getUTCDate() + 1, 0, 0, 0, 0));
}

// Read-only view of the current quota window, resetting the reported values
// (but NOT writing them) if the window has rolled over. Safe to call as
// often as needed -- e.g. from getMetadataQuotaStatus.
async function getQuotaStatus() {
  const snap = await db.collection(COLLECTION_QUOTA).doc(QUOTA_DOC_ID).get();
  const now = new Date();

  if (snap.exists) {
    const data = snap.data() || {};
    const resetAt = data.resetAt && data.resetAt.toDate ? data.resetAt.toDate() : null;
    if (resetAt && now < resetAt) {
      return {
        usedCalls: Number.isFinite(data.usedCalls) ? data.usedCalls : 0,
        cap: Number.isFinite(data.cap) ? data.cap : DAILY_QUOTA_CAP,
        resetAt
      };
    }
  }

  return { usedCalls: 0, cap: DAILY_QUOTA_CAP, resetAt: nextMidnightUtc(now) };
}

// Atomically checks + increments the shared daily quota. Must be called
// (and must return allowed=true) BEFORE issuing any real Spotify API call.
async function tryConsumeQuota() {
  const ref = db.collection(COLLECTION_QUOTA).doc(QUOTA_DOC_ID);

  return db.runTransaction(async (tx) => {
    const snap = await tx.get(ref);
    const now = new Date();
    const data = snap.exists ? snap.data() || {} : {};
    const resetAt = data.resetAt && data.resetAt.toDate ? data.resetAt.toDate() : null;

    let usedCalls = Number.isFinite(data.usedCalls) ? data.usedCalls : 0;
    let cap = Number.isFinite(data.cap) ? data.cap : DAILY_QUOTA_CAP;
    let effectiveResetAt = resetAt;

    if (!effectiveResetAt || now >= effectiveResetAt) {
      usedCalls = 0;
      cap = DAILY_QUOTA_CAP;
      effectiveResetAt = nextMidnightUtc(now);
    }

    const allowed = usedCalls < cap;
    if (allowed) {
      usedCalls += 1;
      tx.set(ref, {
        usedCalls,
        cap,
        resetAt: admin.firestore.Timestamp.fromDate(effectiveResetAt),
        updatedAt: admin.firestore.FieldValue.serverTimestamp()
      }, { merge: true });
    }

    return { allowed, usedCalls, cap, remaining: Math.max(0, cap - usedCalls), resetAt: effectiveResetAt };
  });
}

exports.getMetadataQuotaStatus = onCall(
  { region: "us-central1", enforceAppCheck: false, cors: true },
  async (request) => {
    if (!request.auth) {
      throw new HttpsError("unauthenticated", "Sign-in required.");
    }
    const status = await getQuotaStatus();
    return {
      usedCalls: status.usedCalls,
      cap: status.cap,
      remaining: Math.max(0, status.cap - status.usedCalls),
      resetAt: status.resetAt.toISOString()
    };
  }
);

// ============================================================
// Customer Admin: legacy-user venue assignment + support tool
// (EnterpriseAdmin-only; see firebase/functions/adminUsers.js)
// ============================================================
Object.assign(exports, require("./adminUsers")(admin, db));

// ============================================================
// Venue membership: Settings > Invite + invitation auto-claim
// (per-venue admin, not EnterpriseAdmin; see firebase/functions/venueMembers.js)
// ============================================================
Object.assign(exports, require("./venueMembers")(admin, db));

// ============================================================
// Audit System V2: canonical idempotent performance events
// ============================================================
Object.assign(exports, require("./performanceEvents")(admin, db));

// ============================================================
// LEGACY FUNCTIONS (Migrated)
// ============================================================

// Middleware for authorization. Every real caller (ApiService.h/.cpp's
// bearerToken_, "KaraokeWorld") sends "Authorization: Bearer KaraokeWorld" --
// this previously checked for the literal string 'secretvalue', which no
// client has ever actually sent, so every route below 401'd for everyone.
// It went unnoticed because ApiService::searchArtistAndSong tries the local
// cache and Firestore metadataSongs FIRST and only falls through to these
// legacy routes on a genuine miss; see ApiService.cpp's tryCachedLookup/
// tryFirestoreLookup for the matching fix (they no longer report a false
// "ok" on an incomplete cached/Firestore entry, which is what was masking
// this for so long).
const isAuthorized = (req, res, next) => {
  const authHeader = req.headers.authorization;
  if (authHeader === 'Bearer KaraokeWorld') {
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
//
// Response shape is { data: { searchResult, artist, track, audioFeatures } } --
// this MUST stay in sync with ApiService::doSpotifyApiCall (Source/Services/
// ApiService.cpp), which parses exactly these four named keys out of `data`.
// A successful lookup is also upserted into metadataSongs so every venue
// benefits from one client's Spotify hit instead of re-querying Spotify for
// the same song (see METADATA_MIGRATION_DESIGN.md).
app.get('/searchArtistAndSong/:artistName/:songName', isAuthorized, async (req, res) => {
  // Gate BEFORE spending a real Spotify call -- shared across every caller
  // (see the quota section above for why this can't be a per-machine count).
  const quota = await tryConsumeQuota();
  if (!quota.allowed) {
    res.status(429).json({
      error: 'Daily Spotify metadata quota exceeded',
      usedCalls: quota.usedCalls,
      cap: quota.cap,
      resetAt: quota.resetAt.toISOString()
    });
    return;
  }

  checkForTokenRefresh().then(() => {
    const { artistName, songName } = req.params;
    const search = `track:${songName} artist:${artistName}`;

    spotifyApi.searchTracks(search)
      .then(data => {
        const items = data.body.tracks.items;
        if (items.length > 0) {
          const artistId = items[0].artists[0].id;
          const trackId = items[0].id;
          // getArtist/getTrack are required -- if either fails, the whole
          // request should fail. getAudioFeaturesForTrack is best-effort
          // only: Spotify deprecated Audio Features for apps without
          // Extended Quota Mode (Nov 2024), so it 403s here permanently.
          // Falling back to null instead of rejecting the whole Promise.all
          // means artist/track/image/duration/release-date/genres still
          // come through even though tempo/key can no longer be sourced.
          Promise.all([
            spotifyApi.getArtist(artistId),
            spotifyApi.getTrack(trackId),
            spotifyApi.getAudioFeaturesForTrack(trackId).catch(featErr => {
              logger.warn('searchArtistAndSong: audio-features unavailable', {
                artistName, songName, status: spotifyErrorDetails(featErr).status
              });
              return null;
            })
          ]).then(async ([artistData, trackData, featuresResult]) => {
            const audioFeaturesBody = featuresResult ? featuresResult.body : null;
            const responsePayload = {
              data: {
                searchResult: data.body,
                artist: artistData.body,
                track: trackData.body,
                audioFeatures: audioFeaturesBody
              }
            };

            const normalizedKey = normalizeKey(artistName, songName);
            const canonical = buildCanonicalMetadata(
              artistName, songName, data.body, artistData.body, trackData.body, audioFeaturesBody);

            try {
              await db.collection(COLLECTION_METADATA).doc(normalizedKey).set({
                ...canonical,
                normalizedKey,
                source: "legacyApi",
                updatedAt: admin.firestore.FieldValue.serverTimestamp(),
                fetchedAt: admin.firestore.FieldValue.serverTimestamp()
              }, { merge: true });
            } catch (err) {
              // Never fail the client's request over a caching side-effect.
              logger.error("metadataSongs upsert failed", { normalizedKey, error: err.message });
            }

            res.json(responsePayload);
          }).catch(err => {
            const { status, message } = spotifyErrorDetails(err);
            logger.error('searchArtistAndSong: artist/track/audioFeatures lookup failed', {
              artistName, songName, status, message
            });
            res.status(status).json({ error: message });
          });
        } else {
          res.status(404).json({ error: 'No results found' });
        }
      }).catch(err => {
        const { status, message } = spotifyErrorDetails(err);
        logger.error('searchArtistAndSong: track search failed', { artistName, songName, status, message });
        res.status(status).json({ error: message });
      });
  });
});

// A client's own local, offline audio analysis (KeyBpmAnalyzer for
// tempo/key, real per-file duration measurement) is at least as trustworthy
// as anything Spotify could supply for these three fields going forward --
// Spotify deprecated Audio Features entirely (Nov 2024), so tempo/key can
// never come from a live Spotify lookup again, and a locally-measured
// duration reflects the actual karaoke file being played rather than the
// commercial track length. Lets one venue's real, already-done analysis
// save every other venue using the same karaoke vendor's pressing from
// redoing it. No Spotify call, no quota consumption.
app.post('/submitLocalAnalysis', isAuthorized, async (req, res) => {
  const artistName = String((req.body && req.body.artistName) || "").trim();
  const songName = String((req.body && req.body.songName) || "").trim();
  const tempo = Number((req.body && req.body.tempo) || 0);
  const keySignature = String((req.body && req.body.keySignature) || "").trim();
  const durationMS = Number((req.body && req.body.durationMS) || 0);

  if (!artistName || !songName) {
    res.status(400).json({ error: "artistName and songName are required." });
    return;
  }
  if (tempo <= 0 && !keySignature && durationMS <= 0) {
    res.status(400).json({ error: "At least one of tempo, keySignature, durationMS is required." });
    return;
  }

  const normalizedKey = normalizeKey(artistName, songName);
  if (!normalizedKey || normalizedKey === "|") {
    res.status(400).json({ error: "Could not normalize artist/song." });
    return;
  }

  const update = {
    artistName,
    songName,
    normalizedKey,
    source: "localAnalysis",
    updatedAt: admin.firestore.FieldValue.serverTimestamp()
  };
  if (tempo > 0) update.tempo = tempo;
  if (keySignature) update.keySignature = keySignature;
  if (durationMS > 0) update.durationMS = durationMS;

  try {
    await db.collection(COLLECTION_METADATA).doc(normalizedKey).set(update, { merge: true });
    res.json({ ok: true, normalizedKey });
  } catch (err) {
    logger.error("submitLocalAnalysis upsert failed", { normalizedKey, error: err.message });
    res.status(500).json({ error: "Failed to save." });
  }
});
