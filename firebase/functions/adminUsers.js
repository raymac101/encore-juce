/* eslint-disable no-console */
/*
 * Customer Admin backend: legacy-user venue assignment + support tool.
 *
 * Every function here is EnterpriseAdmin-only. Firestore rules require a
 * `platformAdmin` custom claim for writes to `hosts` (cross-user) and
 * `user-venue-lookup` (create/delete) -- nothing in this codebase ever sets
 * that claim, so these operations are impossible from the desktop client via
 * plain Firestore REST regardless of in-app role gating. All of them go
 * through the Admin SDK here instead (which bypasses security rules), with
 * the caller's role re-derived server-side from hosts/{auth.uid}.role via
 * requireEnterpriseAdmin() -- never trust the client's own role check.
 *
 * module.exports(admin, db) -- required from index.js as:
 *   Object.assign(exports, require('./adminUsers')(admin, db));
 */

const { onCall, HttpsError } = require("firebase-functions/v2/https");
const { logger } = require("firebase-functions");

const COLLECTION_HOSTS = "hosts";
const COLLECTION_VENUE_LOOKUP = "user-venue-lookup";
const COLLECTION_LEGACY_USERS = "users"; // TAGG mobile app profile data -- read-only, never written here.
const COLLECTION_AUDIT_LOG = "adminAuditLog";

const CALL_OPTS = { region: "us-central1", enforceAppCheck: false, cors: true };

