# Venue Marketing Email — Implementation Plan

Status: DRAFT v3, 2026-10-01. Decisions from the product owner are recorded in section 2. Remaining open questions are in section 13.

## 1. Goal

Let a Venue Admin or Company Admin send branded marketing emails (images, text, links, buttons) to TAGG users who joined their venue, with:

- A per-venue audience that only contains people who explicitly opted in.
- Working unsubscribe per venue and globally, enforced on every send.
- Reporting: TAGG users who joined, opt-ins, unsubscribes, delivered, bounced, clicks per link, complaints.
- Sending through the venue's or company's **own** email service account, such as SendGrid. Encore does not send marketing email itself.

Composing and reporting happen in a new standalone web app, the **Encore Venue Portal**, not inside the Encore desktop app.

## 2. Decisions

| # | Topic | Decision |
|---|---|---|
| 1 | Consent | Opt-in only, for every country. |
| 2 | Email provider | **Each venue or company must connect its own email service account** (SendGrid or similar) before it can send. There is no Encore platform sender. prxy, the company mail host, is not used. |
| 3 | Audience | Anyone who joins the venue in the TAGG app, not only people who sang. |
| 4 | What venues see | The customer list belongs to the venue. Venue and Company Admins can view and export the email addresses of people who **opted in** to that venue. People who never opted in appear only as counts and stage names. |
| 5 | Editor | A standalone web app at **viracicom.com/emailserver**, outside Encore. No embedded web view in the JUCE app. |
| 6 | Limits | Encore sets no send caps. The venue's own provider plan sets its limits. |
| 7 | Pricing | Free for now. The venue pays its own email provider. Track usage per venue so Encore billing can be added later. |
| 8 | Who can send | Venue Admin for their venue. Company Admin for any or all venues in their company. |

## 3. What exists today and what it means

| Existing piece | Location | Implication |
|---|---|---|
| TAGG venue join | `tagg3/app/screens/main/DiscoverScreen.tsx` `handleJoin()`: user types the venue's 6-character code | **The join itself is stored only on the phone** (zustand + AsyncStorage). A new server-side join record is required going forward. |
| TAGG user profile | `users/{uid}` written in `RegisterExtScreen.tsx` with `email`, `stageName`, `city`, `notify` | **No venue field.** A read-only scan of 1,000 profiles on 2026-10-01 found no signup-venue or home-venue field. Stage names for venue reports come from here. Email at send time comes from Firebase Auth. |
| TAGG song history | `users/{uid}/userAudit/{id}`, one doc per song sung, with `venueId`, `venueName`, `date` | **Main backfill source.** Links past users to venues they sang at. |
| TAGG per-venue favorites | `users/{uid}/venue-data/{venueId}/favourites` | Secondary backfill source. Shows the user joined that venue in TAGG. |
| Per-venue singer roster | `venues/{venueId}/members/{singerId}` from `performanceEvents.js` | Optional extra signal (visit count, last sang). Not the audience source. |
| Firebase Auth | Auth | Email lookup at send time with Admin SDK `getUsers()`, 100 per call. Skip `disabled` accounts. |
| Functions module pattern | `adminUsers.js`, `venueMembers.js` | New `marketingEmail.js` follows the same `(admin, db) => {...}` shape and the server-side role checks. |
| Company roles | `companies/{companyId}/members/{uid}.role == "company_admin"`; venue `companyId` field | Reused for Company Admin authorization. |
| Venue roles | `user-venue-lookup/{uid}_{venueId}.role` in `Admin`, `Tester`, `EnterpriseAdmin` | Reused for Venue Admin authorization. |
| Transactional mail | Trigger Email extension via `mail` collection | Unchanged. Keeps handling invites and password resets. Marketing never uses it. |
| Hosting | Repo-root `firebase.json` serves `hosting/` | The portal becomes a **second Hosting site**. The existing site is not changed. |

**Backfill size, measured 2026-10-01 (read-only):**

| Measure | Count |
|---|---|
| TAGG user profiles | 1,205 |
| Users linked to at least one venue | 676 |
| User–venue pairs | 756 |
| Pairs found only through favorites | 12 |
| Venues with linked users | 21 |
| Largest venue audiences | 420, 142, 62, 23, 22 |

