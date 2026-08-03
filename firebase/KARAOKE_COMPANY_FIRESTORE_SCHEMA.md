# Karaoke Company Firestore Schema Contract

## Purpose
Field-level schema for multi-tenant company, venue, host, device, and song distribution workflows.

## Conventions
- id fields are string identifiers.
- Timestamps use Firestore Timestamp.
- Enums are stored as lowercase strings.
- All tenant-owned docs include companyId.

## Top-Level Collections

### companies/{companyId}
Fields:
- name: string
- status: active | suspended
- createdAt: Timestamp
- updatedAt: Timestamp
- ownerUserId: string
- settings:
  - minSupportedClientVersion: string
  - latestRecommendedClientVersion: string
  - forcedUpgradeClientVersion: string
  - autoDownloadOnWifi: boolean

Example:
```json
{
  "name": "Encore West",
  "status": "active",
  "ownerUserId": "uid_company_admin_1",
  "settings": {
    "minSupportedClientVersion": "1.0.0",
    "latestRecommendedClientVersion": "1.2.4",
    "forcedUpgradeClientVersion": "1.1.8",
    "autoDownloadOnWifi": false
  },
  "createdAt": "SERVER_TIMESTAMP",
  "updatedAt": "SERVER_TIMESTAMP"
}
```

### deviceAssignments/{deviceId}
Denormalized fast-lookup for startup.

Fields:
- deviceId: string
- companyId: string
- venueId: string
- hostId: string | null
- assignmentType: permanent | temporary
- assignedAt: Timestamp
- expiresAt: Timestamp | null
- status: active | unassigned | revoked

### hostVenueAssignments/{assignmentId}
Fields:
- assignmentId: string
- companyId: string
- hostId: string
- venueId: string
- roleAtVenue: host | lead_host
- startsAt: Timestamp | null
- endsAt: Timestamp | null
- status: active | inactive
- createdBy: string
- createdAt: Timestamp

## Company Subcollections

### companies/{companyId}/venues/{venueId}
Fields:
- name: string
- code: string
- timezone: string
- status: active | inactive
- address:
  - line1: string
  - city: string
  - state: string
  - postalCode: string
- createdAt: Timestamp
- updatedAt: Timestamp

### companies/{companyId}/hosts/{hostId}
hostId should equal Firebase Auth UID.

Fields:
- hostId: string
- email: string
- displayName: string
- status: invited | active | disabled
- invitedBy: string
- invitedAt: Timestamp | null
- acceptedAt: Timestamp | null
- defaultVenueId: string | null
- permissions:
  - canRunVenue: boolean
  - canViewReports: boolean
- createdAt: Timestamp
- updatedAt: Timestamp

### companies/{companyId}/devices/{deviceId}
Fields:
- deviceId: string
- label: string
- platform: macos | windows
- appVersion: string
- lastSeenAt: Timestamp
- status: active | inactive | lost
- venueId: string | null
- hostId: string | null
- registeredBy: string
- registeredAt: Timestamp
- metadata:
  - machineName: string
  - osVersion: string

### companies/{companyId}/songPackages/{packageId}
Fields:
- packageId: string
- companyId: string
- name: string
- version: string
- status: draft | published | revoked
- minClientVersion: string
- manifestPath: string
- songCount: number
- totalBytes: number
- checksum: string
- publishedAt: Timestamp | null
- publishedBy: string | null
- supersedesPackageId: string | null
- createdAt: Timestamp
- updatedAt: Timestamp

### companies/{companyId}/campaigns/{campaignId}
Fields:
- campaignId: string
- companyId: string
- packageId: string
- name: string
- status: scheduled | active | completed | cancelled
- rollout:
  - mode: all_devices | venues | devices
  - venueIds: string[]
  - deviceIds: string[]
- startAt: Timestamp | null
- minClientVersion: string
- createdBy: string
- createdAt: Timestamp
- updatedAt: Timestamp

### companies/{companyId}/deviceUpdateStatus/{statusId}
One row per device per package.
statusId recommendation: deviceId_packageId

Fields:
- statusId: string
- companyId: string
- deviceId: string
- venueId: string | null
- packageId: string
- campaignId: string | null
- state: pending | downloading | verifying | installing | installed | failed | skipped
- progressPercent: number
- bytesDownloaded: number
- totalBytes: number
- errorCode: string | null
- errorMessage: string | null
- updatedAt: Timestamp
- installedAt: Timestamp | null

### companies/{companyId}/audits/{auditId}
Fields:
- type: host_invited | host_accepted | device_assigned | package_published | update_installed | update_failed | venue_session_started | venue_session_ended
- actorUserId: string
- actorRole: platform_admin | company_admin | host | system
- targetId: string
- targetType: host | device | venue | package | campaign | session
- details: map
- createdAt: Timestamp

## Suggested Composite Indexes
1. deviceUpdateStatus by companyId + state + updatedAt desc
2. campaigns by companyId + status + startAt
3. devices by companyId + status + lastSeenAt desc
4. hosts by companyId + status + displayName

## Client Write Boundaries
Direct client writes (host app):
- companies/{companyId}/devices/{deviceId}.lastSeenAt, appVersion
- companies/{companyId}/deviceUpdateStatus/{statusId} for own device only

Privileged writes (Cloud Functions):
- songPackages publish/revoke
- campaigns creation and fan-out status rows
- assignment lifecycle updates
- audit ingestion for sensitive actions

## Migration Notes
- Keep existing venue queue documents unchanged initially.
- Add companyId references incrementally to legacy docs.
- Build backfill script for hosts/devices before enforcing strict rules.
