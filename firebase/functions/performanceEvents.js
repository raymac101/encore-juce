const crypto = require("crypto");
const { onCall, HttpsError } = require("firebase-functions/v2/https");
const { logger } = require("firebase-functions");

const CALL_OPTS = { region: "us-central1", enforceAppCheck: false, cors: true };
const SCHEMA_VERSION = 2;
const MIN_QUALIFIED_DURATION_MS = 30_000;
const DEFAULT_NIGHT_CUTOFF_HOUR = 5;
const OPERATOR_ROLES = ["Host", "Admin", "Tester", "EnterpriseAdmin"];
const SOURCES = new Set(["encore", "mobile", "unknown"]);
const PLATFORMS = new Set(["ios", "android", "macos", "windows", "unknown"]);
const COMPLETION_REASONS = new Set(["completed", "stopped", "skipped"]);
const UUID_PATTERN = /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;

function requiredString(data, field, maxLength = 500) {
  const value = String(data[field] || "").trim();
  if (!value) {
    throw new HttpsError("invalid-argument", `${field} is required.`);
  }
  if (value.length > maxLength) {
    throw new HttpsError("invalid-argument", `${field} is too long.`);
  }
  return value;
}

function optionalString(data, field, maxLength = 500) {
  const value = String(data[field] || "").trim();
  if (value.length > maxLength) {
    throw new HttpsError("invalid-argument", `${field} is too long.`);
  }
  return value;
}

function nonNegativeInteger(data, field) {
  const value = Number(data[field]);
  if (!Number.isSafeInteger(value) || value < 0) {
    throw new HttpsError("invalid-argument", `${field} must be a non-negative integer.`);
  }
  return value;
}

function enumValue(data, field, allowed, fallback) {
  const value = String(data[field] || fallback).trim().toLowerCase();
  if (!allowed.has(value)) {
    throw new HttpsError("invalid-argument", `${field} has an unsupported value.`);
  }
  return value;
}

function parseClientTimestamp(data, field) {
  const value = data[field];
  const millis = typeof value === "number" ? value : Date.parse(String(value || ""));
  if (!Number.isFinite(millis) || millis <= 0) {
    throw new HttpsError("invalid-argument", `${field} must be a valid timestamp.`);
  }
  return Math.trunc(millis);
}

function previousIsoDate(year, month, day) {
  const date = new Date(Date.UTC(year, month - 1, day) - 24 * 60 * 60 * 1000);
  return date.toISOString().slice(0, 10);
}

function calculateNightKey(timestampMs, timezone, cutoffHour = DEFAULT_NIGHT_CUTOFF_HOUR) {
  let parts;
  try {
    parts = new Intl.DateTimeFormat("en-CA", {
      timeZone: timezone,
      year: "numeric",
      month: "2-digit",
      day: "2-digit",
      hour: "2-digit",
      hourCycle: "h23"
    }).formatToParts(new Date(timestampMs));
  } catch (err) {
    throw new HttpsError("failed-precondition", `Venue timezone is invalid: ${timezone}`);
  }

  const values = Object.fromEntries(parts.map((part) => [part.type, part.value]));
  const year = Number(values.year);
  const month = Number(values.month);
  const day = Number(values.day);
  const hour = Number(values.hour);
  const localDate = `${values.year}-${values.month}-${values.day}`;
  return hour < cutoffHour ? previousIsoDate(year, month, day) : localDate;
}

function firstVenueString(venue, names, fallback = "") {
  for (const name of names) {
    const value = String(venue[name] || "").trim();
    if (value) return value;
  }
  return fallback;
}