About 530 users have no venue link: they registered but never sang or saved a favorite. They join an audience only when they next join a venue. Backfilled users are **not subscribed**. They're asked in the app (section 4.1), because consent is opt-in only.

### Security issues that block this feature as-is

1. **`firestore.rules`: `venues/{venueId}/{document=**}` allows read and write to any signed-in user.** Firestore rules are OR'd, so a narrower rule below it cannot take access away. → All marketing data lives in **new top-level collections** that deny all client access.
2. **`firestore.rules`: `users/{userId}` allows any signed-in user to read and write every user doc,** including `email`. Consent must not live there, or anyone could change a stranger's consent. → Consent is written only by a callable function into a server-only collection. Tightening `users/` is recommended as separate work.
3. **`storage.rules` is fully open** under the 2026-07-18 stopgap. → Campaign images go through an upload function. The bucket rule is left alone, per the stopgap note.

## 4. Consent and audience

### 4.1 Capturing consent in TAGG

After a successful join in `handleJoin()`, TAGG calls a new `recordVenueJoin` function. If the user has never answered for this venue, TAGG shows a one-time prompt:

> **Get emails from {Venue Name}?**
> Specials, theme nights and events. {Venue Name} will receive your email address. Unsubscribe any time.
> [Yes, sign me up] [No thanks]

The sentence about the venue receiving the address is required, because opted-in addresses are visible to and exportable by the venue, and pass through the venue's own email provider.

- The answer is stored per venue, with timestamp, app version and the exact prompt text version. This is the consent record.
- The prompt is never shown again for that venue unless the user changes it in settings.
- A new **Settings › Venue Emails** screen lists every venue the user joined with a toggle each, plus "Never email me from any venue".
- **Existing users:** the backfill adds their past venues to `venueVisitors` with no consent. On the first app launch after the update, TAGG shows one prompt: "Want emails from venues you've sung at?" It lists each venue with a checkbox, unchecked by default. Each checked venue is recorded as consent.
- We do **not** email existing users asking for consent. Under Canadian law a consent-request email is itself a commercial message.

### 4.2 Legal and deliverability requirements

- Every email includes the venue name, the venue's physical address, a one-click unsubscribe link and an honest subject line.
- Unsubscribes take effect before the next send.
- The venue's sending domain must have SPF, DKIM and DMARC. Emails carry the RFC 8058 `List-Unsubscribe` and `List-Unsubscribe-Post: List-Unsubscribe=One-Click` headers that Gmail and Yahoo require for bulk senders.
- TAGG and Encore privacy policies get updated to describe venue emails and opt-out.
- Venues must accept **Venue Email Terms** in the portal before connecting a provider (section 6.4).

## 5. Data model

All collections below deny all client reads and writes. Only Cloud Functions touch them.

```
venueVisitors/{venueId}_{uid}
  venueId, uid, companyId, stageName (cached)
  firstJoinedAt, lastJoinedAt, joinCount, songsSung
  source: "tagg_join" | "backfill_audit" | "backfill_favourites"
  consentAsked: bool

marketingSubscribers/{venueId}_{uid}
  venueId, companyId, uid
  status: "subscribed" | "declined" | "unsubscribed" | "bounced" | "complained"
  consentAt, consentSource: "tagg_join_prompt" | "tagg_settings"
  consentTextVersion, appVersion
  unsubscribedAt, unsubscribeSource: "link" | "one_click" | "tagg_settings" | "provider"

marketingGlobalPrefs/{uid}
  noVenueEmail: bool, updatedAt

emailSuppression/{sha256(lowercase email)}
  reason: "unsubscribe_all" | "hard_bounce" | "complaint", at, venueId?

emailConnections/{connectionId}
  ownerType: "company" | "venue", ownerId
  provider: "sendgrid" | "smtp" | "mailgun" | "ses" | "postmark" | "brevo"
  fromName, fromEmail, replyTo, sendingDomain
  encryptedCredentials (Cloud KMS envelope; never returned to any client)
  webhookSecret, status: "incomplete" | "verified" | "failing" | "paused"
  lastTestAt, lastWebhookAt, dnsCheck: { spf, dkim, dmarc, checkedAt }
  termsAcceptedBy, termsAcceptedAt, termsVersion

marketingCampaigns/{campaignId}
  scope: "venue" | "company", companyId, venueIds[]
  createdBy, connectionId
  status: "draft" | "scheduled" | "sending" | "sent" | "cancelled" | "failed"
  subject, preheader, design (editor project JSON), mjml, html, text
  audienceFilter: { joinedWithinDays?, minJoins? }
  scheduledAt, sentAt
  stats: { targeted, sent, delivered, bounced, opened, uniqueOpens, clicked, uniqueClicks, unsubscribed, complained }

marketingCampaigns/{campaignId}/recipients/{uid}
  status, providerMessageId, deliveredAt, firstOpenAt, firstClickAt

marketingCampaigns/{campaignId}/links/{linkId}
  url, label, clicks, uniqueClicks

marketingUsage/{ownerId}_{yyyy-mm}
  emailsSent, campaignsSent, provider

marketingDailyStats/{venueId}_{yyyy-mm-dd}
  joins, optIns, declines, unsubscribes, subscribedTotal

marketingExports/{exportId}
  ownerId, venueIds[], by, at, rowCount
```

