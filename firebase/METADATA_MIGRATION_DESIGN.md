# Encore JUCE Metadata Migration: Execution Checklist & Design

## Phase 1: Canonical Cloud Metadata
- Define normalized key spec shared by client and backend.
- Create Firestore collection metadataSongs keyed by normalizedKey.
- Create importer job to load your existing metadata JSON into metadataSongs.
- Add fields: artistName, songName, imageUrl, durationMS, tempo, keySignature, releaseDate, genres, updatedAt, fetchedAt, source, confidence, payloadHash.
- Add aliases strategy for common variants and swapped artist/title edge cases.
- Add Firestore indexes for updatedAt and popularityScore.
- Done when imported row count matches expected and random spot checks pass.

## Phase 2: Queue and Spotify Worker
- Create Cloud Function enqueueMetadataFetch.
- enqueueMetadataFetch validates input, computes normalizedKey, dedupes, writes metadataFetchQueue only if missing or stale.
- Create scheduled Cloud Function processMetadataQueue.
- processMetadataQueue enforces quota budget, calls Spotify, upserts metadataSongs, marks queue status.
- Add retry policy with exponential backoff and max retries.
- Add quota document metadataQuota/daily with usedCalls and resetAt.
- Done when queue can process end to end without exceeding daily cap.

## Phase 3: Snapshot and Delta Distribution
- Create scheduled Cloud Function exportMetadataSnapshot.
- exportMetadataSnapshot writes metadata_snapshot.json to Firebase Storage.
- Create delta file generation metadata_delta_YYYYMMDDHH.json based on updatedAt.
- Store snapshot manifest with version, hash, generatedAt.
- Done when snapshot and delta files are generated and verifiable.

## Phase 4: JUCE Client Integration
- **ApiService:**
  - Keep local cache read first.
  - Add Firestore metadata read by normalizedKey.
  - Add enqueue call on cloud miss.
  - Keep callback shape unchanged for existing UI paths.
- **LibraryScanner:**
  - Continue local apply for fast library enrich.
  - Add optional cloud-assisted enrich pass for unmatched songs.
  - Save enriched results back to songbook and local cache.
- **MainComponent:**
  - Wire startup metadata delta sync.
  - Trigger silent background sync without blocking UI.
- **SongEditDialog:**
  - On Get Metadata, resolve local then cloud then enqueue.
  - Show Pending status if queued instead of silent failure.
- **UserPreferences:**
  - Add lastMetadataSyncAt and snapshotVersion fields.
- Done when a miss on one machine becomes a hit on another after sync window.

## Phase 5: Security and Governance
- Firestore rules: clients can read metadataSongs.
- Firestore rules: clients cannot write metadataSongs directly.
- Clients can submit fetch requests only through authenticated function endpoint.
- Enable App Check and request throttling.
- Service account for worker has least-privilege write scope.
- Done when rule tests pass and direct client write attempts are rejected.

## Phase 6: Observability
- Log event types: local_hit, cloud_hit, enqueued, fetched, failed, throttled.
- Dashboard metrics:
  - local cache hit rate
  - cloud hit rate
  - spotify calls used today
  - queue depth
  - oldest queued age
  - fetch failure rate
- Alerts:
  - quota above 85 percent
  - queue oldest age above threshold
  - failure rate spike
- Done when alerts fire correctly in a test window.

## JUCE Class-by-Class Task Map
- **ApiService:**
  - add getCloudMetadataByKey
  - add enqueueMetadataFetch
  - merge cloud result into local shared cache
  - expose reason codes in callback messages
- **LibraryScanner:**
  - split metadata enrich into local pass and optional cloud pass
  - add unmatched key batch extraction helper
  - persist merged results to songbook
- **MainComponent:**
  - add startup call startMetadataBackgroundSync
  - attach progress callback for non-blocking diagnostics
- **SongEditDialog:**
  - show cache/cloud/pending states
  - add retry action for pending misses
- **UserPreferences:**
  - persist lastMetadataSyncAt and snapshot manifest hash

## Firebase Function-by-Function Task Map
- **enqueueMetadataFetch:**
  - input validation
  - normalized key generation
  - dedupe and stale check
  - queue insert with priority
- **processMetadataQueue:**
  - pull batch by priority
  - enforce daily and per-minute budget
  - spotify fetch and transform
  - metadataSongs upsert
  - queue state transition
- **exportMetadataSnapshot:**
  - export full snapshot
  - export deltas
  - update manifest

## Acceptance Test Checklist
- New song metadata fetched on device A appears on device B after sync.
- Same missing song requested by 10 users creates one queue job.
- Daily Spotify cap never exceeded.
- Offline startup still enriches from local cache.
- Library scan remains responsive with large songbooks.
- Metadata conflicts resolve deterministically by updatedAt and confidence.

## Suggested Delivery Sprints
- **Sprint 1:**
  - Firestore schema
  - importer
  - enqueue function
- **Sprint 2:**
  - worker
  - quota controls
  - metrics
- **Sprint 3:**
  - JUCE ApiService integration
  - SongEditDialog pending UX
  - startup delta sync
- **Sprint 4:**
  - snapshot export
  - rollout
  - hardening
