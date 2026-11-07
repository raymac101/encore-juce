/*
  ==============================================================================

    ProfileSync.cpp

  ==============================================================================
*/

#include "ProfileSync.h"
#include "FirestoreClient.h"

namespace
{
    using FC = FirestoreClient;

    // Encore's account-creation default -- never worth sharing with TAGG.
    const juce::String kPlaceholderStageName = "Anonymous";
    const juce::String kPlaceholderAvatar    = "assets/images/AvatarWhite.png";

    juce::String str (const std::string& s) { return juce::String (s).trim(); }

    bool isRealStageName (const juce::String& s)
    {
        return s.isNotEmpty() && ! s.equalsIgnoreCase (kPlaceholderStageName);
    }

    juce::var emptyArrayValue()
    {
        juce::DynamicObject::Ptr arr = new juce::DynamicObject();
        juce::DynamicObject::Ptr wrap = new juce::DynamicObject();
        wrap->setProperty ("arrayValue", juce::var (arr.get()));
        return juce::var (wrap.get());
    }

    // The identity fields both apps share, as (Firestore field, Host member).
    struct SharedField
    {
        const char* name;
        std::string Host::* member;
    };

    const SharedField kSharedFields[] = {
        { "stageName", &Host::stageName },
        { "avatarUrl", &Host::avatarUrl },
        { "fullName",  &Host::fullName  },
        { "city",      &Host::city      },
        { "country",   &Host::country   },
        { "gender",    &Host::gender    },
        { "birthday",  &Host::birthday  },
    };

    // Value of a shared field as it should appear in TAGG, or empty when
    // Encore only holds a placeholder for it.
    juce::String taggValueFor (const Host& h, const SharedField& f)
    {
        const auto v = str (h.*(f.member));
        if (f.member == &Host::avatarUrl) return ProfileSync::toTaggAvatar (v);
        if (f.member == &Host::stageName) return isRealStageName (v) ? v : juce::String();
        return v;
    }

    juce::String maskFor (const juce::StringArray& names)
    {
        juce::String q;
        for (auto& n : names)
            q << (q.isEmpty() ? "?" : "&") << "updateMask.fieldPaths=" << n;
        return q;
    }

    bool patchFields (const juce::String& docPath, juce::DynamicObject::Ptr fields)
    {
        juce::StringArray names;
        for (auto& p : fields->getProperties())
            names.add (p.name.toString());
        if (names.isEmpty())
            return true;
        return FC::getInstance().patchDocument (docPath + maskFor (names), juce::var (fields.get()));
    }

    void setString (juce::DynamicObject::Ptr& fields, const juce::String& name, const juce::String& value)
    {
        fields->setProperty (name, FC::stringValue (value));
        if (name == "stageName")
            fields->setProperty ("stageNameLower", FC::stringValue (value.toLowerCase()));
    }
}

//==============================================================================
juce::String ProfileSync::toTaggAvatar (const juce::String& encoreAvatar)
{
    const auto a = encoreAvatar.trim();
    if (a.isEmpty() || a == kPlaceholderAvatar)
        return {};

    // "assets/icon/1064391.png" -> "preset:1064391"
    if (a.startsWithIgnoreCase ("assets/icon/"))
        return "preset:" + a.fromLastOccurrenceOf ("/", false, false)
                            .upToLastOccurrenceOf (".", false, false);

    return a;
}

bool ProfileSync::isStageNameAvailable (const juce::String& stageName, const juce::String& uid)
{
    const auto lower = stageName.trim().toLowerCase();
    if (lower.isEmpty())
        return false;

    // { from:[{collectionId:"users"}], where:{fieldFilter:{stageNameLower == x}}, limit:5 }
    juce::DynamicObject::Ptr from = new juce::DynamicObject();
    from->setProperty ("collectionId", "users");
    juce::Array<juce::var> fromArr;
    fromArr.add (juce::var (from.get()));

    juce::DynamicObject::Ptr fieldRef = new juce::DynamicObject();
    fieldRef->setProperty ("fieldPath", "stageNameLower");
    juce::DynamicObject::Ptr filter = new juce::DynamicObject();
    filter->setProperty ("field", juce::var (fieldRef.get()));
    filter->setProperty ("op", "EQUAL");
    filter->setProperty ("value", FC::stringValue (lower));
    juce::DynamicObject::Ptr where = new juce::DynamicObject();
    where->setProperty ("fieldFilter", juce::var (filter.get()));

    juce::DynamicObject::Ptr q = new juce::DynamicObject();
    q->setProperty ("from", fromArr);
    q->setProperty ("where", juce::var (where.get()));
    q->setProperty ("limit", 5);

    for (auto& d : FC::getInstance().runQuery ({}, juce::var (q.get())))
    {
        const auto docId = d.getProperty ("name", "").toString().fromLastOccurrenceOf ("/", false, false);
        if (docId.isNotEmpty() && docId != uid)
            return false;
    }
    return true;
}

