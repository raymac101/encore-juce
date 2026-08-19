/*
  ==============================================================================

    LoginFlowController.cpp

  ==============================================================================
*/

#include "LoginFlowController.h"
#include "../Services/UserPreferences.h"
#include "../Services/HostService.h"
#include "../Services/InvitationService.h"
#include "../Services/LicenseService.h"

namespace
{
    using FC = FirestoreClient;

    //--- Firestore queries -----------------------------------------------------
    juce::var stringFilter(const juce::String& fieldPath, const juce::String& op, const juce::var& value)
    {
        // { fieldFilter: { field: { fieldPath }, op, value } }
        juce::DynamicObject::Ptr field = new juce::DynamicObject();
        field->setProperty("fieldPath", fieldPath);

        juce::DynamicObject::Ptr filter = new juce::DynamicObject();
        filter->setProperty("field", juce::var(field.get()));
        filter->setProperty("op", op);
        filter->setProperty("value", value);

        juce::DynamicObject::Ptr wrap = new juce::DynamicObject();
        wrap->setProperty("fieldFilter", juce::var(filter.get()));
        return juce::var(wrap.get());
    }

    juce::var compositeAnd(juce::Array<juce::var> filters)
    {
        juce::DynamicObject::Ptr c = new juce::DynamicObject();
        c->setProperty("op", "AND");
        c->setProperty("filters", filters);

        juce::DynamicObject::Ptr wrap = new juce::DynamicObject();
        wrap->setProperty("compositeFilter", juce::var(c.get()));
        return juce::var(wrap.get());
    }

    juce::var fromCollection(const juce::String& collection)
    {
        juce::DynamicObject::Ptr fc = new juce::DynamicObject();
        fc->setProperty("collectionId", collection);

        juce::Array<juce::var> arr;
        arr.add(juce::var(fc.get()));
        return arr;
    }

    // Same as fromCollection(), but matches `collection` at ANY depth
    // (Firestore's "collection group" query) rather than only directly
    // under the query's parent path -- needed for `members`, which lives
    // at companies/{companyId}/members/{uid} and there's no way to know
    // companyId up front.
    juce::var fromCollectionGroup(const juce::String& collection)
    {
        juce::DynamicObject::Ptr fc = new juce::DynamicObject();
        fc->setProperty("collectionId", collection);
        fc->setProperty("allDescendants", true);

        juce::Array<juce::var> arr;
        arr.add(juce::var(fc.get()));
        return arr;
    }

    juce::var buildQuery(const juce::String& collection, juce::var where)
    {
        juce::DynamicObject::Ptr q = new juce::DynamicObject();
        q->setProperty("from", fromCollection(collection));
        if (! where.isVoid())
            q->setProperty("where", where);
        return juce::var(q.get());
    }

    juce::var buildCollectionGroupQuery(const juce::String& collection, juce::var where)
    {
        juce::DynamicObject::Ptr q = new juce::DynamicObject();
        q->setProperty("from", fromCollectionGroup(collection));
        if (! where.isVoid())
            q->setProperty("where", where);
        return juce::var(q.get());
    }

    // Company membership isn't in custom claims (nothing in the backend
    // ever sets them) -- the only real record is the
    // companies/{companyId}/members/{uid} doc created by
    // CompanyService::addCompanyMember(). Find it (if any) via a
    // collection-group query on `members` filtered by userId, and pull
    // companyId back out of the matched doc's resource name (there's no
    // field carrying it directly).
    struct CompanyMembership
    {
        bool found = false;
        juce::String companyId;
        juce::String role;
    };

    CompanyMembership queryCompanyMembership(const juce::String& uid)
    {
        CompanyMembership result;

        auto query = buildCollectionGroupQuery("members",
            stringFilter("userId", "EQUAL", FC::stringValue(uid)));

        auto docs = FC::getInstance().runQuery({}, query);
        if (docs.isEmpty())
            return result;

        const auto& d = docs.getReference(0);
        const auto name = d.getProperty("name", "").toString();
        // .../companies/{companyId}/members/{uid} -- companyId is the
        // third-from-last path segment.
        auto segments = juce::StringArray::fromTokens(name, "/", "");
        if (segments.size() >= 3)
            result.companyId = segments[segments.size() - 3];

        result.role  = FC::readString(d, "role");
        result.found = result.companyId.isNotEmpty();
        return result;
    }

