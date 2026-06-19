# Karaoke Company Cloud Functions API Contract

## Purpose
Contract for server-side endpoints handling song package publishing, campaign fan-out, and device update status reporting.

## Principles
- All privileged operations execute in Cloud Functions.
- Every request is authenticated.
- Tenant scope is enforced by companyId and membership checks.
- Endpoints are idempotent where possible.

## Endpoint Summary
1. createHostInvite (callable)
2. acceptHostInvite (callable)
3. registerDevice (callable)
4. assignDeviceToVenue (callable)
5. createSongPackage (HTTPS upload-init + finalize callable)
6. publishSongPackage (callable)
7. createUpdateCampaign (callable)
8. getPendingUpdatesForDevice (callable)
9. reportDeviceUpdateStatus (callable)
10. revokeSongPackage (callable)

## Auth and Headers
- Firebase Auth ID token required for all callables and HTTPS endpoints.
- App Check required for client-originated endpoints.
- Correlation id header recommended: x-request-id.

## Detailed Contracts

### 1) createHostInvite
Type: Callable
Caller: company admin

Request:
```json
{
  "companyId": "cmp_123",
  "email": "host@company.com",
  "displayName": "DJ Alex",
  "defaultVenueId": "ven_101"
}
```

Response:
```json
{
  "inviteId": "inv_abc",
  "status": "invited",
  "expiresAt": "2026-07-01T00:00:00Z"
}
```

Validation:
- Caller role must be company_admin for companyId.
- Email unique within active company host accounts.

### 2) acceptHostInvite
Type: Callable
Caller: invited user

Request:
```json
{
  "inviteId": "inv_abc",
  "companyId": "cmp_123"
}
```

Response:
```json
{
  "hostId": "uid_host_9",
  "status": "active"
}
```

### 3) registerDevice
Type: Callable
Caller: host or company admin

Request:
```json
{
  "companyId": "cmp_123",
  "deviceId": "dev_mac_99",
  "label": "MacBook Pro KJ-2",
  "platform": "macos",
  "appVersion": "1.2.4",
  "metadata": {
    "machineName": "Encore-KJ-2",
    "osVersion": "macOS 15"
  }
}
```

Response:
```json
{
  "deviceId": "dev_mac_99",
  "status": "active",
  "registeredAt": "SERVER_TIMESTAMP"
}
```

Idempotency:
- Repeated registerDevice for same deviceId updates heartbeat/appVersion.

### 4) assignDeviceToVenue
Type: Callable
Caller: company admin

Request:
```json
{
  "companyId": "cmp_123",
  "deviceId": "dev_mac_99",
  "venueId": "ven_101",
  "hostId": "uid_host_9",
  "assignmentType": "temporary",
  "expiresAt": "2026-06-22T06:00:00Z"
}
```

Response:
```json
{
  "deviceId": "dev_mac_99",
  "venueId": "ven_101",
  "hostId": "uid_host_9",
  "status": "active"
}
```

Side effects:
- Writes companies/{companyId}/devices/{deviceId}
- Writes deviceAssignments/{deviceId}
- Emits audit event

### 5) createSongPackage
Type: HTTPS + callable finalize

Step A: init upload (callable)
Request:
```json
{
  "companyId": "cmp_123",
  "name": "June Rotation Pack",
  "version": "2026.06.19",
  "minClientVersion": "1.1.8"
}
```

Response:
```json
{
  "packageId": "pkg_20260619_01",
  "uploadPrefix": "companies/cmp_123/songs/pkg_20260619_01/",
  "signedUploadUrls": ["https://..."],
  "status": "draft"
}
```

Step B: finalize package (callable)
Request:
```json
{
  "companyId": "cmp_123",
  "packageId": "pkg_20260619_01",
  "files": [
    {
      "path": "songs/artist_a-track_b.zip",
      "bytes": 12345678,
      "sha256": "abcdef..."
    }
  ]
}
```

Response:
```json
{
  "packageId": "pkg_20260619_01",
  "manifestPath": "companies/cmp_123/manifests/pkg_20260619_01.json",
  "songCount": 245,
  "totalBytes": 987654321,
  "status": "draft"
}
```