## 6. Email providers (venue or company owned)

### 6.1 Connection is required

- A venue cannot schedule or send a campaign until it has a **verified** connection. It can still build drafts and see its audience before then.
- A connection can belong to a company or a venue. A company connection is used by all its venues unless a venue connects its own. This lets a company with several venues set up one domain once.
- The venue pays its provider directly. Encore never holds billing for email.
- Recipient addresses pass through the venue's provider account. That's consistent with decision 4, and the consent prompt says so.

### 6.2 Supported providers

| Provider | How | Phase | Delivery/bounce reports |
|---|---|---|---|
| **SendGrid** | Web API with API key | 2 | Yes, through its signed event webhook |
| **SMTP relay of a bulk email service** (Mailgun, Brevo, Amazon SES SMTP, SendGrid SMTP, etc.) | Nodemailer | 2 | No. Shown as "not available from this provider" |
| Mailgun, Amazon SES, Brevo, Postmark | Native APIs with webhooks | 5 | Yes |

Clicks, unsubscribes and estimated opens are tracked by **our own** links (section 7), so those reports work the same with every provider.

Ordinary mailboxes are **blocked** as senders. That covers free addresses like gmail.com, yahoo.com, outlook.com, hotmail.com, icloud.com and aol.com, and SMTP hosts on a known mailbox-host list. Gmail and Yahoo reject or junk bulk mail that claims to come from their domains through another service. Mailbox hosts also cap sending and usually forbid marketing.

All providers sit behind one adapter in `firebase/functions/marketing/providers/`, so adding one never touches campaign code:

```
sendBatch(connection, messages[]) -> [{ uid, providerMessageId | error }]
verifyWebhook(connection, request) -> bool
parseEvents(request) -> [{ type, providerMessageId, email, at, url? }]
testConnection(connection) -> { ok, detail }
```

### 6.3 Setup wizard in the portal

**Settings › Email Provider** walks the admin through setup, with a step-by-step guide for SendGrid:

1. **Accept the Venue Email Terms.**
2. **Choose a provider** and create an account there if needed. The wizard links to the signup page.
3. **Choose a sending address on a domain the venue owns,** e.g. `news@rustymic.com`. The wizard explains that a free email address won't work.
4. **Authenticate the domain at the provider.** For SendGrid this is "Domain Authentication", which gives DNS records to add. The wizard checks the domain's SPF, DKIM and DMARC itself and explains each failure in plain words. If DMARC is missing, it shows the exact record to add.
5. **Paste an API key** (for SendGrid, a key limited to Mail Send) or SMTP credentials. These are encrypted with Cloud KMS right away and can never be viewed again, only replaced. The portal shows "Connected: SendGrid, news@rustymic.com".
6. **Set up delivery reports** (webhook providers only). The wizard shows this connection's unique webhook URL and which events to tick. It turns green when the first event arrives.
7. **Send a test email** to the admin. Passing all steps marks the connection **verified**.

The connection is re-checked nightly. If the key stops working or DNS breaks, the status becomes `failing`, the admins get a notice in the portal, and scheduled campaigns are held.

### 6.4 Venue-owned customer lists

- The portal's Audience screen shows name, email, opt-in date and status for opted-in subscribers. **Export to CSV** covers the same people.
- Every export also includes the venue's unsubscribed list, so the venue can suppress them in any other tool it uses.
- The Venue Email Terms make the venue responsible for honoring unsubscribes and the law when it emails addresses outside Encore, including through its own provider directly.
- Each export is logged in `marketingExports`.
- Customers who later unsubscribe are removed from future exports. Addresses already exported can't be recalled, which the Terms cover.

