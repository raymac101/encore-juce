# Encore JUCE Migration Feature Gap Audit

## Purpose

This document compares the original Angular/Electron Encore application in `/Volumes/MediaDrive/CodeProjects/Encore/Encore/encore` with the current JUCE/C++ application in `/Volumes/MediaDrive/CodeProjects/Encore/Encore Juce/encore-juce`. The goal is to identify the remaining desktop features that still need to migrate to JUCE, separate true parity gaps from completed work, and provide a prioritized backlog for implementation.

## Scope

Included in scope:
- Desktop host workflows
- Operator and venue-management workflows
- Playback, queue, library, metadata, archive, and second-screen behavior
- Desktop-facing integrations with Firebase and mobile-driven requests

Excluded from scope:
- Pure mobile-app features unless the desktop app must receive or react to them
- Backend recovery work already completed
- New-product ideas that were not actually shipped in the Angular/Electron app
- Speculative JUCE-only ambitions unless they are required for Angular parity

## Evidence Sources

Primary Angular/Electron evidence:
- `src/app/services/venue.service.ts`
- `src/app/services/audio.service.ts`
- `src/app/components/lyric-display/lyric-display.component.ts`
- `src/app/services/auth.service.ts`
- `ARCHIVE_SYSTEM_DOCS.md`
- `ROUND_FEATURE_SUMMARY.md`
- `FIREBASE_UPLOAD_FEATURE.md`

Primary JUCE evidence:
- `CLAUDE.md`
- `Source/UI/MainComponent.cpp`
- `Source/Services/QueueService.cpp`
- `Source/Services/RequestService.cpp`
- `Source/Services/LibraryScanner.cpp`
- `Source/Services/ApiService.h`
- `Source/UI/SongEditDialog.cpp`
- `firebase/METADATA_MIGRATION_DESIGN.md`
- `firebase/MIGRATION_RECOVERY_LOG.md`

## Executive Summary

The JUCE application already covers the core karaoke-host foundation:
- Native audio engine
- CDG rendering and lyric window
- Queue management
- Request polling from Firebase
- Local song library scanning and SQLite search
- Venue loading, playlists, settings, and session archive flow
- Email/password login and venue-selection flow

The remaining migration work is concentrated in six areas:
- Metadata-client integration and shared metadata sync
- Authentication parity, especially OAuth/provider flows
- Admin and venue-management workflows
- Analytics and reporting parity
- Second-screen polish including ads and richer singer-display behavior
- Operational parity for advanced queue rules, uploads, and shared-song workflows

Additionally, Angular's dedicated ribbon control surface is a separate parity gap and is tracked explicitly in this document.

## Feature Comparison Matrix

### 1. Authentication and Access Control

Angular/Electron status:
- Email/password login
- Google OAuth
- Password reset
- Email verification
- User-to-venue association flows
- Role-aware access and invitation states

Evidence:
- `src/app/services/auth.service.ts`
- `src/app/services/user-venue.service.ts`

JUCE status:
- Email/password login is implemented
- Venue-selection and request-access flow exists
- OAuth/provider login is not fully implemented

Evidence:
- `Source/Services/FirestoreClient.h`
- `Source/UI/LoginWindow.cpp`
- `Source/Auth/LoginFlowController.cpp`

Gap classification:
- Partial migration

Remaining work:
- Implement Google and other provider login flows in JUCE
- Match Angular's invitation and access-state UX where still missing
- Confirm whether password reset and email-verification UX need parity in the native app

### 2. Queue Management and Singer Rotation

Angular/Electron status:
- Strong queue integrity and concurrency handling
- Advanced round-system behavior
- Reordering and recovery logic
- Approval and queue-rule enforcement at scale

Evidence:
- `src/app/services/venue.service.ts`
- `ROUND_FEATURE_SUMMARY.md`

JUCE status:
- Queue load, append, remove, delete, and watcher polling are implemented
- Request intake and approval wiring exist
- Core rotation UI exists in QueueBar
- Some advanced round and operational-hardening behavior appears thinner than Angular

Evidence:
- `Source/Services/QueueService.cpp`
- `Source/Services/RequestService.cpp`
- `Source/UI/QueueBar.cpp`

Gap classification:
- Partial migration

Remaining work:
- Audit Angular round-order logic against JUCE behavior and port any missing rules
- Port queue-integrity and concurrent-update hardening where JUCE still relies on simpler flows
- Validate parity for reorder edge cases, duplicate prevention, and recovery from transient network failures