module.exports = function adminUsersModule(admin, db) {
  //============================================================
  // Shared helpers
  //============================================================

  /** Single choke point every callable below must call first. Re-derives
   *  role from Firestore via Admin SDK -- never trusts anything in the
   *  request payload or a client-side check. */
  async function requireEnterpriseAdmin(request) {
    if (!request.auth) {
      throw new HttpsError("unauthenticated", "Sign in required.");
    }
    const uid = request.auth.uid;
    const snap = await db.collection(COLLECTION_HOSTS).doc(uid).get();
    const role = snap.exists ? String(snap.data().role || "") : "";
    if (role !== "EnterpriseAdmin") {
      throw new HttpsError("permission-denied", "EnterpriseAdmin role required.");
    }
    return { uid, email: request.auth.token.email || "" };
  }

  /** `details` must be an explicit allow-listed subset per call site --
   *  never spread an entire request body, and never include a password. */
  async function writeAuditLog(action, admin_, targetUid, targetEmail, success, details) {
    try {
      await db.collection(COLLECTION_AUDIT_LOG).add({
        action,
        adminUid: admin_.uid,
        adminEmail: admin_.email,
        targetUid: targetUid || "",
        targetEmail: targetEmail || "",
        timestamp: admin.firestore.FieldValue.serverTimestamp(),
        success: !!success,
        details: details || {}
      });
    } catch (err) {
      // Audit logging must never block the actual action if Firestore hiccups.
      logger.error("adminAuditLog write failed", { action, targetUid, err: String(err) });
    }
  }

  function hostFromSnap(snap) {
    if (!snap.exists) return null;
    const d = snap.data() || {};
    return {
      userId: snap.id,
      email: String(d.email || ""),
      companyId: String(d.companyId || ""),
      profileId: String(d.profileId || ""),
      avatarUrl: String(d.avatarUrl || ""),
      stageName: String(d.stageName || ""),
      fullName: String(d.fullName || ""),
      birthday: String(d.birthday || ""),
      country: String(d.country || ""),
      city: String(d.city || ""),
      gender: String(d.gender || ""),
      signUpDate: String(d.signUpDate || ""),
      lastLogin: String(d.lastLogin || ""),
      loginCount: Number(d.loginCount || 0),
      role: String(d.role || ""),
      accountStatus: String(d.accountStatus || "active")
    };
  }

  async function activeVenueRowsFor(uid) {
    const snap = await db
      .collection(COLLECTION_VENUE_LOOKUP)
      .where("userId", "==", uid)
      .where("status", "==", "active")
      .get();
    return snap.docs.map((d) => {
      const v = d.data() || {};
      return {
        id: d.id,
        venueId: String(v.venueId || ""),
        venueName: String(v.venueName || ""),
        venueCity: String(v.venueCity || ""),
        role: String(v.role || "")
      };
    });
  }

  function venueSummaryFromSnap(d) {
    const v = d.data() || {};
    return {
      id: d.id,
      name: String(v.name || ""),
      address: String(v.address || ""),
      city: String(v.city || ""),
      country: String(v.country || ""),
      code: String(v.code || ""),
      codePlus: String(v.codePlus || ""),
      adminEmail: String(v.adminEmail || ""),
      numSongs: Number(v.numSongs || 0),
      numSingers: Number(v.numSingers || 0),
      numStrikes: Number(v.numStrikes || 0),
      repeatSongs: !!v.repeatSongs,
      autoapprove: !!v.autoapprove
    };
  }

  //============================================================
  // adminSearchUsers -- email exact match, or stage-name prefix match.
  // Falls back to an Auth-only lookup by email for accounts with no hosts
  // doc at all.
  //============================================================
  const adminSearchUsers = onCall(CALL_OPTS, async (request) => {
    const admin_ = await requireEnterpriseAdmin(request);
    const data = request.data || {};
    const rawQuery = String(data.query || "").trim();
    const pageSize = Math.min(50, Math.max(1, Number(data.pageSize) || 25));
    const cursor = data.cursor || null; // last doc id from the previous page

    if (!rawQuery) {
      throw new HttpsError("invalid-argument", "query is required.");
    }

    const looksLikeEmail = rawQuery.includes("@");
    let results = [];
    let nextCursor = null;

    if (looksLikeEmail) {
      const email = rawQuery.toLowerCase();
      const snap = await db.collection(COLLECTION_HOSTS).where("email", "==", email).limit(pageSize).get();
      results = snap.docs.map((d) => hostFromSnap(d));
    } else {
      const prefix = rawQuery.toLowerCase();
      let q = db
        .collection(COLLECTION_HOSTS)
        .orderBy("stageNameLower")
        .where("stageNameLower", ">=", prefix)
        .where("stageNameLower", "<", prefix + "")
        .limit(pageSize);
      if (cursor) {
        const cursorSnap = await db.collection(COLLECTION_HOSTS).doc(cursor).get();
        if (cursorSnap.exists) q = q.startAfter(cursorSnap);
      }
      const snap = await q.get();
      results = snap.docs.map((d) => hostFromSnap(d));
      if (snap.docs.length === pageSize) {
        nextCursor = snap.docs[snap.docs.length - 1].id;
      }
    }

    // Auth-only fallback: only when nothing matched in hosts and the query
    // was a full email, so a pure orphan account is still findable/actionable.
    if (results.length === 0 && looksLikeEmail) {
      try {
        const userRecord = await admin.auth().getUserByEmail(rawQuery.toLowerCase());
        results = [
          {
            userId: userRecord.uid,
            email: userRecord.email || "",
            stageName: "",
            fullName: userRecord.displayName || "",
            country: "",
            city: "",
            signUpDate: "",
            lastLogin: "",
            loginCount: 0,
            role: "",
            accountStatus: userRecord.disabled ? "deactivated" : "active",
            authOnly: true
          }
        ];
      } catch (err) {
        // No matching Auth user either -- genuinely not found, not an error.
      }
    }

    return { results, nextCursor };
  });

  //============================================================
  // adminListUnassignedHosts -- hosts with no active user-venue-lookup row.
  // Firestore can't left-join, so this pages hosts and cross-references
  // user-venue-lookup in small `in` queries per page rather than scanning
  // either collection in full.
  //============================================================
  const adminListUnassignedHosts = onCall(CALL_OPTS, async (request) => {
    await requireEnterpriseAdmin(request);
    const data = request.data || {};
    const pageSize = Math.min(200, Math.max(1, Number(data.pageSize) || 200));
    const cursor = data.cursor || null;

    let hostsQuery = db.collection(COLLECTION_HOSTS).orderBy("email").limit(pageSize);
    if (cursor) {
      const cursorSnap = await db.collection(COLLECTION_HOSTS).doc(cursor).get();
      if (cursorSnap.exists) hostsQuery = hostsQuery.startAfter(cursorSnap);
    }

    const hostsSnap = await hostsQuery.get();
    const hostDocs = hostsSnap.docs;
    const nextCursor = hostDocs.length === pageSize ? hostDocs[hostDocs.length - 1].id : null;

    const activeUids = new Set();
    const CHUNK = 30; // Firestore `in` operator cap.
    for (let i = 0; i < hostDocs.length; i += CHUNK) {
      const chunkIds = hostDocs.slice(i, i + CHUNK).map((d) => d.id);
      if (chunkIds.length === 0) continue;
      const lookupSnap = await db
        .collection(COLLECTION_VENUE_LOOKUP)
        .where("userId", "in", chunkIds)
        .where("status", "==", "active")
        .get();
      lookupSnap.docs.forEach((d) => activeUids.add(String((d.data() || {}).userId || "")));
    }

    const unassigned = hostDocs
      .filter((d) => !activeUids.has(d.id))
      .map((d) => hostFromSnap(d));

    return { results: unassigned, nextCursor };
  });

  //============================================================
  // adminListVenues -- every venue, for the "assign venue + role" picker.
  // Deliberately NOT VenueService::getVenues() (plain Firestore REST from
  // the client): firestore.rules only lets an account read a venue it's
  // already an active member of (no platformAdmin claim exists anywhere),
  // so that call would silently return only the admin's OWN venues --
  // wrong for cross-venue assignment. This goes through the Admin SDK
  // instead, same as everything else in this module. Venues are a much
  // smaller collection than hosts, so a single capped list (no pagination)
  // is enough; the client filters as the admin types.
  //============================================================
  const adminListVenues = onCall(CALL_OPTS, async (request) => {
    await requireEnterpriseAdmin(request);

    const snap = await db.collection("venues").limit(1000).get();
    const results = snap.docs.map((d) => venueSummaryFromSnap(d));

    return { results };
  });

  //============================================================
  // adminDeleteVenue -- irreversible. Mirrors VenueService::deleteVenue()'s
  // subcollection cleanup (Source/Services/VenueService.cpp) exactly, plus
  // also removes any user-venue-lookup rows for this venue (the original
  // client-side deleteVenue() doesn't do this, but leaving those rows
  // behind would orphan them against a venue that no longer exists --
  // and would incorrectly keep affected hosts out of the "unassigned"
  // backlog, since they'd still look actively assigned). Goes through the
  // Admin SDK for the same reason adminListVenues does: an EnterpriseAdmin
  // account isn't necessarily an active member of every venue, and
  // firestore.rules' isLegacyVenueAdmin(venueId) check would otherwise
  // block deleting a venue the admin doesn't personally belong to.
  //============================================================
  const VENUE_SUBCOLLECTION_PATHS = [
    "queue", "requested", "playing", "emojis",
    "playlists/venueLists/new",
    "playlists/venueLists/Popular",
    "playlists/venueLists/Recommended"
  ];

  async function deleteCollectionByPath(path) {
    const snap = await db.collection(path).limit(500).get();
    if (snap.empty) return 0;
    const batch = db.batch();
    snap.docs.forEach((d) => batch.delete(d.ref));
    await batch.commit();
    // Recurse in case there were more than 500 (matches the batched-delete
    // pattern already used elsewhere in this file, e.g. adminHardDeleteUser).
    return snap.docs.length + (snap.docs.length === 500 ? await deleteCollectionByPath(path) : 0);
  }

  const adminDeleteVenue = onCall(CALL_OPTS, async (request) => {
    const admin_ = await requireEnterpriseAdmin(request);
    const data = request.data || {};
    const venueId = String(data.venueId || "").trim();
    const confirmName = String(data.confirmName || "").trim().toLowerCase();

    if (!venueId || !confirmName) {
      throw new HttpsError("invalid-argument", "venueId and confirmName are required.");
    }

    const venueRef = db.collection("venues").doc(venueId);
    const venueSnap = await venueRef.get();
    if (!venueSnap.exists) {
      throw new HttpsError("not-found", "Venue not found.");
    }

    const realName = String((venueSnap.data() || {}).name || "").trim().toLowerCase();
    if (!realName || realName !== confirmName) {
      throw new HttpsError("failed-precondition", "confirmName does not match this venue's name.");
    }

    for (const sub of VENUE_SUBCOLLECTION_PATHS) {
      await deleteCollectionByPath(`venues/${venueId}/${sub}`);
    }

    const lookupSnap = await db.collection(COLLECTION_VENUE_LOOKUP).where("venueId", "==", venueId).get();
    const lookupBatch = db.batch();
    lookupSnap.docs.forEach((d) => lookupBatch.delete(d.ref));
    if (lookupSnap.docs.length > 0) await lookupBatch.commit();

    await venueRef.delete();

    await writeAuditLog("delete_venue", admin_, "", "", true, {
      venueId,
      venueName: realName,
      deletedLookupRows: lookupSnap.docs.length
    });

    return { ok: true };
  });

  //============================================================
  // adminGetUserProfile -- combined read for the detail panel. The ONLY
  // function allowed to read the legacy `users` (TAGG) collection, and only
  // ever for display -- never written here.
  //============================================================
  const adminGetUserProfile = onCall(CALL_OPTS, async (request) => {
    await requireEnterpriseAdmin(request);
    const uid = String((request.data || {}).uid || "").trim();
    if (!uid) throw new HttpsError("invalid-argument", "uid is required.");

    const hostSnap = await db.collection(COLLECTION_HOSTS).doc(uid).get();
    const legacyUserSnap = await db.collection(COLLECTION_LEGACY_USERS).doc(uid).get();
    const venues = await activeVenueRowsFor(uid);

    let authRecord = null;
    try {
      const u = await admin.auth().getUser(uid);
      authRecord = {
        disabled: !!u.disabled,
        providers: (u.providerData || []).map((p) => p.providerId),
        lastSignInTime: (u.metadata && u.metadata.lastSignInTime) || "",
        creationTime: (u.metadata && u.metadata.creationTime) || ""
      };
    } catch (err) {
      authRecord = null; // No Auth account (shouldn't normally happen for a real hosts doc).
    }

    return {
      host: hostFromSnap(hostSnap),
      legacyProfile: legacyUserSnap.exists ? legacyUserSnap.data() : null,
      venues,
      auth: authRecord
    };
  });

  //============================================================
  // adminAssignVenueRole -- upserts user-venue-lookup/{uid}_{venueId} with
  // the exact field contract LoginFlowController.cpp's queryAssociations()
  // reads. A mismatch here means the user's next login silently falls back
  // to AwaitInvitation instead of loading their venue -- verified manually
  // per the plan's Verification section, not just by code review.
  //============================================================
  const adminAssignVenueRole = onCall(CALL_OPTS, async (request) => {
    const admin_ = await requireEnterpriseAdmin(request);
    const data = request.data || {};
    const uid = String(data.uid || "").trim();
    const venueId = String(data.venueId || "").trim();
    const role = String(data.role || "").trim();
    const venueName = String(data.venueName || "").trim();
    const venueCity = String(data.venueCity || "").trim();
    const userEmail = String(data.userEmail || "").trim();

    if (!uid || !venueId || !role) {
      throw new HttpsError("invalid-argument", "uid, venueId, and role are required.");
    }

    const docId = `${uid}_${venueId}`;
    const now = admin.firestore.FieldValue.serverTimestamp();

    await db
      .collection(COLLECTION_VENUE_LOOKUP)
      .doc(docId)
      .set(
        {
          userId: uid,
          venueId,
          userEmail,
          role,
          status: "active",
          joinedDate: now,
          lastActive: now,
          venueName,
          venueCity
        },
        { merge: true }
      );

    await writeAuditLog("assign_venue_role", admin_, uid, userEmail, true, { venueId, role });

    return { ok: true };
  });

  //============================================================
  // adminSetUserPassword -- direct admin-side password set. Never logged.
  //============================================================
  const adminSetUserPassword = onCall(CALL_OPTS, async (request) => {
    const admin_ = await requireEnterpriseAdmin(request);
    const data = request.data || {};
    const uid = String(data.uid || "").trim();
    const newPassword = String(data.newPassword || "");

    if (!uid || newPassword.length < 6) {
      throw new HttpsError("invalid-argument", "uid and a newPassword of at least 6 characters are required.");
    }

    let targetEmail = "";
    try {
      const u = await admin.auth().getUser(uid);
      targetEmail = u.email || "";
    } catch (err) {
      // proceed without email context
    }

    await admin.auth().updateUser(uid, { password: newPassword });
    // `details` deliberately never includes newPassword, not even hashed.
    await writeAuditLog("set_password", admin_, uid, targetEmail, true, {});

    return { ok: true };
  });

  //============================================================
  // adminDeactivateUser / adminReactivateUser -- reversible removal.
  //============================================================
  const adminDeactivateUser = onCall(CALL_OPTS, async (request) => {
    const admin_ = await requireEnterpriseAdmin(request);
    const uid = String((request.data || {}).uid || "").trim();
    if (!uid) throw new HttpsError("invalid-argument", "uid is required.");

    await admin.auth().updateUser(uid, { disabled: true });
    await db.collection(COLLECTION_HOSTS).doc(uid).set({ accountStatus: "deactivated" }, { merge: true });
    await writeAuditLog("deactivate_user", admin_, uid, "", true, {});

    return { ok: true };
  });

  const adminReactivateUser = onCall(CALL_OPTS, async (request) => {
    const admin_ = await requireEnterpriseAdmin(request);
    const uid = String((request.data || {}).uid || "").trim();
    if (!uid) throw new HttpsError("invalid-argument", "uid is required.");

    await admin.auth().updateUser(uid, { disabled: false });
    await db.collection(COLLECTION_HOSTS).doc(uid).set({ accountStatus: "active" }, { merge: true });
    await writeAuditLog("reactivate_user", admin_, uid, "", true, {});

    return { ok: true };
  });

  //============================================================
  // adminHardDeleteUser -- irreversible. Server-side re-validates
  // confirmEmail against the real target email; never trusts that the
  // client-side confirmation UI actually ran. Leaves venue audit/history
  // records intact -- deleting a customer shouldn't corrupt a venue's
  // historical reporting.
  //============================================================
  const adminHardDeleteUser = onCall(CALL_OPTS, async (request) => {
    const admin_ = await requireEnterpriseAdmin(request);
    const data = request.data || {};
    const uid = String(data.uid || "").trim();
    const confirmEmail = String(data.confirmEmail || "").trim().toLowerCase();

    if (!uid || !confirmEmail) {
      throw new HttpsError("invalid-argument", "uid and confirmEmail are required.");
    }

    const hostSnap = await db.collection(COLLECTION_HOSTS).doc(uid).get();
    let realEmail = hostSnap.exists ? String((hostSnap.data() || {}).email || "").toLowerCase() : "";

    if (!realEmail) {
      try {
        const u = await admin.auth().getUser(uid);
        realEmail = String(u.email || "").toLowerCase();
      } catch (err) {
        // no Auth record either
      }
    }

    if (!realEmail || realEmail !== confirmEmail) {
      throw new HttpsError("failed-precondition", "confirmEmail does not match this account's email.");
    }

    try {
      await admin.auth().deleteUser(uid);
    } catch (err) {
      // Tolerate an already-missing Auth account so a retry after a partial
      // failure can still clean up Firestore.
      if (err.code !== "auth/user-not-found") throw err;
    }

    await db.collection(COLLECTION_HOSTS).doc(uid).delete();

    const lookupSnap = await db.collection(COLLECTION_VENUE_LOOKUP).where("userId", "==", uid).get();
    const batch = db.batch();
    lookupSnap.docs.forEach((d) => batch.delete(d.ref));
    if (lookupSnap.docs.length > 0) await batch.commit();

    await writeAuditLog("hard_delete_user", admin_, uid, realEmail, true, {
      deletedVenueRows: lookupSnap.docs.length
    });

    return { ok: true };
  });

  return {
    adminSearchUsers,
    adminListUnassignedHosts,
    adminListVenues,
    adminDeleteVenue,
    adminGetUserProfile,
    adminAssignVenueRole,
    adminSetUserPassword,
    adminDeactivateUser,
    adminReactivateUser,
    adminHardDeleteUser
  };
};
