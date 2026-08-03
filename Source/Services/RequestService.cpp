/*
  ==============================================================================

    RequestService.cpp

  ==============================================================================
*/

#include "RequestService.h"
#include "FirestoreClient.h"

namespace
{
    using FC = FirestoreClient;

    //--- Firestore typed-value readers (work on the inner wrapper var) -------

    juce::String valueAsString(const juce::var& v)
    {
        if (v.hasProperty("stringValue"))    return v.getProperty("stringValue", "").toString();
        if (v.hasProperty("integerValue"))   return v.getProperty("integerValue", "").toString();
        if (v.hasProperty("doubleValue"))    return juce::String((double) v.getProperty("doubleValue", 0.0));
        if (v.hasProperty("booleanValue"))   return ((bool) v.getProperty("booleanValue", false)) ? "true" : "false";
        return {};
    }

    int valueAsInt(const juce::var& v, int dflt = 0)
    {
        if (v.hasProperty("integerValue"))
            return (int) v.getProperty("integerValue", "").toString().getLargeIntValue();
        if (v.hasProperty("doubleValue"))
            return (int) (double) v.getProperty("doubleValue", 0.0);
        if (v.hasProperty("stringValue"))
            return v.getProperty("stringValue", "").toString().getIntValue();
        return dflt;
    }

    double valueAsDouble(const juce::var& v, double dflt = 0.0)
    {
        if (v.hasProperty("doubleValue"))
            return (double) v.getProperty("doubleValue", dflt);
        if (v.hasProperty("integerValue"))
            return (double) v.getProperty("integerValue", "").toString().getLargeIntValue();
        if (v.hasProperty("stringValue"))
            return v.getProperty("stringValue", "").toString().getDoubleValue();
        return dflt;
    }

    bool valueAsBool(const juce::var& v, bool dflt = false)
    {
        if (v.hasProperty("booleanValue"))
            return (bool) v.getProperty("booleanValue", dflt);
        if (v.hasProperty("stringValue"))
        {
            auto s = v.getProperty("stringValue", "").toString().toLowerCase();
            return s == "true" || s == "1" || s == "yes";
        }
        return dflt;
    }

    juce::var fieldByName(const juce::var& fields, const juce::String& name)
    {
        return fields.getProperty(juce::Identifier(name), juce::var());
    }

    juce::String docIdFromName(const juce::String& docName)
    {
        // "projects/X/databases/(default)/documents/venues/Y/requested/<id>"
        return docName.fromLastOccurrenceOf("/", false, false);
    }

    QueueItem itemFromDoc(const juce::var& doc)
    {
        auto fields = doc.getProperty("fields", juce::var());

        QueueItem q;
        q.id           = docIdFromName(doc.getProperty("name", "").toString()).toStdString();
        if (q.id.empty())
            q.id       = valueAsString(fieldByName(fields, "id")).toStdString();
        q.deviceId     = valueAsString(fieldByName(fields, "deviceId")).toStdString();
        q.profileId    = valueAsString(fieldByName(fields, "profileId")).toStdString();
        q.foxId        = valueAsString(fieldByName(fields, "foxId")).toStdString();
        q.singerName   = valueAsString(fieldByName(fields, "singerName")).toStdString();
        q.singerAvatar = valueAsString(fieldByName(fields, "avatar")).toStdString();
        q.songId       = valueAsString(fieldByName(fields, "songId")).toStdString();
        q.songName     = valueAsString(fieldByName(fields, "song")).toStdString();
        q.songArtist   = valueAsString(fieldByName(fields, "artist")).toStdString();
        q.songVersion  = valueAsString(fieldByName(fields, "songVersion")).toStdString();
        q.duration     = valueAsInt   (fieldByName(fields, "duration"));
        q.order        = valueAsInt   (fieldByName(fields, "order"));
        q.songOrder    = valueAsInt   (fieldByName(fields, "songOrder"));
        q.pitch        = (float) valueAsDouble(fieldByName(fields, "pitch"), 1.0);
        q.status       = valueAsString(fieldByName(fields, "status")).toStdString();
        q.action       = valueAsString(fieldByName(fields, "action")).toStdString();
        q.reason       = valueAsString(fieldByName(fields, "reason")).toStdString();
        q.time         = valueAsString(fieldByName(fields, "time")).toStdString();
        q.alerts       = valueAsBool  (fieldByName(fields, "addedAlert"))
                       || valueAsBool (fieldByName(fields, "singingAlert"))
                       || valueAsBool (fieldByName(fields, "nextAlert"));
        return q;
    }
}

//==============================================================================
RequestService& RequestService::getInstance()
{
    static RequestService instance;
    return instance;
}

RequestService::RequestService() = default;

RequestService::~RequestService()
{
    stop();
}

void RequestService::start(const juce::String& venueId)
{
    if (venueId.isEmpty())
    {
        DBG ("[Request] start ignored: empty venueId");
        return;
    }

    // Restart cleanly when switching venue.
    if (running_ && venueId == venueId_)
        return;

    stop();

    venueId_ = venueId;
    running_ = true;
    seenStatus_.clear();
    DBG ("[Request] start polling venues/" << venueId_ << "/requested every "
         << pollIntervalMs_ << "ms");

    // Fire one poll immediately, then on a recurring timer.
    juce::Timer::startTimer(pollIntervalMs_);
    poll();
}

void RequestService::stop()
{
    if (! running_)
        return;
    running_ = false;
    juce::Timer::stopTimer();
    seenStatus_.clear();
    DBG ("[Request] stop");
}

