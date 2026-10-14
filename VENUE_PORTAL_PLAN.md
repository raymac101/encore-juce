# Encore Venue Portal — Design Plan

Status: DRAFT v1, 2026-10-14. Supersedes the provider and data-access parts of `VENUE_MARKETING_EMAIL_PLAN.md` (v3). The consent, unsubscribe and legal sections of v3 still apply and are summarized in section 6.

Open questions for the product owner are in section 13. Every place this plan had to assume an answer is marked **[Assumed]**.

## 1. What changed from v3

| Topic | v3 (2026-10-01) | Now |
|---|---|---|
| Who sends email | Each venue connects its own SendGrid or SMTP account | **Encore sends through one platform account it owns** |
| Who pays for email | The venue pays its own provider | **Encore charges the venue** for sending |
| Customer data | Venue sees and exports opted-in emails | **Venue never sees emails or contact details.** It sees counts, stage names in leaderboards, and campaign results in aggregate |
| Scope | Email composer only | **Venue Portal**: free dashboard, paid Insights subscription, email campaigns, billing |

Removed from v3: email provider connections, the setup wizard, Cloud KMS credential storage, the subscriber list with emails, and CSV export.

## 2. Product tiers

| Feature | Free | Insights (subscription) |
|---|---|---|
| Sign in, venue picker | Yes | Yes |
| **Total customers who have sung at the venue** (all time) | Yes | Yes |
| Email audience size (opted-in subscribers) | Yes | Yes |
| Songs performed this month, last night summary | Yes | Yes |
| Email campaigns | Pay per send | Monthly email allowance included, then pay per send **[Assumed]** |
| Top songs | — | Yes |
| Top artists | — | Yes |
| Top singers (most songs sung) | — | Yes |
| Top customers (most nights attended, loyalty) | — | Yes |
| Peak hours of the night (day × hour heatmap) | — | Yes |
| Date ranges and period comparison on every report | — | Yes |
| Smart audiences for campaigns: regulars, lapsed, new | — | Yes **[Assumed, optional]** |
| CSV/PDF export of the analytics (no contact data) | — | Yes |
| Company roll-up across all venues | — | Yes, if the company subscribes |

**[Assumed]** "Top singers" ranks by songs sung. "Top customers" ranks by distinct nights attended, with last-visit date. If you meant something else by these two, see question 3.

The free tier is the hook. A venue sees its customer count, sees a locked preview of the Insights pages with blurred sample charts, and upgrades from there.

## 3. What exists today and what it means

| Existing piece | Location | Use in the portal |
|---|---|---|
| **Performance events v2** (since 2026-08-18) | `firebase/functions/performanceEvents.js` writes `performanceEvents/{eventId}` | **Primary analytics source.** Each event has `venueId`, `songId`, `songName`, `artist`, `userId` or `guestSingerId`, `singerStageName`, `clientEndedAt`, `venueTimezone`, and `nightKey`, which already assigns after-midnight songs to the previous night. |
| Legacy venue audit | `venues/{venueId}/audit`, written by `Source/Services/AuditService.cpp` and the old Angular app | **Backfill source** for history before v2: song, artist, singerId, singerName, date. No night key, so the backfill computes one from the venue time zone. |
| Per-user song history | `users/{uid}/userAudit` | Backfill of customer-to-venue links for email consent, as in v3. |
| Per-venue singer roster | `venues/{venueId}/members/{singerId}` | Already has first/last performed and count. It lives under `venues/`, which any signed-in user can read, so the portal does **not** build on it. New stats go in deny-all collections. |
| Venue and company roles | `user-venue-lookup`, `companies/{id}/members` | Reused for portal authorization, unchanged from v3. |
| Hosting | Repo-root `firebase.json` | The portal becomes a second Hosting site, as in v3. |

Today's audience scale (measured 2026-10-01): 21 venues with linked users, largest audiences 420, 142, 62, 23 and 22. Pricing in section 8 is sized with this in mind.

## 4. Architecture

```
TAGG app ──consent──▶ Cloud Functions ◀──callables── Venue Portal (React, viracicom.com)
Encore desktop ──performance events──▶ Firestore │                    │
                                                 ├──▶ Stats rollups    ├──▶ Stripe Checkout / Billing Portal
                                                 ├──▶ Campaign dispatch ──▶ Encore's SendGrid account ──▶ inbox
Stripe ──webhooks──▶ Cloud Functions             └──▶ Entitlements
SendGrid ──event webhook──▶ Cloud Functions
```

Rules that hold everywhere:

