/* eslint-disable no-console */
/*
 * One-off backfill: populate hosts/{uid}.stageNameLower for every existing
 * host doc, needed by adminSearchUsers' prefix query (see
 * firebase/functions/adminUsers.js). Going forward, stageNameLower must be
 * kept in sync wherever stageName is written (LoginFlowController.cpp and
 * any profile-edit UI) -- this script only fixes existing data.
 *
 * Usage: npm run backfill:stageNameLower
 */
const admin = require("firebase-admin");

if (!admin.apps.length) {
  admin.initializeApp();
}

const db = admin.firestore();
const COLLECTION_HOSTS = "hosts";
const BATCH_SIZE = 400;
const PAGE_SIZE = 500;

async function main() {
  let cursor = null;
  let scanned = 0;
  let updated = 0;

  for (;;) {
    let query = db.collection(COLLECTION_HOSTS).orderBy("__name__").limit(PAGE_SIZE);
    if (cursor) query = query.startAfter(cursor);

    const snap = await query.get();
    if (snap.empty) break;

    let batch = db.batch();
    let batchCount = 0;

    for (const doc of snap.docs) {
      scanned += 1;
      const data = doc.data() || {};
      const stageName = String(data.stageName || "");
      const desired = stageName.toLowerCase();

      if (data.stageNameLower !== desired) {
        batch.set(doc.ref, { stageNameLower: desired }, { merge: true });
        batchCount += 1;
        updated += 1;

        if (batchCount >= BATCH_SIZE) {
          await batch.commit();
          batch = db.batch();
          batchCount = 0;
        }
      }
    }

    if (batchCount > 0) {
      await batch.commit();
    }

    console.log(`Scanned ${scanned}, updated ${updated} so far...`);
    cursor = snap.docs[snap.docs.length - 1];

    if (snap.docs.length < PAGE_SIZE) break;
  }

  console.log("Backfill complete.");
  console.log(`Scanned: ${scanned}`);
  console.log(`Updated: ${updated}`);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
