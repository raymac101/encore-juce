#!/usr/bin/env node
// Uploads the promoted manifest to Firebase Storage's Installers/manifest.json
// object -- the actual URL UpdateService.cpp's kManifestUrl reads from at
// launch. This is a DIFFERENT product/URL than Firebase Hosting: `firebase
// deploy --only hosting` (what promote-release.sh already did) never touches
// it, which is why past promotions never actually reached a running app's
// auto-update check on their own -- that gap was papered over by manually
// re-uploading this same file through the Firebase console.
//
// No npm dependencies: signs the service-account JWT with Node's built-in
// crypto module and uploads with the built-in fetch (Node 18+), the same
// OAuth2 service-account flow every Google Cloud client library uses under
// the hood.
//
// Usage:
//   GOOGLE_APPLICATION_CREDENTIALS=/path/to/key.json \
//     node scripts/upload-manifest-to-storage.cjs hosting/encore/manifest.json

const fs = require ('fs');
const crypto = require ('crypto');

const BUCKET = 'tagg-9ee2b.appspot.com';
const OBJECT_NAME = 'Installers/manifest.json';

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
    const manifestPath = process.argv[2];

    if (! keyPath || ! manifestPath)
    {
        console.error ('usage: GOOGLE_APPLICATION_CREDENTIALS=<key.json> node upload-manifest-to-storage.cjs <manifest.json path>');
        process.exit (1);
    }

    const serviceAccount = JSON.parse (fs.readFileSync (keyPath, 'utf8'));
    const manifestBody = fs.readFileSync (manifestPath);

    const token = await getAccessToken (serviceAccount);

    const uploadUrl = `https://storage.googleapis.com/upload/storage/v1/b/${encodeURIComponent (BUCKET)}/o`
        + `?uploadType=media&name=${encodeURIComponent (OBJECT_NAME)}`;

    const resp = await fetch (uploadUrl, {
        method: 'POST',
        headers: {
            Authorization: `Bearer ${token}`,
            'Content-Type': 'application/json',
        },
        body: manifestBody,
    });

    const json = await resp.json();
    if (! resp.ok)
    {
        console.error ('Upload failed:', JSON.stringify (json, null, 2));
        process.exit (1);
    }

    console.log (`Uploaded ${OBJECT_NAME} to gs://${BUCKET} (generation ${json.generation})`);
}

main().catch ((err) =>
{
    console.error (err);
    process.exit (1);
});