function validatePayload(rawData) {
  const data = rawData || {};
  const eventId = requiredString(data, "eventId", 36);
  if (!UUID_PATTERN.test(eventId)) {
    throw new HttpsError("invalid-argument", "eventId must be a UUID.");
  }

  const userId = optionalString(data, "userId", 128);
  const guestSingerId = optionalString(data, "guestSingerId", 128);
  if (!userId && !guestSingerId) {
    throw new HttpsError("invalid-argument", "userId or guestSingerId is required.");
  }

  const actualPlayedDurationMs = nonNegativeInteger(data, "actualPlayedDurationMs");
  const completionReason = enumValue(data, "completionReason", COMPLETION_REASONS, "stopped");
  if (actualPlayedDurationMs < MIN_QUALIFIED_DURATION_MS && completionReason !== "completed") {
    throw new HttpsError("failed-precondition", "Performance has not reached the audit threshold.");
  }

  const clientStartedAtMs = parseClientTimestamp(data, "clientStartedAt");
  const clientEndedAtMs = parseClientTimestamp(data, "clientEndedAt");
  if (clientEndedAtMs < clientStartedAtMs) {
    throw new HttpsError("invalid-argument", "clientEndedAt cannot precede clientStartedAt.");
  }

  return {
    eventId,
    venueId: requiredString(data, "venueId", 128),
    songId: requiredString(data, "songId", 256),
    songName: requiredString(data, "songName", 500),
    artist: requiredString(data, "artist", 500),
    songVersion: optionalString(data, "songVersion", 200),
    songDurationMs: nonNegativeInteger(data, "songDurationMs"),
    actualPlayedDurationMs,
    completionReason,
    userId,
    guestSingerId,
    singerStageName: requiredString(data, "singerStageName", 200),
    source: enumValue(data, "source", SOURCES, "unknown"),
    platform: enumValue(data, "platform", PLATFORMS, "unknown"),
    deviceId: optionalString(data, "deviceId", 256),
    encoreInstallationId: optionalString(data, "encoreInstallationId", 256),
    clientStartedAtMs,
    clientEndedAtMs
  };
}

function stablePayloadHash(payload) {
  return crypto.createHash("sha256").update(JSON.stringify(payload)).digest("hex");
}

function buildCanonicalEvent(payload, venue, callerUid, admin) {
  const venueTimezone = firstVenueString(venue, ["timezone"], "UTC");
  const configuredCutoff = Number(venue.nightCutoffHour);
  const nightCutoffHour = Number.isInteger(configuredCutoff) && configuredCutoff >= 0 && configuredCutoff <= 12
    ? configuredCutoff
    : DEFAULT_NIGHT_CUTOFF_HOUR;

  return {
    eventId: payload.eventId,
    schemaVersion: SCHEMA_VERSION,
    qualified: true,
    inputHash: stablePayloadHash(payload),

    venueId: payload.venueId,
    venueName: firstVenueString(venue, ["name", "venueName"]),
    companyId: firstVenueString(venue, ["companyId"]),
    venueTimezone,
    nightCutoffHour,
    nightKey: calculateNightKey(payload.clientEndedAtMs, venueTimezone, nightCutoffHour),
    city: firstVenueString(venue, ["city"]),
    subdivisionCode: firstVenueString(venue, ["subdivisionCode", "provinceCode", "stateCode"]),
    subdivisionName: firstVenueString(venue, ["subdivisionName", "province", "state"]),
    countryCode: firstVenueString(venue, ["countryCode"]),
    countryName: firstVenueString(venue, ["country", "countryName"]),

    songId: payload.songId,
    songName: payload.songName,
    artist: payload.artist,
    songVersion: payload.songVersion,
    songDurationMs: payload.songDurationMs,
    actualPlayedDurationMs: payload.actualPlayedDurationMs,
    completionReason: payload.completionReason,

    userId: payload.userId || null,
    guestSingerId: payload.guestSingerId || null,
    singerStageName: payload.singerStageName,
    source: payload.source,
    platform: payload.platform,
    deviceId: payload.deviceId || null,
    encoreInstallationId: payload.encoreInstallationId || null,
    kjId: callerUid,

    clientStartedAt: admin.firestore.Timestamp.fromMillis(payload.clientStartedAtMs),
    clientEndedAt: admin.firestore.Timestamp.fromMillis(payload.clientEndedAtMs),
    serverRecordedAt: admin.firestore.FieldValue.serverTimestamp(),
    bigQueryStatus: "pending",
    bigQueryDeliveredAt: null,
    expireAt: null
  };
}