## 7. Cloud Functions (`firebase/functions/marketingEmail.js` and `marketing/`)

| Function | Type | Called by | Purpose |
|---|---|---|---|
| `recordVenueJoin` | callable | TAGG | Upsert `venueVisitors`. Returns whether to show the consent prompt. |
| `getPendingVenueConsents` | callable | TAGG | Backfilled venues the user hasn't answered for yet, for the one-time existing-user prompt. |
| `setVenueEmailConsent` | callable | TAGG | Record yes/no for one or more venues with consent metadata. |
| `getMyVenueEmailPrefs` / `setGlobalNoVenueEmail` | callable | TAGG | Settings screen. |
| `onUserAuditCreate` | Firestore trigger on `users/{uid}/userAudit/{id}` | internal | Keeps `songsSung` and `lastJoinedAt` current from new song history. |
| `backfillVenueVisitors` | script in `functions/scripts/` | operator | One-time pass over all `userAudit` and `favourites` docs to build `venueVisitors`. Idempotent, with a dry-run mode. |
| `createPortalSession` | callable | Encore | Mints a short-lived custom token so the Venue Portal button signs the user in without retyping a password. |
| `getPortalContext` | callable | Portal | The venues and companies this user can manage, with roles and connection status. |
| `getAudienceSummary` | callable | Portal | Joined, subscribed, declined, unsubscribed, bounced. Growth series. Paginated list: stage names for everyone, plus email for opted-in subscribers. |
| `exportSubscribers` | callable | Portal | CSV of opted-in subscribers plus the unsubscribed list. Logged. |
| `acceptEmailTerms` | callable | Portal | Records terms acceptance on the connection. |
| `saveEmailConnection` / `deleteEmailConnection` | callable | Portal | Stores provider settings. Encrypts credentials with KMS. |
| `checkEmailConnection` | callable + nightly scheduler | Portal / internal | DNS checks (SPF, DKIM, DMARC), credential check, free-mail and mailbox-host blocklist, webhook-seen check. Sets status. |
| `saveCampaign` / `listCampaigns` / `getCampaign` / `deleteCampaign` | callable | Portal | Draft CRUD. Server compiles MJML to HTML, sanitizes, and injects the locked footer. |
| `uploadCampaignImage` | callable (signed upload URL) | Portal | Resize and compress with sharp. Stored under `marketing/{ownerId}/images/`, served by public URL. |
| `sendTestEmail` | callable | Portal | Send the draft to the caller's own address through the venue's connection. |
| `scheduleCampaign` / `cancelCampaign` | callable | Portal | Requires a verified connection. Validates content, footer and consent scope. |
| `dispatchCampaign` | Cloud Tasks worker | internal | Pages subscribers 500 at a time. Resolves emails from Auth. Filters suppression, global opt-out and disabled accounts. Dedupes across venues for company campaigns. Rewrites links and sends through the adapter at a rate the provider accepts. Idempotent per campaign and user, so retries never double-send. Stops cleanly if the provider reports a quota error and resumes later. |
| `runScheduledCampaigns` | scheduler, every 5 min | internal | Enqueues campaigns that are due. |
| `unsubscribe` | HTTPS GET + POST | recipient | Signed token, no login needed. GET shows a confirmation page with "unsubscribe from all venues". POST handles one-click. For a company campaign, unsubscribes from every venue the campaign targeted. |
| `trackClick` | HTTPS `/r/...` | recipient | Records the click and redirects. Only redirects to URLs stored on that campaign, so it can't be abused as an open redirect. |
| `trackOpen` | HTTPS `/o/...` pixel | recipient | Estimated opens. Labelled "estimated" because Apple Mail pre-loads images. |
| `providerWebhook` | HTTPS `/hooks/{connectionId}` | provider | Verify signature (SendGrid signs events). Record delivered, bounce and complaint. Hard bounces and complaints go to `emailSuppression`. |
| `rollupDailyStats` | scheduler, nightly | internal | Fills `marketingDailyStats` and `marketingUsage`. |