void RequestService::timerCallback()
{
    poll();
}

void RequestService::poll()
{
    if (! running_ || pollInFlight_ || venueId_.isEmpty())
        return;

    pollInFlight_ = true;
    const auto venueId = venueId_;
    juce::WeakReference<juce::Timer> wr; // no-op; we capture by raw

    juce::Thread::launch([this, venueId]
    {
        const auto path = "venues/" + venueId + "/requested";
        auto docs = FC::getInstance().listCollection(path, 200);

        juce::MessageManager::callAsync([this, venueId, docs = std::move(docs)]() mutable
        {
            // Drop late results if user switched venue or stopped while we
            // were in flight.
            if (running_ && venueId == venueId_)
                dispatch(docs);
            pollInFlight_ = false;
        });
    });
}

void RequestService::dispatch(const juce::Array<juce::var>& docs)
{
    // Track which docs are still present this cycle so we can prune the
    // seen map.
    std::unordered_map<std::string, std::string> nowSeen;
    nowSeen.reserve((size_t) docs.size());

    for (auto& d : docs)
    {
        auto item = itemFromDoc(d);
        if (item.id.empty())
            continue;

        const auto status = juce::String(item.status).toLowerCase();
        const auto action = juce::String(item.action).toLowerCase();

        // Build a synthetic key that combines status + action so that flips
        // like new->approved or approved->approveddelete are treated as
        // distinct events.
        const std::string key = item.status + "|" + item.action;
        nowSeen[item.id] = key;

        auto prev = seenStatus_.find(item.id);
        const bool firstTime = (prev == seenStatus_.end());
        const bool changed   = ! firstTime && prev->second != key;

        if (! firstTime && ! changed)
            continue;  // already dispatched this state for this doc

        // Order matches Angular's filter precedence — delete wins over the
        // status-based dispatch, then status takes over.
        if (action == "delete")
        {
            DBG ("[Request] DELETE id=" << item.id << " song='" << item.songName
                 << "' singer='" << item.singerName << "'");
            if (onDeleteRequest)
                onDeleteRequest(item);
        }
        else if (status == "new")
        {
            DBG ("[Request] NEW id=" << item.id << " song='" << item.songName
                 << "' singer='" << item.singerName << "'");
            if (onNewRequest)
                onNewRequest(item);
        }
        else if (status == "approved")
        {
            DBG ("[Request] APPROVED id=" << item.id << " song='" << item.songName
                 << "' singer='" << item.singerName << "'");
            if (onApprovedRequest)
                onApprovedRequest(item);
        }
        else if (status == "rejected")
        {
            DBG ("[Request] REJECTED id=" << item.id << " song='" << item.songName
                 << "' reason='" << item.reason << "'");
            if (onRejectedRequest)
                onRejectedRequest(item);
        }
        // "approvedpending" and other transient states are ignored — we are
        // waiting for the mobile client to flip them to "approved".

        seenStatus_[item.id] = key;
    }

    // Forget docs that have disappeared (deleted from the collection) so
    // they can fire again if re-added.
    for (auto it = seenStatus_.begin(); it != seenStatus_.end(); )
    {
        if (nowSeen.find(it->first) == nowSeen.end())
            it = seenStatus_.erase(it);
        else
            ++it;
    }
}

//==============================================================================
void RequestService::patchStatus(const juce::String& venueId,
                                 const juce::String& itemId,
                                 const juce::String& status,
                                 const juce::String& reason)
{
    if (venueId.isEmpty() || itemId.isEmpty() || status.isEmpty())
    {
        DBG ("[Request] patchStatus skipped (missing arg)");
        return;
    }

    juce::Thread::launch([venueId, itemId, status, reason]
    {
        const auto path = "venues/" + venueId + "/requested/" + itemId;
        juce::URL url(path);

        std::vector<std::pair<juce::String, juce::var>> entries;
        entries.emplace_back("status", FC::stringValue(status));
        if (reason.isNotEmpty())
            entries.emplace_back("reason", FC::stringValue(reason));

        // Build update mask query string ?updateMask.fieldPaths=...
        juce::String maskedPath = path + "?updateMask.fieldPaths=status";
        if (reason.isNotEmpty())
            maskedPath += "&updateMask.fieldPaths=reason";

        std::initializer_list<std::pair<juce::String, juce::var>> init = {};
        // We can't pass a runtime list to makeFields, so build manually.
        juce::DynamicObject::Ptr fields = new juce::DynamicObject();
        fields->setProperty("status", FC::stringValue(status));
        if (reason.isNotEmpty())
            fields->setProperty("reason", FC::stringValue(reason));

        const bool ok = FC::getInstance().patchDocument(maskedPath, juce::var(fields.get()));
        DBG ("[Request] patchStatus " << itemId << " -> " << status
             << (reason.isNotEmpty() ? (" (" + reason + ")") : juce::String())
             << "  ok=" << (ok ? 1 : 0));
    });
}

void RequestService::deleteRequested(const juce::String& venueId,
                                     const juce::String& itemId)
{
    if (venueId.isEmpty() || itemId.isEmpty())
        return;

    juce::Thread::launch([venueId, itemId]
    {
        const auto path = "venues/" + venueId + "/requested/" + itemId;
        const bool ok = FC::getInstance().deleteDocument(path);
        DBG ("[Request] delete " << itemId << "  ok=" << (ok ? 1 : 0));
    });
}
