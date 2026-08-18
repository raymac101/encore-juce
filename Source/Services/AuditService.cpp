/*
  ==============================================================================

    AuditService.cpp

    Migration of src/app/services/audit.service.ts.

    Writes audit records to four Firestore paths whenever a song has been
    played for more than 30 seconds (or until natural end).

  ==============================================================================
*/

#include "AuditService.h"
#include "FirestoreClient.h"

AuditService& AuditService::getInstance()
{
    static AuditService instance;
    return instance;
}

//==============================================================================
// Private helpers
//==============================================================================

namespace
{
    using FC = FirestoreClient;

    //--- Firestore utility helpers -----------------------------------------------

    juce::String valueAsString(const juce::var& v)
    {
        if (v.hasProperty("stringValue"))  return v.getProperty("stringValue",  "").toString();
        if (v.hasProperty("integerValue")) return v.getProperty("integerValue", "").toString();
        return {};
    }

    juce::int64 valueAsInt64(const juce::var& v)
    {
        if (v.hasProperty("integerValue"))
            return v.getProperty("integerValue", "").toString().getLargeIntValue();
        if (v.hasProperty("doubleValue"))
            return (juce::int64)(double) v.getProperty("doubleValue", 0.0);
        return 0;
    }

    /** Strip "projects/.../databases/(default)/documents/" prefix. */
    juce::String relPathFromDocName(const juce::String& docName)
    {
        const auto marker = juce::String("/documents/");
        auto idx = docName.indexOf(marker);
        if (idx < 0) return docName;
        return docName.substring(idx + marker.length());
    }

    //--- Firestore query helpers -------------------------------------------------

    juce::var fieldFilter(const juce::String& fieldPath,
                          const juce::String& op,
                          const juce::var& value)
    {
        juce::DynamicObject::Ptr field = new juce::DynamicObject();
        field->setProperty("fieldPath", fieldPath);

        juce::DynamicObject::Ptr filter = new juce::DynamicObject();
        filter->setProperty("field",  juce::var(field.get()));
        filter->setProperty("op",     op);
        filter->setProperty("value",  value);

        juce::DynamicObject::Ptr wrap = new juce::DynamicObject();
        wrap->setProperty("fieldFilter", juce::var(filter.get()));
        return juce::var(wrap.get());
    }

    juce::var buildSingleFieldQuery(const juce::String& collection,
                                    const juce::String& fieldPath,
                                    const juce::String& op,
                                    const juce::var& value)
    {
        juce::DynamicObject::Ptr fc = new juce::DynamicObject();
        fc->setProperty("collectionId", collection);
        juce::Array<juce::var> fromArr;
        fromArr.add(juce::var(fc.get()));

        juce::DynamicObject::Ptr q = new juce::DynamicObject();
        q->setProperty("from",  fromArr);
        q->setProperty("where", fieldFilter(fieldPath, op, value));
        return juce::var(q.get());
    }
}

//==============================================================================
// Static helpers
//==============================================================================

juce::String AuditService::determineSource(const juce::String& deviceId)
{
    if (deviceId.isEmpty() || deviceId == "unknown")
        return "unknown";

    if (deviceId == "local"
        || deviceId.toLowerCase().contains("host")
        || deviceId.toLowerCase().contains("encore"))
        return "host";

    return "phone";
}

juce::var AuditService::auditFields(const Audit& a)
{
    return FC::makeFields({
        { "venueId",   FC::stringValue(juce::String(a.venueId))   },
        { "venueName", FC::stringValue(juce::String(a.venueName)) },
        { "songName",  FC::stringValue(juce::String(a.songName))  },
        { "songId",    FC::stringValue(juce::String(a.songId))    },
        { "date",      FC::integerValue(a.date)                   },
        { "artist",    FC::stringValue(juce::String(a.artist))    },
        { "singerId",  FC::stringValue(juce::String(a.singerId))  },
        { "singerName",FC::stringValue(juce::String(a.singerName))},
        { "source",    FC::stringValue(juce::String(a.source))    },
        { "deviceId",  FC::stringValue(juce::String(a.deviceId))  },
    });
}

