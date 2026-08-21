/* eslint-disable no-console */
/*
 * Venue membership: an admin adds a user to their venue directly (Settings
 * > Invite). If that email already has an Encore account, the venue shows
 * up in their list the moment they next sign in -- no separate "accept"
 * step. If it doesn't have an account yet, a pending invitation is left
 * behind for acceptVenueInvitation() to claim automatically the first
 * time they DO sign up/in with that email. Either way, an email goes out
 * via the Firebase "Trigger Email" extension (writes to the `mail`
 * collection it watches -- the extension itself, and the SMTP relay it
 * sends through, must be installed/configured in the Firebase console;
 * nothing here can provision that).
 *
 * Unlike adminUsers.js (EnterpriseAdmin-only, platform-wide), the caller
 * check here mirrors firestore.rules' isLegacyVenueAdmin(venueId): any
 * active Host/Admin/Tester/EnterpriseAdmin of the SPECIFIC target venue --
 * re-derived server-side via the Admin SDK, same principle as
 * requireEnterpriseAdmin() in adminUsers.js: never trust the client's own
 * role check.
 *
 * Both writes to user-venue-lookup go through here rather than straight
 * Firestore REST from the client because firestore.rules restricts
 * create/delete on that collection to a platformAdmin custom claim that
 * nothing in this codebase ever sets (see adminUsers.js's own comment) --
 * a direct client write there silently fails.
 *
 * module.exports(admin, db) -- required from index.js as:
 *   Object.assign(exports, require('./venueMembers')(admin, db));
 */

const { onCall, HttpsError } = require("firebase-functions/v2/https");
const { logger } = require("firebase-functions");

const COLLECTION_VENUE_LOOKUP = "user-venue-lookup";
const COLLECTION_VENUES = "venues";
const COLLECTION_INVITATIONS = "venueInvitations";
const COLLECTION_MAIL = "mail"; // Firebase "Trigger Email" extension's default watched collection.
const ADMIN_ROLES = ["Host", "Admin", "Tester", "EnterpriseAdmin"];

const CALL_OPTS = { region: "us-central1", enforceAppCheck: false, cors: true };