    // Company-only owners/admins (no personal venue association of their
    // own) still need *some* venue to boot MainComponent with -- it has no
    // venue-less mode. Real venues live in the root `venues` collection
    // (see VenueService::addVenue), tagged with a companyId field, so find
    // the first one that belongs to this company.
    struct CompanyVenue
    {
        juce::String venueId;
        juce::String venueName;
    };

    CompanyVenue queryFirstCompanyVenue(const juce::String& companyId)
    {
        CompanyVenue result;
        if (companyId.isEmpty())
            return result;

        juce::Array<juce::var> filters;
        filters.add(stringFilter("companyId", "EQUAL", FC::stringValue(companyId)));
        auto query = buildQuery("venues", compositeAnd(filters));

        auto docs = FC::getInstance().runQuery({}, query);
        if (docs.isEmpty())
            return result;

        const auto& d = docs.getReference(0);
        const auto name = d.getProperty("name", "").toString();
        result.venueId   = name.fromLastOccurrenceOf("/", false, false);
        result.venueName = FC::readString(d, "name");
        return result;
    }

    //--- Host loading / creation ---------------------------------------------
    Host hostFromDoc(const juce::var& doc, const juce::String& uid)
    {
        Host h;
        h.userId    = FC::readString(doc, "userId").toStdString();
        if (h.userId.empty()) h.userId = uid.toStdString();
        h.email     = FC::readString(doc, "email").toStdString();
        h.companyId = FC::readString(doc, "companyId").trim().toStdString();
        h.profileId = FC::readString(doc, "profileId").toStdString();
        h.avatarUrl = FC::readString(doc, "avatarUrl").toStdString();
        h.stageName = FC::readString(doc, "stageName").toStdString();
        h.fullName  = FC::readString(doc, "fullName").toStdString();
        h.country   = FC::readString(doc, "country").toStdString();
        h.city      = FC::readString(doc, "city").toStdString();
        h.gender    = FC::readString(doc, "gender").toStdString();
        h.birthday  = FC::readString(doc, "birthday").toStdString();
        h.signUpDate = FC::readString(doc, "signUpDate").toStdString();
        h.lastLogin  = FC::readString(doc, "lastLogin").toStdString();
        h.loginCount = (int) FC::readInt(doc, "loginCount", 0);
        auto roleStr = FC::readString(doc, "role").toStdString();
        if (! roleStr.empty())
            h.role = AccessRightsUtil::stringToUserRole(roleStr);
        return h;
    }

    bool isFirstHostEver()
    {
        // Cheap probe: list `hosts` with a 1-document page.
        auto docs = FC::getInstance().listCollection("hosts", 1);
        return docs.isEmpty();
    }

    Host loadOrCreateHost(const juce::String& uid, const juce::String& email)
    {
        auto path = "hosts/" + uid;
        int status = 0;
        auto doc = FC::getInstance().getDocument(path, &status);

        if (status == 200 && doc.hasProperty("fields"))
        {
            Host existing = hostFromDoc(doc, uid);

            // lastLogin/loginCount previously only got set once, at account
            // creation below, and were never touched again on subsequent
            // logins. Fire a best-effort background patch (same style as
            // touchLastAccess()) so they actually reflect real sign-in
            // activity going forward -- read-then-write, not atomic, but
            // that's an accepted tradeoff for a simple incrementing field
            // (FirestoreClient has no fieldTransform/increment support).
            auto fields = FC::makeFields({
                { "lastLogin",  FC::stringValue(juce::Time::getCurrentTime().toISO8601(true)) },
                { "loginCount", FC::integerValue(existing.loginCount + 1) }
            });
            const auto patchPath = path + "?updateMask.fieldPaths=lastLogin&updateMask.fieldPaths=loginCount";
            FC::getInstance().patchDocument(patchPath, fields);

            return existing;
        }

        // No host doc — create one. First user ever ⇒ EnterpriseAdmin.
        const bool first = isFirstHostEver();
        const UserRole role = first ? UserRole::EnterpriseAdmin : UserRole::Host;

        auto now = juce::Time::getCurrentTime();
        juce::Time expiry(now.toMilliseconds() + (juce::int64) 365 * 24 * 60 * 60 * 1000);

        auto fields = FC::makeFields({
            { "userId",     FC::stringValue(uid) },
            { "email",      FC::stringValue(email) },
            { "companyId",  FC::stringValue("") },
            { "profileId",  FC::stringValue("") },
            { "avatarUrl",  FC::stringValue("assets/images/AvatarWhite.png") },
            { "stageName",  FC::stringValue("Anonymous") },
            { "fullName",   FC::stringValue(FC::getInstance().getDisplayName()) },
            { "birthday",   FC::stringValue("") },
            { "country",    FC::stringValue("") },
            { "city",       FC::stringValue("") },
            { "gender",     FC::stringValue("") },
            { "signUpDate", FC::stringValue(now.toISO8601(true)) },
            { "lastLogin",  FC::stringValue(now.toISO8601(true)) },
            { "role",       FC::stringValue(juce::String(AccessRightsUtil::userRoleToString(role))) },
            { "accessExpirationDate", FC::timestampValue(expiry) }
        });

        // Use uid as the document id so future getDocument(hosts/<uid>) works.
        FC::getInstance().createDocument("hosts", fields, uid);

        Host h;
        h.userId    = uid.toStdString();
        h.email     = email.toStdString();
        h.role      = role;
        h.stageName = "Anonymous";
        h.signUpDate = now.toISO8601(true).toStdString();
        h.lastLogin  = now.toISO8601(true).toStdString();
        return h;
    }