module.exports = function performanceEventsModule(admin, db) {
  async function requireVenueOperator(request, venueId) {
    if (!request.auth) {
      throw new HttpsError("unauthenticated", "Sign in required.");
    }
    if (request.auth.token.platformAdmin === true) {
      return request.auth.uid;
    }

    const uid = request.auth.uid;
    const membershipSnap = await db.collection("user-venue-lookup").doc(`${uid}_${venueId}`).get();
    const membership = membershipSnap.exists ? membershipSnap.data() || {} : {};
    if (membership.status !== "active" || !OPERATOR_ROLES.includes(String(membership.role || ""))) {
      throw new HttpsError("permission-denied", "Active venue operator access is required.");
    }
    return uid;
  }

  const recordPerformanceEventV2 = onCall(CALL_OPTS, async (request) => {
    const payload = validatePayload(request.data);
    const callerUid = await requireVenueOperator(request, payload.venueId);
    const venueRef = db.collection("venues").doc(payload.venueId);
    const venueSnap = await venueRef.get();
    if (!venueSnap.exists) {
      throw new HttpsError("not-found", "Venue not found.");
    }

    const canonical = buildCanonicalEvent(payload, venueSnap.data() || {}, callerUid, admin);
    const eventRef = db.collection("performanceEvents").doc(payload.eventId);
    const venueRecentRef = venueRef.collection("recentPerformances").doc(payload.eventId);
    const singerId = payload.userId || payload.guestSingerId;
    const memberRef = venueRef.collection("members").doc(singerId);
    const userHistoryRef = payload.userId
      ? db.collection("users").doc(payload.userId).collection("performanceHistory").doc(payload.eventId)
      : null;

    const result = await db.runTransaction(async (tx) => {
      const [existing, memberSnap] = await Promise.all([
        tx.get(eventRef),
        tx.get(memberRef)
      ]);
      if (existing.exists) {
        if (String((existing.data() || {}).inputHash || "") !== canonical.inputHash) {
          throw new HttpsError("already-exists", "eventId already exists with different data.");
        }
        return { duplicate: true };
      }

      tx.create(eventRef, canonical);
      tx.create(venueRecentRef, {
        eventId: canonical.eventId,
        venueId: canonical.venueId,
        nightKey: canonical.nightKey,
        songId: canonical.songId,
        songName: canonical.songName,
        artist: canonical.artist,
        singerStageName: canonical.singerStageName,
        performedAt: canonical.clientEndedAt,
        serverRecordedAt: canonical.serverRecordedAt
      });

      if (userHistoryRef) {
        tx.create(userHistoryRef, {
          eventId: canonical.eventId,
          venueId: canonical.venueId,
          venueName: canonical.venueName,
          songId: canonical.songId,
          songName: canonical.songName,
          artist: canonical.artist,
          songVersion: canonical.songVersion,
          nightKey: canonical.nightKey,
          performedAt: canonical.clientEndedAt,
          serverRecordedAt: canonical.serverRecordedAt
        });
      }

      const memberUpdate = {
        singerId,
        userId: canonical.userId,
        guestSingerId: canonical.guestSingerId,
        singerStageName: canonical.singerStageName,
        lastPerformedAt: canonical.clientEndedAt,
        performanceCount: admin.firestore.FieldValue.increment(1),
        updatedAt: canonical.serverRecordedAt
      };
      if (!memberSnap.exists) {
        memberUpdate.firstPerformedAt = canonical.clientEndedAt;
      }
      tx.set(memberRef, memberUpdate, { merge: true });

      return { duplicate: false };
    });

    logger.info("performance event v2 recorded", {
      eventId: payload.eventId,
      venueId: payload.venueId,
      callerUid,
      duplicate: result.duplicate
    });

    return {
      eventId: payload.eventId,
      schemaVersion: SCHEMA_VERSION,
      status: result.duplicate ? "already_recorded" : "recorded",
      nightKey: canonical.nightKey,
      bigQueryStatus: "pending"
    };
  });

  return { recordPerformanceEventV2 };
};

module.exports._test = {
  calculateNightKey,
  validatePayload,
  stablePayloadHash,
  buildCanonicalEvent,
  DEFAULT_NIGHT_CUTOFF_HOUR,
  MIN_QUALIFIED_DURATION_MS
};