**Authorization**, re-derived on the server for every call:
- Venue Admin: an active `user-venue-lookup/{uid}_{venueId}` with role `Admin`, `Tester` or `EnterpriseAdmin`.
- Company Admin: `companies/{companyId}/members/{uid}.role == "company_admin"`, and every target venue's `companyId` matches.
- EnterpriseAdmin: all venues, plus pausing senders.

## 8. Encore Venue Portal (standalone web app)

The portal uses the same Firebase project and the same email/password accounts, so admins sign in with their existing Encore login. It works from any browser, including a phone.

| Aspect | Choice |
|---|---|
| Location | `web/venue-portal/` in this repo |
| Stack | React, TypeScript, Vite, Firebase JS SDK (Auth + callable Functions) |
| Hosting | New Firebase Hosting site, served at **viracicom.com/emailserver** (see below). Configured as a second site target so the existing installer hosting is untouched. |
| Email editor | **GrapesJS** with the **grapesjs-mjml** plugin. Open source, drag-and-drop, outputs MJML, which compiles to email that renders in Gmail, Outlook and iPhone Mail. *Alternative:* Unlayer, which is more polished but commercial and shows its branding on the free tier. |
| Sign-in from Encore | A Venue Portal button in Encore calls `createPortalSession`. Encore then opens the portal in the system browser with `juce::URL::launchInDefaultBrowser`, passing the one-time token in the URL fragment. No web view inside JUCE. |
| Direct sign-in | Email and password on the portal's login page, plus "forgot password". |

### Serving the portal at viracicom.com/emailserver

viracicom.com is a **Framer** website. The bare domain redirects to `www.viracicom.com`, which Framer serves. A path on that domain can't simply point at Firebase in DNS. It has to be forwarded by Framer.

- **Plan A: Framer rewrite.** Framer's Multi Site feature can serve an external HTTPS site under a path while keeping the viracicom.com address in the browser. Add a rewrite from `/emailserver/*` to the portal's Firebase Hosting address. Build the portal with base path `/emailserver/` so its pages, scripts and images load correctly. Rewrites forward cookies, which is fine because the portal is ours.
- **Plan B: subdomain.** If the Framer plan lacks rewrites, use `emailserver.viracicom.com`. That's one CNAME record pointing at Firebase Hosting, with no Framer involvement.
- Unsubscribe and click-tracking links in emails go on the same address under `/emailserver/u/` and `/emailserver/r/`.
- Because the public address is `www.viracicom.com`, the portal's real address in Encore and in emails is `https://www.viracicom.com/emailserver`.

### Portal screens

1. **Venue picker.** Venues and companies the user manages, each with its email connection status. Company Admins can choose "All venues in {Company}" or several venues.
2. **Campaigns.** Drafts, scheduled and sent, each with headline stats.
3. **Editor.** Drag-and-drop blocks: image, heading, text, button, columns, divider, social links. Image upload. A locked footer with venue name, address and unsubscribe link that can't be edited or removed. Desktop and mobile preview, test send, schedule or send now. Send buttons are disabled with a "Connect an email service first" link until the connection is verified.
4. **Templates.** Starter designs ("Tonight's Event", "Weekly Specials", "Theme Night", "We Miss You"). Admins can save their own.
5. **Audience.** Joined vs. subscribed, opt-in rate, growth chart. Customer list with stage name, join date and visit count for everyone, plus email for opted-in subscribers. CSV export.
6. **Reports.** Section 9.
7. **Settings.** Branding (logo, accent color, reply-to), Email Provider wizard (section 6.3), Venue Email Terms.
8. **Platform admin.** EnterpriseAdmin only. Connections and their status, per-venue volume, complaint and bounce rates, pause sending.

### Encore desktop changes (small)

- A **Venue Portal** button for Admin and above, in the NavBar or Settings. It calls `createPortalSession` on a background thread, then opens the browser via `callAsync`, following CLAUDE.md threading rules.
- Localization keys in `Resources/Languages/en_US.txt`.

### TAGG changes

- `DiscoverScreen.handleJoin()`: call `recordVenueJoin` after a successful join and show the consent prompt when told to.
- One-time existing-user prompt listing their past venues.
- New **Settings › Venue Emails** screen.
- Localized strings for the prompts and settings.

## 9. Reporting

