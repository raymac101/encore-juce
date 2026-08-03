# Karaoke Company Admin Architecture

## Objective
Design a multi-tenant system where karaoke companies can:
- Manage many venues, hosts, and laptops.
- Assign hosts and devices to venues.
- Distribute new songs to company-managed devices.
- Notify devices when new songs are available, with an in-app download flow.

## Goals
- Multi-company isolation with strict tenancy boundaries.
- Centralized admin for company operations.
- Reliable song distribution with resume, validation, and status tracking.
- Clear role-based access control for admin and host workflows.
- Compatibility controls across multiple Encore client versions.

## Non-Goals (Initial Scope)
- Real-time collaborative editing of company data.
- Full MDM-style remote control of laptops.
- Automatic forced installation of large song packages without user consent.

## Core Domain Model
Primary entities:
1. Company
2. Venue
3. Host
4. Device (laptop)
5. HostVenueAssignment
6. DeviceAssignment
7. SongPackage
8. UpdateCampaign
9. DeviceUpdateStatus
10. SessionAuditEvent

Every tenant-owned record carries companyId.

## Roles and Permissions
Roles:
1. Platform Admin: internal Encore operator with global access.
2. Company Admin: manages venues, hosts, devices, songs, campaigns in one company.
3. Host: operates assigned venue(s), can run sessions.
4. Viewer/Manager (optional): reporting and read-only operations.

Standalone users remain supported through the existing legacy host/venue flow. Company-aware clients can read optional auth claims such as companyId and companyRole to switch into multi-tenant mode when the backend starts issuing them.

Enforcement:
- Firebase Auth custom claims for coarse roles.
- Firestore role membership documents for per-company and per-venue scoping.
- Cloud Functions for privileged mutation paths (publishing packages/campaigns).

## Firestore Data Layout

Top-level collections:
- companies/{companyId}
- deviceAssignments/{deviceId}
- hostVenueAssignments/{assignmentId}

Tenant subcollections:
- companies/{companyId}/venues/{venueId}
- companies/{companyId}/hosts/{hostId}
- companies/{companyId}/devices/{deviceId}
- companies/{companyId}/songPackages/{packageId}
- companies/{companyId}/campaigns/{campaignId}
- companies/{companyId}/deviceUpdateStatus/{statusId}
- companies/{companyId}/audits/{auditId}

Suggested keys:
- hostId: Firebase Auth UID.
- deviceId: deterministic install/device fingerprint generated at first app bootstrap.
- packageId: semantic package version or generated id.

## Storage Layout (Firebase Storage)
- companies/{companyId}/songs/{packageId}/...
- companies/{companyId}/artwork/...
- companies/{companyId}/manifests/{packageId}.json

Manifest includes:
- songId
- title/artist/vendor metadata
- file path
- size
- hash/checksum
- optional replacement/supersedes metadata

## Admin Console Features
1. Company admin dashboard
- Venue list and status
- Host roster and invitation status
- Device inventory with assignment state
- Package/campaign history and install metrics

2. Host management
- Invite host by email.
- Accept invite and attach role.
- Assign host to one or multiple venues with optional schedule windows.

3. Device management
- Register laptop via pairing code/QR.
- Assign laptop to venue (static or temporary).
- Mark device active/inactive/lost.

4. Song management
- Upload songs in batch (zip or file set).
- Validate media, compute checksums, write manifest.
- Publish package to targets.

5. Campaign management
- Target all devices, specific venues, or specific devices.
- Set rollout timing and minimum client version.
- Observe per-device delivery state.

## Song Distribution Workflow
1. Company admin uploads songs.
2. Cloud Function validates payload and writes SongPackage + manifest.
3. Admin publishes UpdateCampaign.
4. System writes pending DeviceUpdateStatus rows for target devices.
5. Encore laptop app checks for updates on startup and periodic refresh.
6. If update exists, app shows popup: New songs available.
7. User chooses Download now or Remind me later.
8. Client downloads files with resumable transfers.
9. Client verifies checksums, imports into local Encore library, and updates status.
10. Server reflects Installed or Failed with reason code.

## Laptop and Venue Runtime Flow
1. Host signs in.
2. App resolves company, host assignments, and device assignment.
3. App validates host is allowed for selected venue.
4. Venue session begins and emits audit events.
5. During startup/idle windows, update checks run and notifications are shown.

## Version Compatibility Strategy
Maintain client release policy records:
- minSupportedVersion
- latestRecommendedVersion
- forcedUpgradeVersion

Campaign-level gates:
- minClientVersion

Behavior:
- If client below minSupportedVersion, block package install and show required upgrade.
- If below latestRecommendedVersion, warn but allow operation.

## Security Model
Firestore rules:
- Require authenticated user for all reads/writes.
- Require company scoping on all company-owned documents.
- Prevent cross-company access via companyId checks.
- Restrict company admin mutation rights to their own company.
- Restrict host access to assigned venues and own profile/session context.

Storage rules:
- Scope access by companies/{companyId}/...
- Enforce authenticated + company membership checks.

Cloud Functions:
- Perform privileged writes for publish operations.
- Validate requested targets belong to company.
- Record immutable audit logs for sensitive actions.

## Reliability and Operations
- Idempotent installs (safe retries).
- Resume interrupted downloads.
- Preflight disk-space checks.
- Atomic import stage (temp download folder then commit).
- Rollback controls (mark package revoked).
- Detailed per-device failure codes and retry policy.

## Audit and Compliance
Capture events for:
- Host invite accepted/rejected.
- Device registered/assigned/unassigned.
- Package uploaded/published/revoked.
- Device install started/completed/failed.
- Session start/end and venue operation actions.

## MVP Rollout Plan

Phase 1: Tenancy and Access
- Company model and role model.
- Host invite and venue assignment.
- Device registration and assignment.

Phase 2: Package Distribution
- SongPackage + manifest pipeline.
- Startup update detection.
- New songs available popup and manual download/install.

Phase 3: Campaigns and Controls
- Targeted campaigns.
- Device status dashboard.
- Version gates and required upgrade policy.

## Open Design Decisions
1. Should updates auto-download on trusted high-bandwidth networks?
2. Should hosts be able to defer mandatory packages, and for how long?
3. What maximum package size should trigger staged download behavior?
4. Do we need delta packages, or are full package updates acceptable initially?

## Suggested Next Deliverables
1. Firestore schema contract document (field-level definitions).
2. Firestore/Storage security rules draft.
3. Cloud Functions API contract for package publish and status reporting.
4. Encore desktop client update-state machine spec.