juce::var AuditService::userAuditFields(const UserAudit& u)
{
    // Build genres as a Firestore arrayValue of stringValues.
    juce::Array<juce::var> genreValues;
    for (auto& g : u.genres)
        genreValues.add(FC::stringValue(juce::String(g)));

    juce::DynamicObject::Ptr genreArr = new juce::DynamicObject();
    genreArr->setProperty("values", juce::var(genreValues));
    juce::DynamicObject::Ptr genreWrapper = new juce::DynamicObject();
    genreWrapper->setProperty("arrayValue", juce::var(genreArr.get()));

    return FC::makeFields({
        { "venueId",      FC::stringValue(juce::String(u.venueId))    },
        { "venueName",    FC::stringValue(juce::String(u.venueName))  },
        { "date",         FC::integerValue(u.date)                    },
        { "album",        FC::stringValue(juce::String(u.album))      },
        { "artist",       FC::stringValue(juce::String(u.artist))     },
        { "durationMS",   FC::integerValue(u.durationMS)              },
        { "explicit",     FC::booleanValue(u.isExplicit)              },
        { "genres",       juce::var(genreWrapper.get())               },
        { "id",           FC::stringValue(juce::String(u.id))         },
        { "imageUrl",     FC::stringValue(juce::String(u.imageUrl))   },
        { "keySignature", FC::stringValue(juce::String(u.keySignature)) },
        { "lyricUrl",     FC::stringValue(juce::String(u.lyricUrl))   },
        { "songName",     FC::stringValue(juce::String(u.songName))   },
        { "songId",       FC::stringValue(juce::String(u.songId))     },
        { "releaseDate",  FC::stringValue(juce::String(u.releaseDate)) },
        { "tempo",        FC::doubleValue(u.tempo)                    },
        { "type",         FC::stringValue(juce::String(u.type))       },
        { "singerName",   FC::stringValue(juce::String(u.singerName)) },
        { "avatar",       FC::stringValue(juce::String(u.avatar))     },
        { "profileId",    FC::stringValue(juce::String(u.profileId))  },
        { "deviceId",     FC::stringValue(juce::String(u.deviceId))   },
        { "foxId",        FC::stringValue(juce::String(u.foxId))      },
        { "songVersion",  FC::stringValue(juce::String(u.songVersion)) },
        { "pitch",        FC::doubleValue(u.pitch)                    },
        { "kjId",         FC::stringValue(juce::String(u.kjId))       },
    });
}

juce::var AuditService::memberHistoryMapValue(const MemberHistory& h)
{
    // Each MemberHistory entry is stored as a mapValue in the memberHistory array.
    juce::DynamicObject::Ptr fields = new juce::DynamicObject();
    fields->setProperty("artist",      FC::stringValue(juce::String(h.artist)));
    fields->setProperty("song",        FC::stringValue(juce::String(h.song)));
    fields->setProperty("songId",      FC::stringValue(juce::String(h.songId)));
    fields->setProperty("songVersion", FC::stringValue(juce::String(h.songVersion)));
    fields->setProperty("duration",    FC::integerValue(h.duration));
    fields->setProperty("pitch",       FC::doubleValue(h.pitch));
    fields->setProperty("kjId",        FC::stringValue(juce::String(h.kjId)));
    fields->setProperty("dateTime",    FC::integerValue(h.dateTime));

    juce::DynamicObject::Ptr map = new juce::DynamicObject();
    map->setProperty("fields", juce::var(fields.get()));

    juce::DynamicObject::Ptr wrapper = new juce::DynamicObject();
    wrapper->setProperty("mapValue", juce::var(map.get()));
    return juce::var(wrapper.get());
}

juce::var AuditService::memberFields(const Member& m)
{
    // Build memberHistory as a Firestore arrayValue of mapValues.
    juce::Array<juce::var> histValues;
    for (auto& h : m.memberHistory)
        histValues.add(memberHistoryMapValue(h));

    juce::DynamicObject::Ptr histArr = new juce::DynamicObject();
    histArr->setProperty("values", juce::var(histValues));
    juce::DynamicObject::Ptr histWrapper = new juce::DynamicObject();
    histWrapper->setProperty("arrayValue", juce::var(histArr.get()));

    return FC::makeFields({
        { "id",            FC::stringValue(juce::String(m.id))          },
        { "singerName",    FC::stringValue(juce::String(m.singerName))  },
        { "profileId",     FC::stringValue(juce::String(m.profileId))   },
        { "firstDate",     FC::integerValue(m.firstDate)                },
        { "lastDate",      FC::integerValue(m.lastDate)                 },
        { "memberHistory", juce::var(histWrapper.get())                 },
    });
}

