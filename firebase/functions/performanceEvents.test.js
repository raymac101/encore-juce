const test = require("node:test");
const assert = require("node:assert/strict");
const { _test } = require("./performanceEvents");

function validPayload(overrides = {}) {
  return {
    eventId: "8adfdb68-c2d3-4ae6-9228-f0bc6d84da75",
    venueId: "venue-123",
    songId: "song-456",
    songName: "You're All I Need To Get By",
    artist: "Marvin Gaye & Tammi Terrell",
    songVersion: "DK-100",
    songDurationMs: 185000,
    actualPlayedDurationMs: 45000,
    completionReason: "stopped",
    userId: "user-789",
    guestSingerId: "",
    singerStageName: "EncoreSinger1",
    source: "mobile",
    platform: "ios",
    deviceId: "device-opaque-id",
    encoreInstallationId: "encore-installation-id",
    clientStartedAt: "2026-08-15T00:30:00.000Z",
    clientEndedAt: "2026-08-15T00:30:45.000Z",
    ...overrides
  };
}

test("night key keeps after-midnight UTC activity on the prior karaoke night", () => {
  assert.equal(
    _test.calculateNightKey(Date.parse("2026-08-15T01:00:00.000Z"), "UTC", 5),
    "2026-08-14"
  );
  assert.equal(
    _test.calculateNightKey(Date.parse("2026-08-15T05:00:00.000Z"), "UTC", 5),
    "2026-08-15"
  );
});

test("night key uses the venue timezone rather than server timezone", () => {
  const timestamp = Date.parse("2026-08-15T06:30:00.000Z"); // 2:30 AM in Toronto (EDT)
  assert.equal(_test.calculateNightKey(timestamp, "America/Toronto", 5), "2026-08-14");
});

test("qualified stopped performance validates after thirty seconds", () => {
  const payload = _test.validatePayload(validPayload());
  assert.equal(payload.actualPlayedDurationMs, 45000);
  assert.equal(payload.userId, "user-789");
});

test("natural completion qualifies below thirty seconds", () => {
  const payload = _test.validatePayload(validPayload({
    actualPlayedDurationMs: 12000,
    completionReason: "completed"
  }));
  assert.equal(payload.completionReason, "completed");
});

test("unqualified partial performance is rejected", () => {
  assert.throws(
    () => _test.validatePayload(validPayload({ actualPlayedDurationMs: 29999 })),
    /audit threshold/
  );
});

test("registered or venue-scoped guest identity is required", () => {
  assert.throws(
    () => _test.validatePayload(validPayload({ userId: "", guestSingerId: "" })),
    /userId or guestSingerId/
  );

  const guest = _test.validatePayload(validPayload({
    userId: "",
    guestSingerId: "venue-guest-12"
  }));
  assert.equal(guest.guestSingerId, "venue-guest-12");
});

test("event UUID and timestamps are validated", () => {
  assert.throws(
    () => _test.validatePayload(validPayload({ eventId: "not-a-uuid" })),
    /must be a UUID/
  );
  assert.throws(
    () => _test.validatePayload(validPayload({
      clientStartedAt: "2026-08-15T00:31:00.000Z",
      clientEndedAt: "2026-08-15T00:30:45.000Z"
    })),
    /cannot precede/
  );
});

test("validated payload hash is deterministic for retry detection", () => {
  const first = _test.validatePayload(validPayload());
  const second = _test.validatePayload(validPayload());
  assert.equal(_test.stablePayloadHash(first), _test.stablePayloadHash(second));
  assert.notEqual(
    _test.stablePayloadHash(first),
    _test.stablePayloadHash({ ...second, songId: "different-song" })
  );
});

test("canonical event snapshots venue geography and marks BigQuery pending", () => {
  const payload = _test.validatePayload(validPayload());
  const serverTimestamp = { sentinel: "server-time" };
  const admin = {
    firestore: {
      Timestamp: { fromMillis: (millis) => ({ millis }) },
      FieldValue: { serverTimestamp: () => serverTimestamp }
    }
  };
  const event = _test.buildCanonicalEvent(payload, {
    name: "Karaoke Palace",
    companyId: "company-1",
    timezone: "America/Toronto",
    nightCutoffHour: 5,
    city: "Toronto",
    provinceCode: "ON",
    province: "Ontario",
    countryCode: "CA",
    country: "Canada"
  }, "kj-1", admin);

  assert.equal(event.venueName, "Karaoke Palace");
  assert.equal(event.subdivisionCode, "ON");
  assert.equal(event.countryCode, "CA");
  assert.equal(event.nightKey, "2026-08-14");
  assert.equal(event.bigQueryStatus, "pending");
  assert.equal(event.expireAt, null);
});