### 3. Mobile Request Intake and Approval

Angular/Electron status:
- Polls requested collection
- Handles new, approved, and rejected states
- Auto-approve checks against queue rules
- Deletes or updates request state correctly

JUCE status:
- Same polling model and basic state transitions are implemented
- Core request-to-queue pipeline exists

Evidence:
- `src/app/services/venue.service.ts`
- `Source/Services/RequestService.cpp`
- `CLAUDE.md`

Gap classification:
- Mostly migrated

Remaining work:
- Confirm parity for Angular's edge-case checks and error recovery paths
- Confirm no missing host-side moderation or diagnostics UX around incoming requests

### 4. Local Library Import, Grouping, and Duplicate Handling

Angular/Electron status:
- File grouping for karaoke formats
- Duplicate detection and version handling
- Shared-song download and metadata merge behavior
- Admin upload path into Firebase-backed shared-song catalog

Evidence:
- `KARAOKE_FILE_GROUPING.md`
- `DUPLICATE_DETECTION_INTEGRATION.md`
- `FIREBASE_UPLOAD_FEATURE.md`

JUCE status:
- Local scan, grouping, merge, SQLite persistence, and local metadata apply are implemented
- Song editing and playlist tagging exist
- Cloud/shared-song workflow appears incomplete

Evidence:
- `Source/Services/LibraryScanner.cpp`
- `Source/Services/SongDatabase.cpp`
- `Source/UI/LibraryPage.cpp`
- `Source/UI/SongEditDialog.cpp`

Gap classification:
- Partial migration

Remaining work:
- Verify duplicate-detection parity against Angular's multi-level matching rules
- Port any missing shared-song download and upload workflows
- Port admin-only upload UX if it is still part of the product scope
- Finish any stubbed library-edit workflows such as genre-edit support if required for parity

### 5. Metadata Enrichment and Cloud Sync

Angular/Electron status:
- Metadata lookup and enrichment workflows are established
- Shared metadata/cloud-assisted behavior exists in combination with backend services

JUCE status:
- Local metadata apply exists
- Metadata backend groundwork exists in Firebase
- JUCE client integration is the clearest remaining migration gap

Evidence:
- `Source/Services/ApiService.h`
- `Source/UI/SongEditDialog.cpp`
- `firebase/METADATA_MIGRATION_DESIGN.md`
- `firebase/MIGRATION_RECOVERY_LOG.md`

Gap classification:
- Partial migration, high priority

Remaining work:
- Add cloud metadata read by normalized key
- Add enqueue fallback on cloud miss
- Add startup metadata sync and delta/snapshot handling if still part of the design
- Surface pending/queued/failure status in JUCE song-edit and library flows
- Merge fetched results back into local cache and song database

Note:
- Backend recovery and function isolation are already complete and should not be treated as an open migration item

### 6. Playback, DSP, and Mixer

Angular/Electron status:
- Playback engine with pitch shift, gain, pan, vocal reduction, EQ/compression, and analysis
- Mixer UI and audio controls

JUCE status:
- Native audio engine is more capable than Angular in core playback and DSP
- Pitch, tempo, EQ, compressor, reverb, echo, and waveform support exist
- Mixer page is present and functional

Evidence:
- `src/app/services/audio.service.ts`
- `Source/Audio/AudioEngine.cpp`
- `Source/Audio/PitchShifter.cpp`
- `Source/UI/MixerPage.cpp`

Gap classification:
- Migrated or better than parity for core host playback

Remaining work:
- Validate any Angular-specific controls not yet mapped in JUCE, such as exact pan/vocal-reduction behavior
- Confirm whether plugin-slot behavior in the JUCE mixer is real or still placeholder

### 7. CDG, Lyrics, and Second-Screen Display

Angular/Electron status:
- Rich lyric-display window
- Word-level sync
- Queue display in singer window
- Countdown behavior
- Ad rotation and idle visuals
- Animated ad panel transitions tied to song timeline:
	- Ads slide off-screen when singing starts.
	- Ads slide back in near song end to prepare the between-song state.
	- Lyric/canvas area resizes in sync with ad panel transitions.
- MP4 handling and overlays

Evidence:
- `src/app/components/lyric-display/lyric-display.component.ts`
- `EMOJI_CLEANUP_IMPLEMENTATION.md`