### 6) publishSongPackage
Type: Callable
Caller: company admin

Request:
```json
{
  "companyId": "cmp_123",
  "packageId": "pkg_20260619_01",
  "supersedesPackageId": "pkg_20260601_01"
}
```

Response:
```json
{
  "packageId": "pkg_20260619_01",
  "status": "published",
  "publishedAt": "SERVER_TIMESTAMP"
}
```

Validation:
- Package exists and is draft.
- Manifest exists and checksums are present.

### 7) createUpdateCampaign
Type: Callable
Caller: company admin

Request:
```json
{
  "companyId": "cmp_123",
  "name": "Weekend Rollout",
  "packageId": "pkg_20260619_01",
  "rollout": {
    "mode": "venues",
    "venueIds": ["ven_101", "ven_102"],
    "deviceIds": []
  },
  "startAt": "2026-06-20T00:00:00Z",
  "minClientVersion": "1.1.8"
}
```

Response:
```json
{
  "campaignId": "cmpn_789",
  "status": "scheduled",
  "targetDeviceCount": 14
}
```

Side effects:
- Creates campaign doc.
- Fans out deviceUpdateStatus rows (state pending).

### 8) getPendingUpdatesForDevice
Type: Callable
Caller: host app on startup/poll

Request:
```json
{
  "companyId": "cmp_123",
  "deviceId": "dev_mac_99",
  "appVersion": "1.2.4"
}
```

Response:
```json
{
  "updates": [
    {
      "statusId": "dev_mac_99_pkg_20260619_01",
      "campaignId": "cmpn_789",
      "packageId": "pkg_20260619_01",
      "packageName": "June Rotation Pack",
      "version": "2026.06.19",
      "manifestPath": "companies/cmp_123/manifests/pkg_20260619_01.json",
      "totalBytes": 987654321,
      "minClientVersion": "1.1.8",
      "state": "pending"
    }
  ]
}
```

Behavior:
- Excludes campaigns not yet started.
- Excludes packages incompatible with client if policy requires.

### 9) reportDeviceUpdateStatus
Type: Callable
Caller: host app downloader

Request:
```json
{
  "companyId": "cmp_123",
  "deviceId": "dev_mac_99",
  "statusId": "dev_mac_99_pkg_20260619_01",
  "state": "downloading",
  "progressPercent": 42,
  "bytesDownloaded": 414000000,
  "totalBytes": 987654321,
  "errorCode": null,
  "errorMessage": null
}
```

Response:
```json
{
  "ok": true,
  "updatedAt": "SERVER_TIMESTAMP"
}
```

State machine:
- pending -> downloading -> verifying -> installing -> installed
- pending/downloading/verifying/installing -> failed
- pending -> skipped

### 10) revokeSongPackage
Type: Callable
Caller: company admin

Request:
```json
{
  "companyId": "cmp_123",
  "packageId": "pkg_20260619_01",
  "reason": "corrupt file"
}
```

Response:
```json
{
  "packageId": "pkg_20260619_01",
  "status": "revoked"
}
```

Side effects:
- Marks package revoked.
- Marks active campaigns cancelled or halted.
- Writes audit event.

## Error Contract

Common error shape:
```json
{
  "error": {
    "code": "permission_denied",
    "message": "User does not have company admin role for cmp_123",
    "details": {
      "companyId": "cmp_123",
      "requiredRole": "company_admin"
    }
  }
}
```

Suggested codes:
- invalid_argument
- unauthenticated
- permission_denied
- not_found
- failed_precondition
- aborted
- resource_exhausted
- internal

## Observability and Idempotency
- Log all mutation endpoints with companyId, actor uid, request id.
- Include packageId/campaignId/deviceId in structured logs.
- Make publish and fan-out idempotent using deterministic operation ids.
- Reject duplicate status regressions unless explicitly allowed.

## Suggested Implementation Order
1. registerDevice + assignDeviceToVenue
2. createSongPackage (init/finalize)
3. publishSongPackage
4. createUpdateCampaign
5. getPendingUpdatesForDevice + reportDeviceUpdateStatus
6. invite/host flows and revoke paths