    //--- Associations -----------------------------------------------------------
    std::vector<UserVenueAssociation> queryAssociations(const juce::String& uid)
    {
        // Mirrors VenueService.getVenuesForUser() in the Angular app:
        // query the `user-venue-lookup` collection, filtered by userId &
        // status='active'. Doc id is `${userId}_${venueId}`.
        juce::Array<juce::var> filters;
        filters.add(stringFilter("userId", "EQUAL", FC::stringValue(uid)));
        filters.add(stringFilter("status", "EQUAL", FC::stringValue("active")));
        auto query = buildQuery("user-venue-lookup", compositeAnd(filters));

        std::vector<UserVenueAssociation> out;
        for (auto& d : FC::getInstance().runQuery({}, query))
        {
            UserVenueAssociation a;
            auto name = d.getProperty("name", "").toString();
            a.id = name.fromLastOccurrenceOf("/", false, false);
            a.userId    = FC::readString(d, "userId");
            a.venueId   = FC::readString(d, "venueId");
            a.venueName = FC::readString(d, "venueName");
            a.role      = AccessRightsUtil::stringToUserRole(FC::readString(d, "role").toStdString());
            a.isActive  = (FC::readString(d, "status") == "active");
            a.acceptedDate   = FC::readTime(d, "joinedDate");
            a.lastAccessDate = FC::readTime(d, "lastActive");
            out.push_back(std::move(a));
        }
        return out;
    }

    void touchLastAccess(const juce::String& venueId, const juce::String& uid)
    {
        // user-venue-lookup uses a deterministic doc id of `${uid}_${venueId}`.
        const auto docId = uid + "_" + venueId;
        auto fields = FC::makeFields({
            { "lastActive", FC::timestampValue(juce::Time::getCurrentTime()) }
        });
        const auto path = "user-venue-lookup/" + docId
                        + "?updateMask.fieldPaths=lastActive";
        FC::getInstance().patchDocument(path, fields);
    }

    //--- License gate -----------------------------------------------------------
    // Every path that would otherwise resolve to Outcome::VenueLoaded must
    // go through here first, so an invalid/expired license can't be
    // bypassed via whichever branch forgets the check.
    void tryResolveVenueLoaded(const juce::String& venueId,
                               const juce::String& venueName,
                               LoginFlowController::Result& result)
    {
        bool valid = true;
        juce::String reason;
        LicenseService::checkVenueLicenseSync(venueId, valid, reason);

        if (valid)
        {
            result.outcome   = LoginFlowController::Outcome::VenueLoaded;
            result.venueId   = venueId;
            result.venueName = venueName;
        }
        else
        {
            result.outcome        = LoginFlowController::Outcome::VenueLicenseInvalid;
            result.venueId        = venueId;
            result.venueName      = venueName;
            result.licenseMessage = reason;
        }
    }

    //--- Result dispatch -------------------------------------------------------
    void postOnMessageThread(std::function<void()> fn)
    {
        juce::MessageManager::callAsync(std::move(fn));
    }
}