//==============================================================================
// addAudit — main entry point
//==============================================================================

void AuditService::addAudit(const CdgSong&     song,
                             const Singers&     singer,
                             const QueueItem&   item,
                             const juce::String& venueId,
                             const juce::String& venueName,
                             const juce::String& kjId)
{
    if (venueId.isEmpty())
    {
        DBG("[AuditService] addAudit: no venueId, skipping.");
        return;
    }

    // Capture all data by value for the background thread.
    juce::Thread::launch([this, song, singer, item, venueId, venueName, kjId]()
    {
        const juce::int64 now = juce::Time::currentTimeMillis();

        //----------------------------------------------------------------------
        // 1. Build Audit (basic record, same shape as audit.model.ts)
        //----------------------------------------------------------------------
        Audit audit;
        audit.venueId   = venueId.toStdString();
        audit.venueName = venueName.toStdString();
        audit.songName  = song.songName;
        audit.songId    = song.id;
        audit.date      = now;
        audit.artist    = song.artistName;
        audit.singerId  = item.profileId;
        audit.singerName = singer.name;
        audit.source    = determineSource(juce::String(item.deviceId)).toStdString();
        audit.deviceId  = item.deviceId;

        auto aFields = auditFields(audit);

        // Write to global `audit` collection.
        FC::getInstance().createDocument("audit", aFields);

        // Write to venue recently-played subcollection.
        FC::getInstance().createDocument("venues/" + venueId + "/audit", aFields);

        //----------------------------------------------------------------------
        // 2. Build UserAudit (detailed per-user record, same shape as userAudit.model.ts)
        //    Only written when the singer has a profileId.
        //----------------------------------------------------------------------
        if (! juce::String(item.profileId).isEmpty())
        {
            UserAudit ua;
            ua.venueId      = venueId.toStdString();
            ua.venueName    = venueName.toStdString();
            ua.date         = now;
            ua.album        = song.id;          // CdgSong has no album field; use id as fallback
            ua.artist       = song.artistName;
            ua.durationMS   = song.durationMS;
            ua.isExplicit   = false;
            ua.genres       = song.genres;
            ua.id           = "";               // Firestore will assign; patched below
            ua.imageUrl     = song.imageUrl;
            ua.keySignature = song.keySignature;
            ua.lyricUrl     = "";               // CDG files have no lyric URL
            ua.songName     = song.songName;
            ua.songId       = song.id;
            ua.releaseDate  = song.releaseDate;
            ua.tempo        = song.tempo;
            ua.type         = "karaoke";
            ua.singerName   = singer.name;
            ua.avatar       = singer.avatar;
            ua.profileId    = item.profileId;
            ua.deviceId     = item.deviceId;
            ua.foxId        = item.foxId;
            ua.songVersion  = item.songVersion;
            ua.pitch        = item.pitch;
            ua.kjId         = kjId.toStdString();

            auto uaDoc = FC::getInstance().createDocument(
                "users/" + juce::String(item.profileId) + "/userAudit",
                userAuditFields(ua));

            // Back-patch the `id` field with the Firestore-generated doc ID.
            if (uaDoc.isObject())
            {
                const auto docName = uaDoc.getProperty("name", "").toString();
                const auto docId   = docName.fromLastOccurrenceOf("/", false, false);
                if (docId.isNotEmpty())
                {
                    const auto relPath = relPathFromDocName(docName)
                                        + "?updateMask.fieldPaths=id";
                    FC::getInstance().patchDocument(relPath, FC::makeFields({
                        { "id", FC::stringValue(docId) }
                    }));
                }
            }
        }

        //----------------------------------------------------------------------
        // 3. Upsert venues/{venueId}/membersAudit
        //    Mirrors Angular behaviour: if the singer already has a doc,
        //    append a new MemberHistory entry and update lastDate;
        //    otherwise create a new Member doc.
        //
        //    Locked end-to-end (query -> mutate -> write) so two addAudit()
        //    calls for the same singer can't race on the same stale snapshot.
        //----------------------------------------------------------------------
        const juce::ScopedLock membersAuditLock(membersAuditLock_);

        // Build the new MemberHistory entry.
        MemberHistory history;
        history.artist      = song.artistName;
        history.song        = song.songName;
        history.songId      = song.id;
        history.songVersion = item.songVersion;
        history.duration    = song.durationMS;
        history.pitch       = item.pitch;
        history.kjId        = kjId.toStdString();
        history.dateTime    = now;

        const auto membersAuditPath = "venues/" + venueId + "/membersAudit";

        // Query by profileId (or singerName as fallback if no profileId).
        const bool hasProfileId = juce::String(item.profileId).isNotEmpty();
        juce::Array<juce::var> matchingDocs;

        if (hasProfileId)
        {
            auto query = buildSingleFieldQuery("membersAudit",
                                               "profileId",
                                               "EQUAL",
                                               FC::stringValue(juce::String(item.profileId)));
            // runQuery on the venue's parent path.
            matchingDocs = FC::getInstance().runQuery("venues/" + venueId, query);
        }
        else
        {
            // No profileId — match by singerName for host-added singers.
            auto query = buildSingleFieldQuery("membersAudit",
                                               "singerName",
                                               "EQUAL",
                                               FC::stringValue(juce::String(singer.name)));
            matchingDocs = FC::getInstance().runQuery("venues/" + venueId, query);
        }

        // Filter out "no document" sentinel results that runQuery can return.
        juce::Array<juce::var> realDocs;
        for (auto& d : matchingDocs)
            if (d.isObject() && d.hasProperty("name"))
                realDocs.add(d);

        if (! realDocs.isEmpty())
        {
            //------------------------------------------------------------------
            // Member exists — read current memberHistory, append new entry,
            // PATCH back with updated array + lastDate.
            //------------------------------------------------------------------
            const auto& existingDoc = realDocs[0];
            const auto docName = existingDoc.getProperty("name", "").toString();
            const auto relPath = relPathFromDocName(docName);

            // Deserialise existing memberHistory arrayValue.
            auto fields = existingDoc.getProperty("fields", juce::var());

            juce::Array<juce::var> histValues;

            auto memberHistField = fields.getProperty("memberHistory", juce::var());
            if (memberHistField.hasProperty("arrayValue"))
            {
                auto arrValues = memberHistField
                                     .getProperty("arrayValue", juce::var())
                                     .getProperty("values", juce::var());
                if (arrValues.isArray())
                    for (int i = 0; i < arrValues.size(); ++i)
                        histValues.add(arrValues[i]);
            }

            // Append the new entry (equivalent of arrayUnion).
            histValues.add(memberHistoryMapValue(history));

            // Rebuild the arrayValue wrapper.
            juce::DynamicObject::Ptr histArr = new juce::DynamicObject();
            histArr->setProperty("values", juce::var(histValues));
            juce::DynamicObject::Ptr histWrapper = new juce::DynamicObject();
            histWrapper->setProperty("arrayValue", juce::var(histArr.get()));

            const auto patchPath = relPath
                + "?updateMask.fieldPaths=memberHistory"
                + "&updateMask.fieldPaths=lastDate";
            FC::getInstance().patchDocument(patchPath, FC::makeFields({
                { "memberHistory", juce::var(histWrapper.get()) },
                { "lastDate",      FC::integerValue(now)        },
            }));
        }
        else
        {
            //------------------------------------------------------------------
            // Member does not exist — create a new doc and then back-patch id.
            //------------------------------------------------------------------
            Member newMember;
            newMember.id          = "";
            newMember.singerName  = singer.name;
            newMember.profileId   = item.profileId;
            newMember.firstDate   = now;
            newMember.lastDate    = now;
            newMember.memberHistory.push_back(history);

            auto newDoc = FC::getInstance().createDocument(membersAuditPath,
                                                           memberFields(newMember));
            if (newDoc.isObject())
            {
                const auto docName = newDoc.getProperty("name", "").toString();
                const auto docId   = docName.fromLastOccurrenceOf("/", false, false);
                if (docId.isNotEmpty())
                {
                    const auto relPath = relPathFromDocName(docName)
                                        + "?updateMask.fieldPaths=id";
                    FC::getInstance().patchDocument(relPath, FC::makeFields({
                        { "id", FC::stringValue(docId) }
                    }));
                }
            }
        }
    });
}
