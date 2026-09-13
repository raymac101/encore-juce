#!/usr/bin/env node
// Uploads a local file to the project's default Firebase Storage bucket at
// an explicit object name. Used both for the promoted manifest
// (Installers/manifest.json -- see promote-release.sh) and for the release
// installer binaries themselves (Installers/EncoreKaraoke-*-win64.exe /
// -mac.dmg -- see .github/workflows/release.yml). Neither of those was ever
// reached by `firebase deploy --only hosting` alone: Hosting and Storage are
// different products/URLs, and UpdateService.cpp's manifest/installer
// download URLs are all Storage URLs.
//
// No npm dependencies: signs the service-account JWT with Node's built-in
// crypto module and uploads with the built-in fetch (Node 18+), the same
// OAuth2 service-account flow every Google Cloud client library uses under
// the hood.
//
// Usage:
//   GOOGLE_APPLICATION_CREDENTIALS=/path/to/key.json \
//     node scripts/upload-to-storage.cjs <local file path> <bucket object name> [content-type]
//
// Example:
//   node scripts/upload-to-storage.cjs dist/EncoreKaraoke-1.2.3-win64.exe \
//     "Installers/EncoreKaraoke-1.2.3-win64.exe" application/octet-stream

const fs = require ('fs');
const crypto = require ('crypto');

const BUCKET = 'tagg-9ee2b.appspot.com';

function base64url (input)
{
    return Buffer.from (input).toString ('base64')
        .replace (/\+/g, '-')
        .replace (/\//g, '_')
        .replace (/=+$/, '');
}

async function getAccessToken (serviceAccount)
{
    const header = { alg: 'RS256', typ: 'JWT' };
    const now = Math.floor (Date.now() / 1000);
    const claims = {
        iss: serviceAccount.client_email,
        scope: 'https://www.googleapis.com/auth/devstorage.read_write',
        aud: 'https://oauth2.googleapis.com/token',
        iat: now,
        exp: now + 3600,
    };

    const unsigned = `${base64url (JSON.stringify (header))}.${base64url (JSON.stringify (claims))}`;
    const signature = crypto.sign ('RSA-SHA256', Buffer.from (unsigned), serviceAccount.private_key);
    const jwt = `${unsigned}.${base64url (signature)}`;

    const resp = await fetch ('https://oauth2.googleapis.com/token', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: new URLSearchParams ({
            grant_type: 'urn:ietf:params:oauth:grant-type:jwt-bearer',
            assertion: jwt,
        }),
    });

    const json = await resp.json();
    if (! resp.ok)
        throw new Error (`Token exchange failed: ${JSON.stringify (json)}`);

    return json.access_token;
}

async function main()
{
    const keyPath = process.env.GOOGLE_APPLICATION_CREDENTIALS;
    const localPath = process.argv[2];
    const objectName = process.argv[3];
    const contentType = process.argv[4] || 'application/octet-stream';

    if (! keyPath || ! localPath || ! objectName)
    {
        console.error ('usage: GOOGLE_APPLICATION_CREDENTIALS=<key.json> node upload-to-storage.cjs <local file> <bucket object name> [content-type]');
        process.exit (1);
    }

    const serviceAccount = JSON.parse (fs.readFileSync (keyPath, 'utf8'));
    const body = fs.readFileSync (localPath);

    const token = await getAccessToken (serviceAccount);

    const uploadUrl = `https://storage.googleapis.com/upload/storage/v1/b/${encodeURIComponent (BUCKET)}/o`
        + `?uploadType=media&name=${encodeURIComponent (objectName)}`;

    const resp = await fetch (uploadUrl, {
        method: 'POST',
        headers: {
            Authorization: `Bearer ${token}`,
            'Content-Type': contentType,
        },
        body,
    });

    const json = await resp.json();
    if (! resp.ok)
    {
        console.error ('Upload failed:', JSON.stringify (json, null, 2));
        process.exit (1);
    }

    console.log (`Uploaded ${objectName} to gs://${BUCKET} (generation ${json.generation}, size ${json.size} bytes)`);
}

main().catch ((err) =>
{
    console.error (err);
    process.exit (1);
});