JUCE status:
- CDG decoder and lyric-display window are implemented
- Core second-window display exists
- Advanced Angular window behavior appears only partially matched

Evidence:
- `Source/CDG/CDGDecoder.cpp`
- `Source/UI/LyricDisplayComponent.cpp`
- `Source/UI/LyricDisplayWindow.cpp`

Gap classification:
- Partial migration

Remaining work:
- Compare singer-window queue display parity
- Port ad-rotation and idle-screen behavior if still required
- Port timeline-based ad panel animation behavior (slide-out on song start, slide-in near song end) with synchronized lyric area resizing.
- Confirm countdown and transition polish parity
- Confirm emoji-overlay or audience-reaction behavior if the desktop app still owns that surface
- Confirm MP4 second-screen behavior matches Angular

### 8. Home, Playlists, and Discovery Surfaces

Angular/Electron status:
- Playlist and discovery-oriented surfaces exist
- Shared-songs and featured content appear broader

JUCE status:
- Home page supports featured cards and playlists
- Playlist membership toggles exist in song edit
- Basic discovery surfaces are present

Evidence:
- `src/app/components/playlist`
- `Source/UI/HomePage.cpp`
- `Source/UI/SongEditDialog.cpp`

Gap classification:
- Mostly migrated for basic host workflows

Remaining work:
- Confirm parity for any Angular-only playlist management workflows
- Confirm whether shared-songs browsing is still missing from JUCE

### 9. Venue Management, Settings, and Session Operations

Angular/Electron status:
- Venue settings
- Venue code scheduling and management
- Session-data deletion
- Venue cleanup and operational controls
- License-related functionality

Evidence:
- `src/app/services/venue-code.service.ts`
- `src/app/services/license.service.ts`
- `src/app/services/venue.service.ts`

JUCE status:
- Settings page exists with venue info, queue rules, and session-management hooks
- ArchiveService exists
- Venue service exists
- Some advanced venue-code, license, and admin flows appear incomplete or not enforced

Evidence:
- `Source/UI/SettingsPage.cpp`
- `Source/Services/ArchiveService.cpp`
- `Source/Services/VenueService.cpp`
- `Source/Models/License.cpp`

Gap classification:
- Partial migration

Remaining work:
- Port venue-code scheduling and override behavior if still required
- Port license-enforcement behavior if Angular actively used it
- Port any missing session wipe and venue cleanup tools exposed to host/admin users
- Validate that all settings-backed workflows are wired through MainComponent and services

### 10. Audit, Archive, Analytics, and Reporting

Angular/Electron status:
- Audit tracking
- Archival flows
- Session statistics
- Analytics dashboards and business metrics

Evidence:
- `src/app/services/audit.service.ts`
- `src/app/services/archive.service.ts`
- `src/app/services/analytics.service.ts`
- `ARCHIVE_SYSTEM_DOCS.md`

JUCE status:
- ArchiveService exists
- Audit-related models exist
- Equivalent analytics/reporting surfaces are not clearly present

Evidence:
- `Source/Services/ArchiveService.cpp`
- `Source/Models/Audit.h`
- `Source/UI/SettingsPage.cpp`

Gap classification:
- Partial migration with likely missing analytics UI

Remaining work:
- Confirm archive parity end to end
- Port session-statistics views if missing
- Port analytics dashboards or reports still required by operators
- Distinguish mandatory host reporting from optional business intelligence

### 11. Admin, Enterprise, and User-Management Workflows

Angular/Electron status:
- Dedicated enterprise/admin management surface
- Role and user-management workflows
- Shared-song/admin operations

Evidence:
- `src/app/components/enterprise-admin-management`
- `src/app/services/user-venue.service.ts`

JUCE status:
- Models and some callbacks exist
- Full admin CRUD surface is not obvious from current UI

Evidence:
- `Source/UI/SettingsPage.h`
- `Source/Models/VenueUser.h`
- `Source/Models/User.h`

Gap classification:
- Partial or missing, depending on required scope

Remaining work:
- Decide whether full enterprise-admin tooling must live in JUCE
- If yes, port user invitation, role changes, venue association management, and any admin moderation tools
- If no, explicitly mark those workflows as intentionally staying in web/admin surfaces

### 12. Real-Time Infrastructure and Runtime Behavior