- The portal never reads Firestore directly. Every screen calls a Cloud Function that re-checks the caller's role and the venue's plan on the server.
- All new collections deny client reads and writes.
- No function returns an email address, phone, birthday or real name to the portal. The only person-level field ever returned is a **stage name**, and only in leaderboards.

## 5. Venue analytics

### 5.1 Metric definitions

All times are in the **venue's local time zone**. A "night" is the `nightKey` from performance events, so a song at 1:30 a.m. Saturday counts toward Friday night.

| Metric | Definition |
|---|---|
| Total customers (free) | Distinct registered TAGG singers with at least one qualified performance at the venue, all time. Guest singers are shown as a separate line: "plus N songs by guest singers". **[Assumed]** |
| Top songs | Performances grouped by `songId`, falling back to normalized title + artist when IDs differ between vendors. Ranked by count. |
| Top artists | Performances grouped by normalized artist name. Ranked by count, with distinct-singer count beside it. |
| Top singers | Singers ranked by songs performed in the range. |
| Top customers | Singers ranked by distinct nights attended in the range, then songs. Shows first visit and last visit. |
| Peak hours | Heatmap of day-of-night × local hour, showing songs performed and distinct singers per hour. A line chart averages a typical night. |
| Date ranges | Presets: last night, last 7 days, last 30 days, last 90 days, this year, all time. Plus a custom range and "compare to previous period". |

A "qualified performance" is the existing threshold in `performanceEvents.js`: at least 30 seconds played, or completed.

### 5.2 How the numbers are computed

Raw events are never scanned at query time. A Firestore trigger on `performanceEvents/{eventId}` creates updates rollup documents:

```
venueNightStats/{venueId}_{nightKey}
  venueId, companyId, nightKey, dayOfWeek
  performances, registeredSingers, guestPerformances
  hours: { "20": {songs, singers}, "21": {...}, ... }      // local hour
  songs:   { <songKey>: { n, title, artist } }
  artists: { <artistKey>: { n, name } }
  singers: { <singerId>: { n, stageName, guest } }

venueSingerStats/{venueId}_{singerId}
  venueId, singerId, guest, stageName
  songs, nights, firstNightKey, lastNightKey

venueStatsTotals/{venueId}
  registeredCustomers, guestPerformances, totalPerformances, firstNightKey, updatedAt
```

- `songKey` and `artistKey` are short hashes of the normalized values, because Firestore map keys can't hold arbitrary characters.
- A query for a date range reads one night doc per night in the range, at most 366 for a year, and merges them in the function. Results are cached for 10 minutes per venue and range.
- Tonight's numbers are live because the trigger runs as songs finish.
- The trigger is idempotent by event ID, so a retried event never double-counts.
- A `backfillVenueStats` script in `functions/scripts/` replays legacy `venues/{venueId}/audit` records from before 2026-08-18, then v2 events. It has a dry-run mode and skips legacy records that also exist as v2 events.

**Why not BigQuery now.** Events already carry a `bigQueryStatus` field, so BigQuery is the natural home for cross-venue, platform-wide analytics later. For per-venue dashboards at today's volume, Firestore rollups are cheaper, faster and need no new infrastructure.

### 5.3 Privacy in analytics

- Leaderboards show stage names only. Clicking a name shows that singer's songs and nights at **this venue**, nothing else.
- TAGG gets a setting, **"Show my stage name in venue leaderboards"**, on by default **[Assumed]**. When off, the singer appears as "Private singer" but still counts in totals.
- When a TAGG account is deleted, its stats rows keep their counts and the stage name becomes "Former singer".
- Company Admins see roll-ups across their venues. No venue sees another company's data.

## 6. Email campaigns

### 6.1 Consent (carried over from v3, wording changed)

Consent capture is the same as v3 section 4.1: a one-time prompt after a TAGG venue join, a one-time prompt for existing users listing their past venues, and a Venue Emails settings screen. Opt-in only.

The prompt text changes, because the venue no longer receives the address:

> **Get emails from {Venue Name}?**
> Specials, theme nights and events, sent by Encore for {Venue Name}. {Venue Name} won't see your email address. Unsubscribe any time.
> [Yes, sign me up] [No thanks]

### 6.2 What the venue sees

- **Audience:** subscribed count, opt-in rate, growth chart, unsubscribes. Counts only.
- **Campaign results:** sent, delivered, bounced, estimated opens, clicks per link, unsubscribes. Aggregates only, no per-recipient list.
- **Small-audience rule:** a campaign result is hidden for audiences under 5, so a venue can't work out what one person did.
- Replies go to the venue's reply-to address. A customer who replies chooses to share their address. This is stated in the Venue Terms.