module.exports = function venueMembersModule(admin, db) {
  //============================================================
  // Shared helpers
  //============================================================

  /** Mirrors firestore.rules' isLegacyVenueAdmin(venueId), re-derived
   *  server-side rather than trusted from the client. */
  async function requireVenueAdmin(request, venueId) {
    if (!request.auth) {
      throw new HttpsError("unauthenticated", "Sign in required.");
    }
    const uid = request.auth.uid;
    const snap = await db.collection(COLLECTION_VENUE_LOOKUP).doc(`${uid}_${venueId}`).get();
    const row = snap.exists ? snap.data() || {} : {};
    if (row.status !== "active" || !ADMIN_ROLES.includes(String(row.role || ""))) {
      throw new HttpsError("permission-denied", "You must be an admin of this venue.");
    }
    return { uid, email: request.auth.token.email || "" };
  }

  /** Never let a mail failure roll back the actual membership change --
   *  this is a best-effort notification, not the source of truth. */
  async function sendMail(to, subject, text, html) {
    try {
      await db.collection(COLLECTION_MAIL).add({
        to: [to],
        message: { subject, text, html }
      });
    } catch (err) {
      logger.error("venueMembers: mail enqueue failed", { to, subject, err: String(err) });
    }
  }

  async function upsertActiveMembership(uid, venueId, venueName, role, userEmail) {
    const now = admin.firestore.FieldValue.serverTimestamp();
    await db
      .collection(COLLECTION_VENUE_LOOKUP)
      .doc(`${uid}_${venueId}`)
      .set(
        {
          userId: uid,
          venueId,
          venueName,
          role,
          userEmail,
          status: "active",
          joinedDate: now,
          lastActive: now
        },
        { merge: true }
      );
  }

  //============================================================
  // addVenueMember -- Settings > Invite's actual write path.
  //============================================================
  const addVenueMember = onCall(CALL_OPTS, async (request) => {
    const data = request.data || {};
    const venueId = String(data.venueId || "").trim();
    const email = String(data.email || "").trim().toLowerCase();
    const role = String(data.role || "").trim();

    if (!venueId || !email || !role) {
      throw new HttpsError("invalid-argument", "venueId, email, and role are required.");
    }

    const caller = await requireVenueAdmin(request, venueId);
    const callerLabel = caller.email || "A venue admin";

    const venueSnap = await db.collection(COLLECTION_VENUES).doc(venueId).get();
    const venueName = venueSnap.exists ? String((venueSnap.data() || {}).name || "") : "";
    const venueLabel = venueName || "their venue";

    let existingUid = null;
    try {
      const record = await admin.auth().getUserByEmail(email);
      existingUid = record.uid;
    } catch (err) {
      existingUid = null; // No account yet -- expected, not an error.
    }

    if (existingUid) {
      await upsertActiveMembership(existingUid, venueId, venueName, role, email);
      await sendMail(
        email,
        `You've been added to ${venueLabel} on Encore`,
        `${callerLabel} added you to ${venueLabel} on Encore as ${role}. Open Encore and sign in to see it in your venue list.`,
        `<p><strong>${callerLabel}</strong> added you to <strong>${venueLabel}</strong> on Encore as <strong>${role}</strong>.</p>` +
          `<p>Open Encore and sign in to see it in your venue list.</p>`
      );
      return { ok: true, activated: true };
    }

    // No account yet -- leave a pending invitation for
    // acceptVenueInvitation() to auto-claim once they do sign up/in.
    const existingInvite = await db
      .collection(COLLECTION_INVITATIONS)
      .where("venueId", "==", venueId)
      .where("invitedUserEmail", "==", email)
      .where("isAccepted", "==", false)
      .limit(1)
      .get();

    if (existingInvite.empty) {
      const now = admin.firestore.FieldValue.serverTimestamp();
      const expiry = admin.firestore.Timestamp.fromMillis(Date.now() + 30 * 24 * 60 * 60 * 1000);
      await db.collection(COLLECTION_INVITATIONS).add({
        venueId,
        venueName,
        invitedUserEmail: email,
        invitedByEmail: caller.email || "",
        invitedByName: caller.email || "",
        role,
        invitationDate: now,
        expirationDate: expiry,
        isAccepted: false,
        isExpired: false
      });
    }

    await sendMail(
      email,
      `You've been invited to ${venueLabel} on Encore`,
      `${callerLabel} invited you to join ${venueLabel} on Encore as ${role}. Download Encore and sign up with this email address (${email}) to get access.`,
      `<p><strong>${callerLabel}</strong> invited you to join <strong>${venueLabel}</strong> on Encore as <strong>${role}</strong>.</p>` +
        `<p>Download Encore and sign up with this email address (<strong>${email}</strong>) to get access.</p>`
    );

    return { ok: true, activated: false };
  });

  //============================================================
  // acceptVenueInvitation -- claims ONE pending invitation into an active
  // user-venue-lookup row for the CALLER. The invitation's target email is
  // re-checked against the caller's own auth token server-side, never
  // trusted from the client, so this can't be used to claim someone
  // else's invitation. Two callers:
  //   - LoginFlowController::runPostAuthFlow: once per pending invitation
  //     matching the signed-in email, silently, on every login.
  //   - OnboardingWizard's JoinInvitedVenueStep: once, for the single
  //     invitation the wizard detected during signup.
  //============================================================
  const acceptVenueInvitation = onCall(CALL_OPTS, async (request) => {
    if (!request.auth) {
      throw new HttpsError("unauthenticated", "Sign in required.");
    }
    const uid = request.auth.uid;
    const callerEmail = String(request.auth.token.email || "").toLowerCase();
    const invitationId = String((request.data || {}).invitationId || "").trim();
    if (!invitationId) {
      throw new HttpsError("invalid-argument", "invitationId is required.");
    }

    const invRef = db.collection(COLLECTION_INVITATIONS).doc(invitationId);
    const invSnap = await invRef.get();
    if (!invSnap.exists) {
      // Already claimed (e.g. a race between two logins) or expired/
      // revoked -- not an error worth surfacing to the caller.
      return { ok: true, alreadyGone: true };
    }
    const inv = invSnap.data() || {};
    if (String(inv.invitedUserEmail || "").toLowerCase() !== callerEmail) {
      throw new HttpsError("permission-denied", "This invitation is not for you.");
    }

    const venueId = String(inv.venueId || "");
    if (!venueId) {
      throw new HttpsError("failed-precondition", "Invitation has no venueId.");
    }

    // Re-fetch the venue's current name rather than trusting whatever was
    // cached on the invitation at invite time.
    const venueSnap = await db.collection(COLLECTION_VENUES).doc(venueId).get();
    const venueName = venueSnap.exists
      ? String((venueSnap.data() || {}).name || "")
      : String(inv.venueName || "");

    await upsertActiveMembership(uid, venueId, venueName, String(inv.role || "Host"), callerEmail);
    await invRef.delete();

    return { ok: true, venueId, venueName };
  });

  return { addVenueMember, acceptVenueInvitation };
};