//==============================================================================
void ProfileSync::syncOnLogin (Host& host)
{
    const auto uid = str (host.userId);
    if (uid.isEmpty())
        return;

    int status = 0;
    const auto doc = FC::getInstance().getDocument ("users/" + uid, &status);

    if (status == 404)
    {
        // Host has never used TAGG: give them a TAGG profile carrying their
        // Encore identity, so signing into TAGG later shows the same person.
        createTaggProfile (host);
        if (host.profileId != host.userId)
        {
            host.profileId = host.userId;
            patchFields ("hosts/" + uid, [&] {
                juce::DynamicObject::Ptr f = new juce::DynamicObject();
                f->setProperty ("profileId", FC::stringValue (uid));
                return f;
            }());
        }
        return;
    }

    if (status != 200 || ! doc.hasProperty ("fields"))
        return;   // offline / transient error -- keep the hosts/ values

    const Host before = host;
    juce::DynamicObject::Ptr backfill = new juce::DynamicObject();

    for (auto& f : kSharedFields)
    {
        const auto taggValue = FC::readString (doc, f.name).trim();
        if (taggValue.isNotEmpty())
        {
            host.*(f.member) = taggValue.toStdString();   // TAGG wins
            continue;
        }

        // TAGG has nothing here but Encore does -- fill TAGG in.
        const auto encoreValue = taggValueFor (host, f);
        if (encoreValue.isEmpty())
            continue;
        if (f.member == &Host::stageName && ! isStageNameAvailable (encoreValue, uid))
            continue;
        setString (backfill, f.name, encoreValue);
    }

    if (FC::readString (doc, "stageName").isNotEmpty()
        && FC::readString (doc, "stageNameLower").isEmpty())
        backfill->setProperty ("stageNameLower",
                               FC::stringValue (FC::readString (doc, "stageName").trim().toLowerCase()));

    patchFields ("users/" + uid, backfill);

    host.profileId = host.userId;

    // Mirror the merged identity into hosts/{uid} so every Encore reader of
    // that doc (company member lists, admin tools...) sees the same values.
    juce::DynamicObject::Ptr mirror = new juce::DynamicObject();
    for (auto& f : kSharedFields)
        if (host.*(f.member) != before.*(f.member))
            setString (mirror, f.name, str (host.*(f.member)));
    if (host.profileId != before.profileId)
        mirror->setProperty ("profileId", FC::stringValue (uid));

    patchFields ("hosts/" + uid, mirror);
}

bool ProfileSync::pushSharedFields (const Host& host, bool includeStageName)
{
    const auto uid = str (host.userId);
    if (uid.isEmpty())
        return false;

    juce::DynamicObject::Ptr fields = new juce::DynamicObject();
    for (auto& f : kSharedFields)
    {
        if (f.member == &Host::stageName && ! includeStageName)
            continue;

        const auto v = taggValueFor (host, f);
        // Never blank out TAGG's avatar or stage name with an Encore
        // placeholder; the other fields may legitimately be cleared.
        if (v.isEmpty() && (f.member == &Host::avatarUrl || f.member == &Host::stageName))
            continue;
        setString (fields, f.name, v);
    }

    return patchFields ("users/" + uid, fields);
}

void ProfileSync::createTaggProfile (const Host& host)
{
    const auto uid = str (host.userId);
    if (uid.isEmpty())
        return;

    int status = 0;
    FC::getInstance().getDocument ("users/" + uid, &status);
    if (status != 404)
        return;   // exists already, or we can't tell -- never clobber

    const auto now = juce::Time::getCurrentTime();

    // Same base shape as TAGG's RegisterExtScreen::baseProfile().
    juce::DynamicObject::Ptr fields = new juce::DynamicObject();
    fields->setProperty ("userId",        FC::stringValue (uid));
    fields->setProperty ("email",         FC::stringValue (str (host.email)));
    fields->setProperty ("signUpDate",    FC::timestampValue (now));
    fields->setProperty ("lastLogin",     FC::timestampValue (now));
    fields->setProperty ("friends",       emptyArrayValue());
    fields->setProperty ("blocked",       emptyArrayValue());
    fields->setProperty ("notifications", emptyArrayValue());
    fields->setProperty ("geoShow",       FC::booleanValue (false));
    fields->setProperty ("emailShow",     FC::booleanValue (false));
    fields->setProperty ("notify",        FC::booleanValue (true));
    fields->setProperty ("shareActivity", FC::booleanValue (true));
    fields->setProperty ("online",        FC::booleanValue (false));
    fields->setProperty ("foxId",         FC::stringValue (""));
    fields->setProperty ("type",          FC::stringValue ("user"));

    for (auto& f : kSharedFields)
    {
        const auto v = taggValueFor (host, f);
        if (v.isEmpty())
            continue;
        if (f.member == &Host::stageName && ! isStageNameAvailable (v, uid))
            continue;   // TAGG will ask them to pick one on first sign-in
        setString (fields, f.name, v);
    }

    FC::getInstance().createDocument ("users", juce::var (fields.get()), uid);
}