### 6.3 Sending through Encore's account

| Aspect | Choice |
|---|---|
| Provider | **SendGrid**, one Encore-owned account. The v3 provider adapter stays, so Amazon SES can replace it if volume grows. |
| Sending domain | A dedicated subdomain such as `mail.viracicom.com` **[Assumed]**, with its own SPF, DKIM and DMARC. The root `viracicom.com` SPF and the prxy mailboxes are untouched, so marketing reputation can't harm company mail. |
| From line | `"{Venue Name}" <venues@mail.viracicom.com>`, reply-to set to the venue's verified address. |
| Footer | Locked: venue name, venue physical address, "Sent by Encore on behalf of {Venue}", one-click unsubscribe. Required by CAN-SPAM and CASL. |
| Headers | RFC 8058 `List-Unsubscribe` and `List-Unsubscribe-Post`, as Gmail and Yahoo require. |
| Webhook | SendGrid signed event webhook records delivered, bounce and complaint. Hard bounces and complaints go to the global suppression list. |
| Venue reply-to | Verified by a confirmation email before first send. |

The send pipeline from v3 section 7 stays: `dispatchCampaign` pages subscribers, resolves emails from Firebase Auth on the server, filters suppression and global opt-outs, dedupes across venues for company campaigns, rewrites links for click tracking, and is idempotent per recipient.

### 6.4 Smart audiences (Insights only, optional)

Because the venue can't pick individual recipients, Insights venues can target segments the server computes:

- **Regulars:** 3 or more nights in the last 90 days.
- **Lapsed:** sang before, not seen in 60 days. Fits the "We Miss You" template.
- **New:** first night in the last 30 days.
- **Everyone subscribed** (the only option on the free tier).

The portal shows the segment's size before sending, never its members.

### 6.5 Protecting Encore's sending reputation

Encore now owns the sender, so one bad venue can hurt every venue. **[Assumed]** controls:

- A venue's **first campaign** is held for EnterpriseAdmin approval. Later campaigns send automatically.
- Auto-pause a venue when a campaign's complaint rate passes 0.3% or hard bounces pass 5%.
- No attachments, no scripts, forms or iframes. Links checked against Google Safe Browsing. URL shorteners blocked.
- Venue Terms include an Acceptable Use Policy. EnterpriseAdmin can pause any venue.
- Credits (section 8) cap volume naturally, so no separate daily cap is needed.

## 7. Portal screens

| Screen | Free | Insights |
|---|---|---|
| **Sign in** | Existing Encore email/password, or one-click from the Encore desktop button (v3 `createPortalSession`) | Same |
| **Venue picker** | Venues and companies the user manages, each with its plan badge | Same, plus "All venues" for Company Admins |
| **Dashboard** | Total customers, guest songs, subscribed audience, songs this month, last night summary | Adds trend sparklines and top 5 songs and singers |
| **Insights › Songs** | Locked preview | Top songs table and bar chart, date range, compare |
| **Insights › Artists** | Locked preview | Top artists |
| **Insights › Singers** | Locked preview | Top singers and top customers, with a singer detail drawer |
| **Insights › Peak hours** | Locked preview | Heatmap and typical-night curve |
| **Campaigns** | Drafts, scheduled, sent, with headline results | Same, plus smart audiences |
| **Editor** | GrapesJS + MJML from v3, locked footer, test send, schedule | Same |
| **Billing** | Plan, upgrade button, email credit balance, buy credits, invoices | Same, plus manage subscription |
| **Settings** | Venue branding (logo, color), reply-to address, Venue Terms | Same |
| **Platform admin** | EnterpriseAdmin only: approvals queue, venue send health, pause, revenue summary, grant a free plan | — |

Every Insights page has the same date-range bar at the top, so the user picks a range once and it applies across pages. Stack and hosting are unchanged from v3: React, TypeScript, Vite, Firebase JS SDK, a chart library such as Recharts, on a second Firebase Hosting site.

## 8. Billing

### 8.1 Stripe

| Need | Stripe feature |
|---|---|
| Insights subscription, monthly or annual | Stripe Billing subscription via hosted **Checkout** |
| Card changes, invoices, cancel | Hosted **Customer Portal** |
| Email credits | One-time Checkout payments for credit packs **[Assumed]** |
| Sales tax and GST/HST | **Stripe Tax** |
| Free trial | Subscription trial period, e.g. 14 days **[Assumed]** |