**Audience (per venue, or rolled up per company)**
- TAGG users who joined, subscribed, declined, unsubscribed, bounced or complained.
- Opt-in rate: subscribed divided by joined.
- Weekly joins, opt-ins and unsubscribes chart.
- Customer list: name, first joined, last joined, times joined, subscribed yes/no, and email for subscribers.

**Per campaign**
- Funnel: Targeted, Sent, Delivered*, Unique clicks, Unsubscribed.
- Rates: delivery*, bounce*, click (clicks / sent or delivered), unsubscribe, complaint*.
- Estimated opens, clearly labelled.
- Per-link table: label, URL, clicks, unique clicks.
- Clicks over the first 72 hours.

\* Only with providers that send webhooks, such as SendGrid. Shown as "not available" for SMTP connections.

**Usage:** emails and campaigns per month per venue and company. This is the basis for any future Encore pricing.

## 10. Abuse and safety controls

Encore sets no send caps (decision 6). The controls below protect recipients and the shared `viracicom.com` link domain. Every email carries unsubscribe and tracking links on that domain, so spam sent by one venue could hurt the domain's reputation for all venues.

- Consent, suppression and unsubscribe checks on every send, regardless of provider.
- Auto-pause a connection when a campaign's complaint rate exceeds 0.3% or hard bounces exceed 10%. An admin sees why and can resume after review.
- Server-side HTML sanitization: no scripts, forms or iframes.
- Link checks against Google Safe Browsing. Known URL shorteners blocked.
- An audit log entry for every send: who, when, which venues, audience size, provider.
- EnterpriseAdmin can pause any venue or company.

## 11. Phased delivery

| Phase | Scope | Exit criteria |
|---|---|---|
| **0. Groundwork** | Create the KMS key. Write privacy policy updates and Venue Email Terms. Add deny-all rules for the new collections. Scaffold the portal, its Hosting site, and the Framer rewrite or subdomain. Open a SendGrid test account on a test domain to develop against. | Empty portal loads at viracicom.com/emailserver and signs in. |
| **1. Consent capture (ship TAGG early)** | Backfill script. `recordVenueJoin`, `getPendingVenueConsents` and consent callables. TAGG join prompt, existing-user prompt, and Venue Emails settings screen. | Backfill creates about 756 visitor records with no consent. Joining a venue creates a visitor record. Yes/no answers are stored with metadata. **Ship this first, because nobody can be emailed until they opt in.** |
| **2. Connections + send pipeline** | Provider adapter with SendGrid and SMTP. Setup wizard, KMS storage, DNS and blocklist checks, terms acceptance. `sendTestEmail`, `dispatchCampaign`, unsubscribe page, click and open tracking, SendGrid webhook, suppression list. Hard-coded test template. | A test venue connects SendGrid through the wizard. A 1,000-address seed list sends with no duplicates. One-click unsubscribe works from Gmail's UI. Bounces get suppressed. |
| **3. Portal composer** | Sign-in and Encore handoff. Venue picker. GrapesJS editor with locked footer, image upload, templates, branding. Schedule and cancel. Company multi-venue campaigns. | An admin builds, previews, test-sends and schedules a campaign with images and buttons. It renders correctly in Gmail, Outlook and iOS Mail. |
| **4. Reports** | Audience and campaign reports, usage, CSV export with logging. | Report numbers match the SendGrid dashboard within 1%. |
| **5. Hardening** | Auto-pause, link checks, nightly connection re-checks, platform admin screen, more native providers on request, load test. Pilot with 1–2 venues. | Pilot venues send real campaigns through their own accounts with complaint rate under 0.1%. |

## 12. Testing

- Functions unit tests with `node --test`, like `performanceEvents.test.js`. Cover unsubscribe token signing, consent transitions, suppression filtering, company dedupe, idempotent dispatch, free-mail blocking, DNS check parsing and webhook signatures.
- Firestore emulator tests proving TAGG users can't read any marketing collection.
- Portal component tests for the editor's locked footer, the disabled send state without a connection, and venue picker permissions.
- Seed-list sends to Gmail, Outlook.com, Yahoo and iCloud before each release.

## 13. Open questions

- **Framer plan.** Does the viracicom.com Framer plan include Multi Site rewrites? If not, use `emailserver.viracicom.com`.
- **Venues without their own domain.** Sending needs a domain the venue owns, and some small venues may not have one. Options: they register one, they use their company's connection, or Encore later offers a paid managed sender. Which should the wizard suggest?
