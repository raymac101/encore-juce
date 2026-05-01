# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

JUCE is expected at `/Users/raymac/JUCE` (hardcoded in CMakeLists.txt). The build directory is `build/`.

```bash
# Configure (first time or after CMakeLists changes)
cmake -B build -S .

# Build
cmake --build build

# Run
open "build/EncoreJUCE_artefacts/Encore Karaoke.app"
```

Dependencies (macOS):
```bash
brew install rubberband   # pitch shifting
# SQLite3 ships with macOS
```

No test suite exists yet. There is no linter configured.

## Architecture Overview

**Encore Karaoke** is a desktop KJ (karaoke jockey) app migrated from Angular/Electron to C++/JUCE 7. It manages a live singer rotation queue, plays CDG+MP3 karaoke files, and syncs state with Firebase.

### Threading Model

All network I/O runs on `juce::Thread::launch` background threads. Results are always marshalled back to the JUCE message thread via `juce::MessageManager::callAsync` before touching any UI or shared state. The `juce::Component::SafePointer` pattern is used throughout to guard against component destruction during async callbacks.

### Firebase / Network Layer

Two parallel Firebase mechanisms exist:
- **`FirestoreClient`** (`Source/Services/`) — the live implementation. A synchronous REST client (must be called from a background thread). Handles auth (email/password via Identity Toolkit), Firestore CRUD, and collection list/query. All queue writes go through here.
- **`FirebaseManager`** (`Source/Firebase/`) — a stub for the Firebase C++ SDK, intended for real-time listeners. Not yet integrated into the active code paths.

`FirestoreClient` stores auth state (`idToken_`, `localId_`, `email_`) as instance fields and is a singleton. After sign-in, `getUserId()` returns the Firebase Auth UID, which must be used as the Firestore document ID when creating queue singer documents.

### Request Pipeline (Mobile → Queue)

1. TAGG mobile app writes a doc to `venues/{venueId}/requested` with `status="new"`
2. **`RequestService`** polls `/requested` every 3 s, dispatches on status transitions: `"new"` → `onNewRequest`, `"approved"` → `onApprovedRequest`, `"rejected"` → `onRejectedRequest`
3. **`MainComponent::onIncomingNewRequest`** runs auto-approve checks (queue closed, max songs per singer, repeat policy) then calls `QueueService::appendSong` directly
4. On success, `deleteRequested` removes the doc — do NOT patch to `"approved"` first or the next poll will double-fire `onApprovedRequest`
5. **`QueueService::appendSong`** either PATCHes the existing singer doc's `songs` array, or creates a new doc using `profileId` (the auth UID) as the document ID

### Queue Data Model (Firestore)

```
venues/{venueId}/queue/{authUid}   ← singer document
  name, avatar, deviceId, profileId, order, rotationOrder,
  strikes, songsPerformed, status, songs[]

songs[] element = QueueItem:
  id, singerName, songId, song (name), artist, songVersion,
  duration, order, songOrder, pitch, status, time, ...
```

The document ID **must** be the singer's Firebase Auth UID (`item.profileId` or `FirestoreClient::getUserId()`). Never use Firestore auto-IDs for queue singer docs.

`QueueService` is a singleton with two independent responsibilities:
- One-shot writes: `appendSong`, `removeSong`, `deleteSinger`, `patchSingerSongs`
- Live watcher: `startWatching` / `stopWatching` polls `/queue` every 3 s and fires `onChange(Snapshot)` only when the fingerprint changes

### UI Structure

`MainComponent` (root) owns:
- **`TopBar`** — venue name, connection status, audio level meter
- **`NavBar`** — page switcher (Home / Search / Library / Settings)
- **`MainArea`** — page host; contains `HomePage`, `SearchPage`, `LibraryPage`, `SettingsPage`
- **`QueueBar`** — right-side singer rotation; drag-to-reorder, 3-dot context menus, countdown timer, add-singer button
- **`BottomBar`** — transport controls, pitch/tempo, waveform
- **`LyricDisplayWindow`** — separate window for the singer-facing display (CDG rendering)

`MainArea::onSongSelected` → `SongSelectionDialog::launch` → `MainComponent::onSongSelectionResult` handles all song-click flows from both Home and Search pages.

`MainComponent` holds `activeVenueId_` and `localNowPlaying_` as the source of truth for current state. `setVenueId()` starts both `RequestService` and `QueueService` watchers.

### Audio Engine

`AudioEngine` signal chain:
```
AudioFormatReaderSource → AudioTransportSource → ResamplingAudioSource
  → PitchShifter (RubberBand) → master gain → juce::Reverb → echo delay → output
```

`PitchShifter` wraps the RubberBand library for real-time pitch and tempo adjustment. `updatePlaybackPosition()` runs each audio callback; when it detects end-of-file it calls `stop()` then fires `onSongFinished` via `callAsync` — wire this in `MainComponent` to drive autoplay/countdown.

### Local Song Library

`LibraryScanner` (a `juce::Thread`) recursively scans directories for `.cdg`, `.zip`, `.mp4`, `.m4a`, `.xml` files, parses filenames to extract artist/song/vendor code, and writes results to a `SongDatabase` (SQLite with FTS5). `LibraryPage` owns both objects and wires them at construction. The `SongDatabase` must be opened (`open()`) and its pointer passed to `scanner_.setSongDatabase()` before scanning.

### Localization

`LocalizationManager` loads `Resources/Languages/{locale}.txt` (key=value format). Call `lm.getText("key")` anywhere. When adding UI text, add the key to `en_US.txt` first; other locale files fall back to English for missing keys.

### Key Patterns

- **Firestore field encoding**: always use `FirestoreClient::stringValue()`, `::integerValue()`, `::doubleValue()`, `::booleanValue()` helpers when building field objects for PATCH/POST. Raw `juce::var` values will be rejected by the REST API.
- **Queue doc relative paths**: `relPathFromDocName()` in `QueueService.cpp` strips the `projects/.../documents/` prefix from a Firestore doc name to get the relative path used by `patchDocument`.
- **Singer doc lookup**: always done by case-insensitive `singerName` match via `findSingerByName()` in `QueueService.cpp` — Firestore doc IDs are auth UIDs but the lookup key is the display name.
- **`isNewlyAdded`**: transient flag on `Singers`, set true when a singer is first inserted locally. `SingerRow::paint()` renders a yellow-green highlight; cleared after the next watcher reload.