No card data touches Encore. Stripe webhooks update entitlements in Firestore.

### 8.2 How email is charged — recommended model

**[Assumed]** Prepaid credits, 1 credit = 1 email delivered to the provider.

- Credits are bought in packs. A venue can't overspend and Encore never chases unpaid usage.
- Insights includes a monthly allowance that doesn't roll over. Purchased credits never expire.
- Scheduling a campaign **reserves** credits for the audience size. After sending, unused credits (suppressed, unsubscribed since scheduling) are released. Bounces still cost a credit, because the provider charged for them.
- Test sends are free, up to 20 a day.

**Illustrative prices to react to, not decisions:**

| Item | Price |
|---|---|
| Insights, per venue | $29/month or $290/year |
| Emails included with Insights | 2,000/month |
| Credit pack | 1,000 emails for $10 |
| Encore's cost at SendGrid's entry plan | well under $1 per 1,000 emails, so margin is high; check current pricing |

At today's largest audience of 420, a free-tier venue sending weekly spends about $17 a month. That's a natural nudge toward Insights.

The alternative is **metered billing**: send freely, pay at month end through Stripe usage records. It's simpler for venues but risks unpaid bills and surprise invoices. See question 6.

### 8.3 Entitlements

```
billingAccounts/{billingAccountId}
  ownerType: "venue" | "company", ownerId, stripeCustomerId, billingEmail, createdBy

venueEntitlements/{venueId}
  plan: "free" | "insights", source: "stripe" | "company" | "grant"
  status: "active" | "trialing" | "past_due" | "canceled"
  billingAccountId, stripeSubscriptionId, currentPeriodEnd, graceUntil
  monthlyEmailAllowance, allowanceUsedThisPeriod

emailCredits/{ownerId}
  balance, reserved, updatedAt

emailCreditLedger/{entryId}
  ownerId, delta, reason: "purchase" | "allowance" | "reserve" | "settle" | "release" | "grant" | "refund"
  campaignId?, stripePaymentIntentId?, by, at
```

- `past_due` keeps Insights for a 7-day grace period, then drops to free. Data is kept, so re-subscribing restores everything at once.
- A company subscription can cover all its venues (`source: "company"`), priced per venue **[Assumed]**.
- EnterpriseAdmin can grant Insights for free, e.g. for pilot venues.

## 9. Cloud Functions

New or changed, beyond those kept from v3 (consent callables, campaign CRUD, dispatch, unsubscribe, click and open tracking, provider webhook):

| Function | Type | Purpose |
|---|---|---|
| `onPerformanceEventCreate` | Firestore trigger | Update `venueNightStats`, `venueSingerStats`, `venueStatsTotals`. Idempotent. |
| `backfillVenueStats` | script | Build rollups from legacy audit and v2 events. Dry run. |
| `getVenueOverview` | callable | Free dashboard numbers. Any venue role. |
| `getInsights` | callable | `{ venueIds, from, to, compare, report: "songs" \| "artists" \| "singers" \| "customers" \| "hours" }`. Requires an active Insights entitlement for every venue requested. |
| `getSingerDetail` | callable | One singer's history at the venue. Insights only. Stage name and counts only. |
| `exportInsights` | callable | CSV or PDF of a report. No contact data. |
| `getAudienceSize` | callable | Size of a segment before send. Counts only. |
| `createCheckoutSession` | callable | Subscription or credit-pack Checkout. Venue or Company Admin only. |
| `createBillingPortalSession` | callable | Opens Stripe's Customer Portal. |
| `stripeWebhook` | HTTPS | Verify signature. Handle `checkout.session.completed`, `customer.subscription.*`, `invoice.paid`, `invoice.payment_failed`. Update entitlements and credits. |
| `resetEmailAllowances` | scheduler | Reset allowances at each period start. |
| `approveCampaign` | callable | EnterpriseAdmin approval of a venue's first campaign. |
| `verifyReplyTo` | callable + HTTPS | Confirm the venue's reply-to address. |

Removed from v3: `saveEmailConnection`, `deleteEmailConnection`, `checkEmailConnection`, `acceptEmailTerms`, `exportSubscribers`.

## 10. Security and privacy