Angular/Electron status:
- Firebase listeners and web runtime patterns
- Mature recovery around live updates

JUCE status:
- Stable polling-based queue and request watchers exist
- Real-time Firebase listener layer appears declared but not active

Evidence:
- `Source/Firebase/FirebaseManager.h`
- `Source/Services/QueueService.cpp`
- `Source/Services/RequestService.cpp`

Gap classification:
- Functionally acceptable but architecturally partial

Remaining work:
- Decide whether polling is good enough for parity
- If not, complete the real-time-listener migration
- Do not prioritize this ahead of visible host-facing parity gaps unless latency is a product problem

### 13. Ribbon Quick-Access Control Surface

Angular/Electron status:
- Dedicated collapsible ribbon with 4 expandable boxes:
	- Background Music deck (toggle, transport, volume)
	- Lyrics preview panel
	- Next Up panel
	- Sound F/X pad with dedicated volume/mute
- Shared expansion state managed via a ribbon expansion service.

Evidence:
- `src/app/components/ribbon/ribbon.component.ts`
- `src/app/components/ribbon/ribbon.component.html`
- `src/app/services/ribbon-expansion.service.ts`

JUCE status:
- Equivalent controls are distributed across components (TopBar, BottomBar, QueueBar, lyric window) but there is no single ribbon-style, collapsible quick-control strip.
- Queue expansion exists, but it is not a full ribbon parity replacement.

Evidence:
- `Source/UI/TopBar.cpp`
- `Source/UI/BottomBar.cpp`
- `Source/UI/QueueBar.cpp`
- `Source/UI/MainComponent.cpp`

Gap classification:
- Partial migration with a missing unified UX surface.

Remaining work:
- Decide whether to implement a true ribbon component in JUCE or intentionally map ribbon features into existing bars with equivalent one-click workflows.
- Add a JUCE quick-access Sound F/X trigger surface if this remains in scope.
- Add collapsible/expandable quick-control behavior with persistent state if required for parity.
- Validate parity for background-music side deck behavior during karaoke playback transitions.

## Confirmed Completed Migration Work

These items should be treated as done, not backlog:
- JUCE native audio engine foundation
- JUCE CDG decoder and lyric-display window foundation
- JUCE queue and request-polling pipeline
- JUCE local library scanning and SQLite-backed search
- Firebase function recovery and codebase isolation described in `firebase/MIGRATION_RECOVERY_LOG.md`

## Phased Implementation Plan

This section expands the backlog into an execution order with dependencies, implementation steps, and exit criteria. Each phase should be completed and signed off before moving to the next phase unless noted otherwise.

### Phase 0: Alignment and Baseline (1 week)

Objective:
- Freeze migration scope and parity definition before feature work begins.

Dependencies:
- None.

Step-by-step execution:
1. Confirm scope decisions in Open Decisions (OAuth requirement, admin scope, ad/emoji scope, venue code/license scope).
2. Define parity labels for each feature area: Required for parity, Optional enhancement, Out of scope.
3. Convert this audit into tracked engineering tickets grouped by phase.
4. Establish a migration test matrix covering happy-path host flow, queue flow, request flow, and session close flow.
5. Capture current JUCE baseline metrics (startup time, queue refresh latency, scan/import timing, crash-free sessions).

Deliverables:
- Signed scope document.
- Ticketized backlog with owners.
- Baseline test matrix and metrics snapshot.

Exit criteria:
- Team agrees on required parity set and release gate definitions.

### Phase 1: Metadata and Host-Critical Parity (2 to 3 weeks)

Objective:
- Close the highest operational parity gap: cloud metadata lookup, enqueue fallback, and host-visible metadata state.

Dependencies:
- Phase 0 complete.
- Existing Firebase metadata functions operational.

Step-by-step execution:
1. Implement cloud read path in ApiService by normalized key.
2. Implement enqueue fallback on cloud miss with dedupe-safe behavior.
3. Add status propagation for metadata outcomes: local hit, cloud hit, queued, failed.
4. Wire SongEditDialog to display metadata states and retry actions.
5. Wire Library import/edit flows to apply cloud-assisted enrichment for misses.
6. Persist merged metadata back into local cache and SongDatabase.
7. Add startup background metadata sync trigger in MainComponent.
8. Add lightweight diagnostics logging for metadata pipeline events.