//==============================================================================
void LoginFlowController::runPostAuthFlow(ResultCallback onResult, ErrorCallback onError)
{
    juce::Thread::launch([onResult = std::move(onResult), onError = std::move(onError)]()
    {
        try
        {
            auto& fc = FirestoreClient::getInstance();
            if (! fc.isSignedIn())
            {
                postOnMessageThread([onError]() { onError("Not signed in"); });
                return;
            }

            const auto uid   = fc.getUserId();
            const auto email = fc.getEmail().toLowerCase();
            const auto membership = queryCompanyMembership(uid);
            const auto companyId = membership.companyId;
            const auto companyRole = membership.role;

            // 1) Host bootstrap
            Host host = loadOrCreateHost(uid, email);
            HostService::getInstance().setCurrent(host);

            // 2) Read stored venueId from prefs
            const auto storedVenueId = UserPreferences::getInstance().getVenueId();

            // 3) Auto-claim any pending invitations for this email FIRST --
            //    e.g. a venue admin added this user via Settings > Invite
            //    while they were signed out. This is what makes that show
            //    up in `associations` below with no separate "accept" step
            //    ever surfacing; best-effort (see claimAllPendingSync's own
            //    comment), so a transient failure here just means the next
            //    login retries it.
            InvitationService::claimAllPendingSync(email);

            // 4) Associations
            auto associations = queryAssociations(uid);

            const bool canCreate = (host.role == UserRole::Admin
                                  || host.role == UserRole::EnterpriseAdmin
                                  || host.role == UserRole::Tester);
            const bool hasCompanyContext = companyId.isNotEmpty();
            const bool companyCanCreate = companyRole.equalsIgnoreCase("company_admin")
                                       || companyRole.equalsIgnoreCase("enterprise_admin")
                                       || companyRole.equalsIgnoreCase("platform_admin");

            juce::String companyName;
            juce::String companyFallbackVenueId, companyFallbackVenueName;
            if (hasCompanyContext)
            {
                auto companyDoc = fc.getDocument("companies/" + companyId);
                companyName = FirestoreClient::readString(companyDoc, "name");

                // Only needed as a fallback boot target for a company user
                // with no venue association of their own -- skip the extra
                // query otherwise.
                if (associations.empty())
                {
                    auto companyVenue = queryFirstCompanyVenue(companyId);
                    companyFallbackVenueId   = companyVenue.venueId;
                    companyFallbackVenueName = companyVenue.venueName;
                }
            }

            DBG("[LoginFlow] storedVenueId=" << storedVenueId
                << " associations=" << (int) associations.size()
                << " role=" << juce::String(AccessRightsUtil::userRoleToString(host.role))
                << " companyId=" << companyId
                << " companyRole=" << companyRole);

            Result result;
            result.host              = host;
            result.canCreateVenue    = canCreate || companyCanCreate;
            result.configuredVenueId = storedVenueId;
            result.hasCompanyContext  = hasCompanyContext;
            result.companyId         = companyId;
            result.companyRole       = companyRole;
            result.companyName       = companyName;
            result.companyFallbackVenueId   = companyFallbackVenueId;
            result.companyFallbackVenueName = companyFallbackVenueName;

            // 5) Apply the scenario tree (mirrors Angular start.component logic)
            //    - Multiple associations → ALWAYS show picker (the configured
            //      venue is just badged in the list).
            //    - Single association → auto-load it, UNLESS the user also
            //      has company context: they need the picker so the
            //      "manage company" card is reachable instead of being
            //      dropped straight into their one venue.
            //    - Zero associations → request access (if stored venueId set
            //      and user isn't an admin) / await invitation otherwise,
            //      UNLESS they have company context, in which case they
            //      still need the picker to reach "Manage Company" (there's
            //      no venue association to gate it on for a company-only
            //      owner/admin who never joined a venue directly).
            if (associations.size() > 1 || hasCompanyContext)
            {
                result.outcome      = Outcome::PickVenue;
                result.associations = std::move(associations);
            }
            else if (associations.size() == 1)
            {
                tryResolveVenueLoaded(associations[0].venueId, associations[0].venueName, result);
                if (result.outcome == Outcome::VenueLoaded)
                {
                    UserPreferences::getInstance().setVenueId(associations[0].venueId);
                    // Do not block startup on a slow network write.
                    juce::Thread::launch([vid = associations[0].venueId, uid]()
                    {
                        touchLastAccess(vid, uid);
                    });
                }
            }
            else if (storedVenueId.isNotEmpty())
            {
                const bool adminOverride =
                    (host.role == UserRole::EnterpriseAdmin || host.role == UserRole::Tester);

                if (adminOverride)
                {
                    tryResolveVenueLoaded(storedVenueId, {}, result);
                    if (result.outcome == Outcome::VenueLoaded)
                    {
                        // Do not block startup on a slow network write.
                        juce::Thread::launch([vid = storedVenueId, uid]()
                        {
                            touchLastAccess(vid, uid);
                        });
                    }
                }
                else
                {
                    result.outcome = Outcome::RequestAccess;
                    result.venueId = storedVenueId;
                }
            }
            else
            {
                // Pending invitations for this email were already claimed
                // in step 3 above -- if any existed, associations wouldn't
                // be empty here. Zero associations at this point genuinely
                // means "nothing to join," so self-serve setup is always
                // offered rather than blocking on an invitations list.
                result.outcome             = Outcome::AwaitInvitation;
                result.offerSelfServeSetup = true;
            }

            postOnMessageThread([onResult, result = std::move(result)]() mutable
            {
                onResult(std::move(result));
            });
        }
        catch (const std::exception& e)
        {
            const juce::String msg(e.what());
            postOnMessageThread([onError, msg]() { onError(msg); });
        }
        catch (...)
        {
            postOnMessageThread([onError]() { onError("Unexpected error in login flow"); });
        }
    });
}