- Authorization is re-derived on the server for every call, as in v3 section 7.
- The plan check lives in one helper, `requireEntitlement(venueId, "insights")`, used by every Insights callable. The portal hides locked features, but the server is what enforces them.
- The three security issues in v3 section 3 still apply. Analytics and billing collections are new top-level, deny-all collections, so the open `venues/` rule can't expose them.
- Stripe and SendGrid secrets live in Secret Manager.
- Privacy policies for TAGG and Encore get updated: Encore emails on behalf of venues, venues see aggregated stats and stage names, and the leaderboard opt-out.
- **Venue Terms** cover the Acceptable Use Policy, pricing, refund policy, and the fact that customer contact data stays with Encore.

## 11. Phased delivery

The order puts consent capture first, because nobody can be emailed until they opt in. Insights ships before email because it carries no deliverability or anti-spam risk and starts subscription revenue sooner. **[Assumed]**

| Phase | Scope | Exit criteria |
|---|---|---|
| **0. Groundwork** | Portal scaffold and hosting. Stripe account with Tax. SendGrid account and the `mail.` subdomain with SPF, DKIM, DMARC. Deny-all rules for new collections. Venue Terms and privacy policy drafts. | Empty portal signs in at its URL. A test email from the subdomain scores 9+ on mail-tester. |
| **1. Consent + stats foundation** | TAGG consent prompts and settings (v3 phase 1). Leaderboard opt-out setting. `onPerformanceEventCreate` rollups. Backfill script. | Backfill totals match a direct count of audit records within 1%. A song finished in Encore shows in rollups within a minute. |
| **2. Free dashboard** | Venue picker, `getVenueOverview`, Dashboard, locked Insights previews. Encore desktop "Venue Portal" button. | A venue admin sees its total customers and audience size. |
| **3. Insights + billing** | `getInsights`, all four Insights pages, date ranges, compare, export. Stripe Checkout, Customer Portal, webhook, entitlements, grace period, admin grants. | A test venue subscribes, sees Insights, cancels, and drops to free at period end. |
| **4. Email sending** | Credits, ledger, reservations. Send pipeline on Encore's SendGrid. Editor, templates, test send, reply-to verification, first-campaign approval. Campaign results. | A pilot venue buys credits, gets approval, and sends to a seed list with correct charges and working unsubscribe. |
| **5. Smart audiences + hardening** | Segments, auto-pause, link checks, platform admin screen, load test. Pilot with 1–2 venues. | Pilot complaint rate under 0.1%. Billing reconciles with Stripe. |

## 12. Testing

- `node --test` unit tests for rollup math (night boundaries across midnight and DST), song and artist normalization, entitlement checks, credit reservation and release, and Stripe webhook handling with fixture events.
- Firestore emulator tests proving no client can read stats, billing or marketing collections.
- A test proving no portal callable returns an email field, run over every callable's response shape.
- Stripe test-mode runs of subscribe, upgrade, payment failure, grace expiry and cancel.
- Seed-list sends to Gmail, Outlook.com, Yahoo and iCloud before each release.

## 13. Open questions

1. **Portal address.** It's now more than email. Keep `viracicom.com/emailserver`, or use something like `viracicom.com/venues` or `portal.viracicom.com`?
2. **Brand customers see.** Should emails and the portal say "Encore", "TAGG", or "Viracicom"? This sets the sending subdomain and the "Sent by … on behalf of" line.
3. **Top singers vs. top customers.** Is "most songs sung" vs. "most nights attended" what you meant? Or should top customers mean something else, such as most money spent if that data ever exists?
4. **Guest singers.** Should guests who sing without a TAGG account count toward "total customers"? Guest IDs may not stay the same across nights, so counting them could inflate the number.
5. **Stage names in leaderboards.** OK to show stage names to the venue, with a TAGG opt-out? Or should Insights be fully anonymous ("Singer #12")?
6. **Email charging model.** Prepaid credit packs, monthly metered billing, or a flat per-campaign fee? And should Insights include a monthly email allowance?
7. **Prices.** Insights price per venue, annual discount, company discount, email price, and currency (CAD, USD or both).
8. **Free trial.** Offer a trial of Insights? How long?
9. **Who pays for multi-venue companies.** One company subscription covering all venues, or each venue subscribes on its own?
10. **Free-tier email.** Can free venues send at all by buying credits, or is email an Insights-only feature?
11. **Campaign approval.** Hold each venue's first campaign for your review, review every campaign, or no review?
12. **History.** Should Insights include history from the legacy audit log before August 2026, or start from the v2 events only?
13. **Billing roles.** Only Venue Admins and Company Admins can subscribe and buy credits. Should Hosts see the free dashboard?
14. **Framer plan** (carried over from v3). Does it support path rewrites, or do we use a subdomain?