Primary implementation targets:
- `Source/Services/ApiService.h`
- `Source/Services/ApiService.cpp`
- `Source/UI/SongEditDialog.cpp`
- `Source/UI/LibraryPage.cpp`
- `Source/UI/MainComponent.cpp`
- `Source/Services/LibraryScanner.cpp`

Validation checklist:
- Metadata miss on machine A becomes a hit on machine B after sync window.
- UI surfaces pending/queued/failure state with no silent failure.
- Library scan remains responsive while metadata lookups occur.

Exit criteria:
- Metadata flow works end-to-end for host workflows and passes parity scenarios in the test matrix.

### Phase 2: Queue Integrity and Session Operations Parity (2 weeks)

Objective:
- Match Angular-grade queue stability, rotation correctness, and nightly host operations.

Dependencies:
- Phase 1 complete.

Step-by-step execution:
1. Perform side-by-side logic mapping between Angular queue rules and JUCE queue rules.
2. Port missing round-order and reorder edge-case behavior.
3. Add integrity checks for duplicate songs/order corruption and recovery paths.
4. Harden request approval flow under transient network failures.
5. Complete or verify session wipe and end-session archive operational tools.
6. Add operational alerts/messages in host UI for queue recovery actions.

Primary implementation targets:
- `Source/Services/QueueService.cpp`
- `Source/Services/RequestService.cpp`
- `Source/UI/QueueBar.cpp`
- `Source/Services/ArchiveService.cpp`
- `Source/UI/SettingsPage.cpp`

Validation checklist:
- Reorder, play-next, and play-now operations preserve deterministic order.
- Concurrent updates do not create duplicate queue entries.
- End-session flow archives and clears expected collections safely.

Exit criteria:
- Queue and session operations meet parity test coverage in normal and degraded network conditions.

### Phase 3: Auth and Access Parity (1 to 2 weeks)

Objective:
- Close remaining login and access-flow gaps required for production parity.

Dependencies:
- Phase 0 scope decision on OAuth requirement.

Step-by-step execution:
1. If required, implement OAuth desktop flow (provider redirect + local callback handling).
2. Align invitation/access-state UX with Angular flows (awaiting invitation, request access, denied states).
3. Add missing password reset/email verification UX if parity scope requires it.
4. Ensure venue-selection and association state transitions are robust across restarts.

Primary implementation targets:
- `Source/Services/FirestoreClient.h`
- `Source/Services/FirestoreClient.cpp`
- `Source/UI/LoginWindow.cpp`
- `Source/Auth/LoginFlowController.cpp`

Validation checklist:
- All approved auth paths can sign in and proceed to venue.
- Association edge states show actionable host UX.
- No auth path requires manual data patching to recover.

Exit criteria:
- Authentication flows satisfy the scoped parity rules and pass smoke tests.

### Phase 4: Admin, Venue Governance, and Reporting Parity (2 weeks)

Objective:
- Port the required operator/admin workflows and analytics visibility used in production.

Dependencies:
- Phase 0 decision on enterprise-admin scope.
- Phase 2 session operations stable.

Step-by-step execution:
1. Implement selected venue governance workflows (venue code scheduling, license checks) if in scope.
2. Port required admin user-role/invitation workflows or explicitly route to external admin surface.
3. Implement required session stats and analytics views for host operations.
4. Add shared-song admin upload/download workflows if confirmed in scope.
5. Add role-based UI gating for admin-only actions.

Primary implementation targets:
- `Source/UI/SettingsPage.cpp`
- `Source/Services/VenueService.cpp`
- `Source/Models/License.cpp`
- `Source/Models/VenueUser.h`
- `Source/Models/User.h`
- `Source/UI/MainComponent.cpp`

Validation checklist:
- Host/admin users can complete required operational tasks without web fallback (unless intentionally delegated).
- Reporting screens return consistent metrics with archived/live data.

Exit criteria:
- All in-scope admin and reporting workflows are either implemented in JUCE or formally delegated.

### Phase 5: Singer Display and UX Polish Parity (1 to 2 weeks)

Objective:
- Close second-screen and presentation parity gaps used during live shows.

Dependencies:
- Phase 2 stable queue behavior.
- Phase 0 decision on ads/emoji scope.

