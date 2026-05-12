const { onCall, HttpsError } = require("firebase-functions/v2/https");
const { logger } = require("firebase-functions");
const admin = require("firebase-admin");

if (!admin.apps.length) {
  admin.initializeApp();
}

const db = admin.firestore();

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
