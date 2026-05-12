/* eslint-disable no-console */
const fs = require("fs");
const path = require("path");
const crypto = require("crypto");
const admin = require("firebase-admin");

if (!admin.apps.length) {
  admin.initializeApp();
}

const db = admin.firestore();

const COLLECTION_METADATA = "metadataSongs";
const BATCH_SIZE = 400;

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

function hashPayload(payload) {
  return crypto.createHash("sha256").update(JSON.stringify(payload)).digest("hex");
}

function parseArgs() {
  const inputArg = process.argv[2];
  if (!inputArg) {
    throw new Error("Usage: npm run import:metadata -- <path-to-meta_data.json>");
  }
  return path.resolve(process.cwd(), inputArg);
}

async function commitBatch(writes) {
  if (!writes.length) return;
  const batch = db.batch();
  for (const w of writes) {
    const ref = db.collection(COLLECTION_METADATA).doc(w.normalizedKey);
    batch.set(ref, w.data, { merge: true });
  }
  await batch.commit();
}

async function main() {
  const filePath = parseArgs();
  const raw = fs.readFileSync(filePath, "utf8");
  const parsed = JSON.parse(raw);

  if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
    throw new Error("Expected a JSON object keyed by metadata id.");
  }

  const writes = [];
  let skipped = 0;

  for (const value of Object.values(parsed)) {
    if (!value || typeof value !== "object") {
      skipped += 1;
      continue;
    }

    const artistName = String(value.artistName || "").trim();
    const songName = String(value.songName || "").trim();
    if (!artistName || !songName) {
      skipped += 1;
      continue;
    }

    const normalizedKey = normalizeKey(artistName, songName);
    if (!normalizedKey || normalizedKey === "|") {
      skipped += 1;
      continue;
    }

    const data = {
      normalizedKey,
      artistName,
      songName,
      imageUrl: String(value.imageUrl || ""),
      durationMS: Number(value.durationMS || 0),
      tempo: Number(value.tempo || 0),
      keySignature: String(value.keySignature || ""),
      releaseDate: String(value.releaseDate || ""),
      genres: Array.isArray(value.genres) ? value.genres.map((g) => String(g)) : [],
      source: "bootstrap-import",
      confidence: 0.9,
      payloadHash: hashPayload(value),
      fetchedAt: admin.firestore.FieldValue.serverTimestamp(),
      updatedAt: admin.firestore.FieldValue.serverTimestamp(),
      popularityScore: 0
    };

    writes.push({ normalizedKey, data });
  }

  let committed = 0;
  for (let i = 0; i < writes.length; i += BATCH_SIZE) {
    const chunk = writes.slice(i, i + BATCH_SIZE);
    await commitBatch(chunk);
    committed += chunk.length;
    console.log(`Committed ${committed}/${writes.length}`);
  }

  console.log("Import complete");
  console.log(`Imported: ${committed}`);
  console.log(`Skipped: ${skipped}`);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