Step-by-step execution:
1. Compare Angular lyric/singer display behavior against JUCE component behavior.
2. Port in-scope ad rotation, idle states, and queue preview behavior.
3. Port timeline-based ad transitions (slide ads off when song starts, slide ads back in as song approaches end).
4. Port synchronized display resizing behavior used during ad transitions.
5. Port countdown and transition polish behaviors.
6. Implement Ribbon parity decision:
	- Option A: dedicated JUCE ribbon component with expandable boxes.
	- Option B: explicit parity mapping into TopBar/BottomBar/QueueBar with matching one-click workflows.
7. Add in-scope quick Sound F/X workflow and background music quick deck controls.
8. Validate MP4 and overlay behavior parity.
9. Run live rehearsal scenarios for show continuity.

Primary implementation targets:
- `Source/UI/LyricDisplayComponent.cpp`
- `Source/UI/LyricDisplayWindow.cpp`
- `Source/UI/MainComponent.cpp`
- `Source/CDG/CDGDecoder.cpp`
- `Source/UI/TopBar.cpp`
- `Source/UI/BottomBar.cpp`
- `Source/UI/QueueBar.cpp`

Validation checklist:
- Singer-facing display remains stable across track transitions.
- Countdown, queue preview, and optional overlays behave as expected.
- Ads remain hidden during active singing windows and reappear with animated transition before song end.
- Lyric/video render area resizes smoothly during ad panel transitions without visual tearing.
- Ribbon parity behaviors are available as either a dedicated component or a formally approved UX mapping.

Exit criteria:
- Show-time UX parity requirements are met in full-length rehearsal tests.

### Phase 6: Hardening, Release Readiness, and Cutover (1 week)

Objective:
- Finalize reliability and release readiness for migration cutover.

Dependencies:
- Phases 1 through 5 complete for in-scope items.

Step-by-step execution:
1. Run full regression on migration test matrix.
2. Decide whether to keep polling only or add real-time listeners as a post-cutover enhancement.
3. Triage and fix P0/P1 defects from rehearsal and beta venues.
4. Finalize operator runbook and rollback playbook.
5. Tag release candidate and run staged rollout.

Deliverables:
- Release candidate build.
- Migration completion report with known issues list.
- Venue rollout plan.

Exit criteria:
- Cutover approved with no unresolved P0 issues and acceptable P1 risk profile.

## Execution Order Summary

1. Phase 0 Alignment and Baseline
2. Phase 1 Metadata and Host-Critical Parity
3. Phase 2 Queue Integrity and Session Operations
4. Phase 3 Auth and Access Parity
5. Phase 4 Admin, Venue Governance, and Reporting
6. Phase 5 Singer Display and UX Polish
7. Phase 6 Hardening and Cutover

Parallelization guidance:
- Phase 3 may run partly in parallel with late Phase 2 after queue/session stability is demonstrated.
- Phase 5 may run partly in parallel with late Phase 4 if dependencies are isolated.
- Phase 6 starts only after all scoped parity items are either complete or explicitly deferred.

Definition of done for migration:
- All features marked Required for parity are implemented or explicitly delegated with approved owner.
- End-to-end host workflows pass regression in rehearsal conditions.
- Release readiness artifacts (runbook, rollback, known issues) are completed.

## Explicit Non-Backlog Items Unless Reconfirmed

These should not be automatically treated as migration requirements unless product direction says otherwise:
- Streaming or broadcast integrations
- Karaoke performance recording
- Full plugin-host architecture
- Any feature that appears only in old planning docs and not in shipped Angular code
- Mobile-native features that the desktop app does not own directly

## Recommended Immediate Next Slice (Week 1 of Execution)

Begin with Phase 0 plus the first three steps of Phase 1:
1. Confirm scope decisions and parity labels.
2. Implement cloud metadata read-by-key in ApiService.
3. Implement enqueue-on-miss fallback and status propagation to SongEditDialog.

Reason:
- This is the highest impact operational gap.
- Backend support is already in place.
- The work unblocks several downstream parity items.

## Open Decisions

- Should enterprise-admin management migrate into JUCE, or remain a separate admin/web surface
- Is OAuth a release blocker for the desktop host, or is email/password sufficient for current operations
- Are ad rotation and audience/emoji overlays still required for parity, or were they experimental
- Are venue-code scheduling and license enforcement still actively used in production

## Review Notes

This audit should be treated as a working migration backlog, not a final truth source. Before implementation starts on any domain marked partial, the team should do one source-level parity review between the Angular implementation and the owning JUCE class to avoid migrating features that were never actually used.