void LoginFlowController::selectVenue(const juce::String& venueId,
                                      std::function<void(bool ok, juce::String licenseMessage)> onDone)
{
    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        bool valid = true;
        juce::String reason;
        LicenseService::checkVenueLicenseSync(venueId, valid, reason);

        if (! valid)
        {
            if (onDone)
                postOnMessageThread([onDone, reason]() { onDone(false, reason); });
            return;
        }

        UserPreferences::getInstance().setVenueId(venueId);

        if (onDone)
            postOnMessageThread([onDone]() { onDone(true, {}); });

        // Fire-and-forget update; never hold up UI transition.
        const auto uid = FirestoreClient::getInstance().getUserId();
        if (uid.isNotEmpty())
            touchLastAccess(venueId, uid);
    });
}

void LoginFlowController::requestVenueAccess(const juce::String& venueId,
                                             const juce::String& venueName,
                                             const juce::String& message,
                                             std::function<void(bool, juce::String)> onDone)
{
    juce::Thread::launch([venueId, venueName, message, onDone = std::move(onDone)]()
    {
        auto& fc = FirestoreClient::getInstance();
        const auto uid    = fc.getUserId();
        const auto email  = fc.getEmail().toLowerCase();
        const auto display = fc.getDisplayName().isEmpty() ? email : fc.getDisplayName();

        // Avoid duplicates
        juce::Array<juce::var> filters;
        filters.add(stringFilter("venueId",          "EQUAL", FC::stringValue(venueId)));
        filters.add(stringFilter("requestedByUserId","EQUAL", FC::stringValue(uid)));
        filters.add(stringFilter("status",           "EQUAL", FC::stringValue("pending")));
        auto query = buildQuery("venueJoinRequests", compositeAnd(filters));

        if (! fc.runQuery({}, query).isEmpty())
        {
            postOnMessageThread([onDone]() { if (onDone) onDone(true, "Request already pending"); });
            return;
        }

        auto now = juce::Time::getCurrentTime();
        juce::Time expires(now.toMilliseconds() + (juce::int64) 7 * 24 * 60 * 60 * 1000);

        auto fields = FC::makeFields({
            { "venueId",            FC::stringValue(venueId) },
            { "venueName",          FC::stringValue(venueName) },
            { "requestedByUserId",  FC::stringValue(uid) },
            { "requestedByEmail",   FC::stringValue(email) },
            { "requestedByName",    FC::stringValue(display) },
            { "message",            FC::stringValue(message) },
            { "status",             FC::stringValue("pending") },
            { "requestDate",        FC::timestampValue(now) },
            { "expirationDate",     FC::timestampValue(expires) }
        });

        auto created = fc.createDocument("venueJoinRequests", fields);
        const bool ok = created.isObject() && created.hasProperty("name");
        postOnMessageThread([onDone, ok]()
        {
            if (onDone) onDone(ok, ok ? juce::String() : "Could not send the request");
        });
    });
}
