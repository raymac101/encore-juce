# Sprint 1 Metadata Backend (JUCE)

This folder contains the Sprint 1 backend scaffolding for global metadata sharing:

- Firestore schema via collection conventions (`metadataSongs`, `metadataFetchQueue`)
- Firestore security rules and indexes
- Callable endpoint `enqueueMetadataFetch`
- Import script for seeding `metadataSongs` from `meta_data.json`

## 1) Prerequisites

- Firebase CLI authenticated to your project
- A service account or ADC credentials with Firestore write access
- Node.js 20+

## 2) Install

Run from `firebase/functions`:

```bash
npm install
```

## 3) Seed metadataSongs from local meta_data.json

Example:

```bash
npm run import:metadata -- ../../assets/data/meta_data.json
```

The importer expects a JSON object keyed by document IDs, with values containing at least:

- `artistName`
- `songName`

## 4) Deploy functions + Firestore config

Run from `firebase/functions`:

```bash
npm run deploy
```

## 5) Callable contract: enqueueMetadataFetch

Input payload:

```json
{
  "artistName": "Artist",
  "songName": "Song",
  "normalizedKey": "optional override",
  "appVersion": "optional",
  "priority": 50
}
```

Behavior:

- Returns `already_fresh` when canonical metadata is recent
- Dedupes queue by `normalizedKey`
- Creates or refreshes queue document in `metadataFetchQueue`

## 6) Next Sprint

- Add `processMetadataQueue` worker with Spotify quota enforcement
- Add snapshot export and delta feeds
- Wire JUCE client to callable enqueue and cloud lookup path
