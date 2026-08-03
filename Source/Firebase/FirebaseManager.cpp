/*
  ==============================================================================

    FirebaseManager.cpp

    Firebase C++ SDK integration.

    SDK setup (one-time):
      macOS:
        brew install --cask firebase-cpp-sdk   # or download manually
        export FIREBASE_SDK_ROOT=/path/to/firebase_cpp_sdk
        brew install openssl grpc              # transitive deps

      Windows:
        vcpkg install firebase-cpp-sdk
        set FIREBASE_SDK_ROOT=%VCPKG_ROOT%\installed\x64-windows

    The .jucer sets include/library search paths from ${FIREBASE_SDK_ROOT}.

  ==============================================================================
*/

#include "FirebaseManager.h"
#include "FirebaseConfig.h"
#include "../Services/FirestoreClient.h"

// Firebase C++ SDK
#include <firebase/app.h>
#include <firebase/auth.h>
#include <firebase/firestore.h>

using firebase::App;
using firebase::AppOptions;
using firebase::Future;
namespace fbauth = firebase::auth;
namespace fbfs   = firebase::firestore;

//==============================================================================
// Singleton
//==============================================================================

FirebaseManager& FirebaseManager::getInstance()
{
    static FirebaseManager instance;
    return instance;
}

FirebaseManager::~FirebaseManager()
{
    shutdown();
}

//==============================================================================
// Lifecycle
//==============================================================================

void FirebaseManager::initialise()
{
    if (initialised_.load())
        return;

    AppOptions opts;
    opts.set_api_key    (FirebaseConfig::apiKey.toRawUTF8());
    opts.set_project_id (FirebaseConfig::projectId.toRawUTF8());
    opts.set_app_id     (FirebaseConfig::appId.toRawUTF8());
    opts.set_database_url(FirebaseConfig::databaseUrl.toRawUTF8());
    opts.set_storage_bucket(FirebaseConfig::storageBucket.toRawUTF8());

    app_  = App::Create(opts);
    auth_ = fbauth::Auth::GetAuth(app_);
    db_   = fbfs::Firestore::GetInstance(app_);

    // Restore a previous session if the SDK has cached credentials.
    if (auth_->current_user() != nullptr)
    {
        juce::ScopedLock sl(lock_);
        currentUserId_    = auth_->current_user()->uid();
        currentUserEmail_ = auth_->current_user()->email();
        authenticated_.store(true);
    }

    initialised_.store(true);
    notifyConnection(ConnectionStatus::Connected, "Firebase initialised");
}

void FirebaseManager::shutdown()
{
    if (!initialised_.load())
        return;

    stopQueueListener();
    stopVenueListener();

    // Firestore and Auth are owned by the SDK; do not delete.
    // App::Delete frees everything.
    if (app_ != nullptr)
    {
        App::RemoveApp(app_);   // Releases App + all child services
        app_  = nullptr;
        auth_ = nullptr;
        db_   = nullptr;
    }

    authenticated_.store(false);
    initialised_.store(false);
}

//==============================================================================
// Authentication
//==============================================================================

void FirebaseManager::signInWithEmail(const juce::String& email,
                                      const juce::String& password,
                                      AuthCallback onDone)
{
    jassert(initialised_.load());

    juce::Thread::launch([this, email, password, cb = std::move(onDone)]()
    {
        auto future = auth_->SignInWithEmailAndPassword(
            email.toRawUTF8(), password.toRawUTF8());
        future.OnCompletion([this, cb](const Future<fbauth::AuthResult>& f)
        {
            AuthResult result;
            if (f.status() == firebase::kFutureStatusComplete && f.error() == 0)
            {
                auto* user = auth_->current_user();
                result.ok    = true;
                result.userId = user ? user->uid()   : "";
                result.email  = user ? user->email() : "";

                {
                    juce::ScopedLock sl(lock_);
                    currentUserId_    = result.userId;
                    currentUserEmail_ = result.email;
                }
                authenticated_.store(true);
            }
            else
            {
                result.ok           = false;
                result.errorCode    = f.error_message();
                result.errorMessage = f.error_message();
                authenticated_.store(false);
            }

            juce::MessageManager::callAsync([cb, result]() { if (cb) cb(result); });
        });

        // Block until the future completes so the lambda above fires while
        // `auth_` is still valid.
        while (future.status() == firebase::kFutureStatusPending)
            juce::Thread::sleep(20);
    });
}

void FirebaseManager::signUpWithEmail(const juce::String& email,
                                      const juce::String& password,
                                      AuthCallback onDone)
{
    jassert(initialised_.load());

    juce::Thread::launch([this, email, password, cb = std::move(onDone)]()
    {
        auto future = auth_->CreateUserWithEmailAndPassword(
            email.toRawUTF8(), password.toRawUTF8());
        future.OnCompletion([this, cb](const Future<fbauth::AuthResult>& f)
        {
            AuthResult result;
            if (f.status() == firebase::kFutureStatusComplete && f.error() == 0)
            {
                auto* user = auth_->current_user();
                result.ok           = true;
                result.isNewAccount = true;
                result.userId       = user ? user->uid()   : "";
                result.email        = user ? user->email() : "";

                {
                    juce::ScopedLock sl(lock_);
                    currentUserId_    = result.userId;
                    currentUserEmail_ = result.email;
                }
                authenticated_.store(true);
            }
            else
            {
                result.ok           = false;
                result.errorCode    = f.error_message();
                result.errorMessage = f.error_message();
            }

            juce::MessageManager::callAsync([cb, result]() { if (cb) cb(result); });
        });

        while (future.status() == firebase::kFutureStatusPending)
            juce::Thread::sleep(20);
    });
}

void FirebaseManager::signInWithGoogle(AuthCallback onDone)
{
    // Desktop OAuth requires a local redirect HTTP server (not yet built).
    // Surface a user-friendly error and fall through to email/password.
    AuthResult result;
    result.ok           = false;
    result.errorCode    = "OAUTH_NOT_IMPLEMENTED";
    result.errorMessage = "Google sign-in is not available in the desktop app. "
                          "Please use email and password.";
    juce::MessageManager::callAsync([onDone, result]() { if (onDone) onDone(result); });
}

void FirebaseManager::signOut()
{
    if (auth_ != nullptr)
        auth_->SignOut();

    stopQueueListener();
    stopVenueListener();

    {
        juce::ScopedLock sl(lock_);
        currentUserId_.clear();
        currentUserEmail_.clear();
        currentVenueId_.clear();
        currentVenue_ = VenueItem{};
    }

    authenticated_.store(false);
}

juce::String FirebaseManager::getCurrentUserId() const
{
    juce::ScopedLock sl(lock_);
    return currentUserId_;
}

juce::String FirebaseManager::getCurrentUserEmail() const
{
    juce::ScopedLock sl(lock_);
    return currentUserEmail_;
}

//==============================================================================
// Venue (one-shot load via FirestoreClient)
//==============================================================================

void FirebaseManager::loadVenue(const juce::String& venueId,
                                std::function<void(bool, VenueItem, juce::String)> onDone)
{
    juce::Thread::launch([this, venueId, cb = std::move(onDone)]()
    {
        int status = 0;
        auto doc = FirestoreClient::getInstance()
                       .getDocument("venues/" + venueId, &status);

        bool ok = (status >= 200 && status < 300);
        VenueItem venue;
        juce::String error;

        if (ok)
        {
            auto flat = FirestoreClient::unwrapFields(doc);
            venue = VenueItem::fromJson(juce::JSON::toString(flat));
            venue.id = venueId.toStdString();

            juce::ScopedLock sl(lock_);
            currentVenue_   = venue;
            currentVenueId_ = venueId;
        }
        else
        {
            error = "HTTP " + juce::String(status) + " loading venue " + venueId;
        }

        juce::MessageManager::callAsync([cb, ok, venue, error]()
        {
            if (cb) cb(ok, venue, error);
        });
    });
}

VenueItem FirebaseManager::getCurrentVenue() const
{
    juce::ScopedLock sl(lock_);
    return currentVenue_;
}

juce::String FirebaseManager::getCurrentVenueId() const
{
    juce::ScopedLock sl(lock_);
    return currentVenueId_;
}

//==============================================================================
// Real-time Firestore listeners
//==============================================================================

void FirebaseManager::startQueueListener(const juce::String& venueId,
                                         QueueCallback onChange)
{
    jassert(initialised_.load());
    stopQueueListener();

    auto colRef = db_->Collection("venues")
                     .Document(venueId.toStdString())
                     .Collection("queue");

    auto reg = colRef.AddSnapshotListener(
        [this, cb = std::move(onChange)]
        (const fbfs::QuerySnapshot& snap,
         fbfs::Error error,
         const std::string& errMsg)
        {
            if (error != fbfs::kErrorOk)
            {
                notifyError("queue_listener", juce::String(errMsg.c_str()));
                return;
            }

            std::vector<Singers> singers = parseSingers(static_cast<const void*>(&snap));

            juce::MessageManager::callAsync([cb, singers]() mutable
            {
                if (cb) cb(std::move(singers));
            });
        });

    queueReg_ = std::make_unique<fbfs::ListenerRegistration>(std::move(reg));
}

void FirebaseManager::startVenueListener(const juce::String& venueId,
                                         VenueCallback onChange)
{
    jassert(initialised_.load());
    stopVenueListener();

    auto docRef = db_->Collection("venues").Document(venueId.toStdString());

    auto reg = docRef.AddSnapshotListener(
        [this, venueId, cb = std::move(onChange)]
        (const fbfs::DocumentSnapshot& snap,
         fbfs::Error error,
         const std::string& errMsg)
        {
            if (error != fbfs::kErrorOk)
            {
                notifyError("venue_listener", juce::String(errMsg.c_str()));
                return;
            }

            VenueItem venue = parseVenue(static_cast<const void*>(&snap));
            venue.id = venueId.toStdString();

            {
                juce::ScopedLock sl(lock_);
                currentVenue_   = venue;
                currentVenueId_ = venueId;
            }

            juce::MessageManager::callAsync([cb, venue]() mutable
            {
                if (cb) cb(venue);
            });
        });

    venueReg_ = std::make_unique<fbfs::ListenerRegistration>(std::move(reg));
}

void FirebaseManager::stopQueueListener()
{
    if (queueReg_ != nullptr)
    {
        queueReg_->Remove();
        queueReg_.reset();
    }
}

void FirebaseManager::stopVenueListener()
{
    if (venueReg_ != nullptr)
    {
        venueReg_->Remove();
        venueReg_.reset();
    }
}

//==============================================================================
// Queue writes  (delegate to FirestoreClient on a background thread)
//==============================================================================

void FirebaseManager::addSongToQueue(const juce::String& venueId,
                                     const QueueItem& item,
                                     WriteCallback onDone)
{
    juce::Thread::launch([venueId, item, cb = std::move(onDone)]()
    {
        // Delegates to FirestoreClient for now; QueueService::appendSong handles
        // full singer-lookup logic when called directly from MainComponent.
        bool ok = FirestoreClient::getInstance()
                      .createDocument(
                          "venues/" + venueId + "/queue",
                          FirestoreClient::makeFields({
                              { "singerName", FirestoreClient::stringValue(item.singerName) },
                              { "songId",     FirestoreClient::stringValue(item.songId)     },
                              { "songName",   FirestoreClient::stringValue(item.songName)   },
                              { "status",     FirestoreClient::stringValue(item.status)     }
                          }))
                      .isObject();

        juce::MessageManager::callAsync([cb, ok]()
        {
            if (cb) cb(ok, ok ? "" : "Failed to write queue item");
        });
    });
}

void FirebaseManager::removeSongFromQueue(const juce::String& venueId,
                                          const QueueItem& item,
                                          WriteCallback onDone)
{
    juce::Thread::launch([venueId, item, cb = std::move(onDone)]()
    {
        // Deletion logic is in QueueService::removeSong; forward there.
        // Placeholder implementation: no-op + callback.
        juce::ignoreUnused(venueId, item);
        juce::MessageManager::callAsync([cb]()
        {
            if (cb) cb(false, "removeSongFromQueue: delegate to QueueService::removeSong");
        });
    });
}

void FirebaseManager::updateQueueItem(const juce::String& venueId,
                                      const QueueItem& item,
                                      WriteCallback onDone)
{
    juce::Thread::launch([venueId, item, cb = std::move(onDone)]()
    {
        juce::String path = "venues/" + venueId + "/queue/" + juce::String(item.singerName);
        bool ok = FirestoreClient::getInstance().patchDocument(
            path,
            FirestoreClient::makeFields({
                { "status", FirestoreClient::stringValue(item.status) },
                { "pitch",  FirestoreClient::doubleValue(item.pitch)  }
            }));

        juce::MessageManager::callAsync([cb, ok]()
        {
            if (cb) cb(ok, ok ? "" : "updateQueueItem patch failed");
        });
    });
}

void FirebaseManager::deleteSinger(const juce::String& venueId,
                                   const juce::String& singerName,
                                   WriteCallback onDone)
{
    juce::Thread::launch([venueId, singerName, cb = std::move(onDone)]()
    {
        // Delegate to QueueService which knows the actual Firestore document ID.
        juce::ignoreUnused(venueId, singerName);
        juce::MessageManager::callAsync([cb]()
        {
            if (cb) cb(false, "deleteSinger: delegate to QueueService::deleteSinger");
        });
    });
}

//==============================================================================
// Snapshot parsers
//==============================================================================

std::vector<Singers> FirebaseManager::parseSingers(const void* ptr)
{
    const auto& snap = *static_cast<const fbfs::QuerySnapshot*>(ptr);
    std::vector<Singers> result;

    for (const auto& doc : snap.documents())
    {
        Singers singer;
        singer.id   = doc.id();

        auto getData = [&](const char* field) -> std::string {
            auto v = doc.Get(field);
            return v.is_valid() ? v.string_value() : "";
        };
        auto getInt = [&](const char* field, int def = 0) -> int {
            auto v = doc.Get(field);
            return v.is_valid() ? static_cast<int>(v.integer_value()) : def;
        };

        singer.name          = getData("singerName");
        singer.avatar        = getData("avatar");
        singer.deviceId      = getData("deviceId");
        singer.order         = getInt("order");
        singer.rotationOrder = getInt("rotationOrder");
        singer.strikes       = getInt("strikes");

        // Parse the `songs` array field.
        auto songsVar = doc.Get("songs");
        if (songsVar.is_valid() && songsVar.type() == fbfs::FieldValue::Type::kArray)
        {
            for (const auto& sv : songsVar.array_value())
            {
                if (!sv.is_valid() || sv.type() != fbfs::FieldValue::Type::kMap)
                    continue;

                QueueItem qi;
                const auto& m = sv.map_value();
                auto mGet = [&](const char* k) -> std::string {
                    auto it = m.find(k);
                    return it != m.end() ? it->second.string_value() : "";
                };
                auto mGetInt = [&](const char* k, int def = 0) -> int {
                    auto it = m.find(k);
                    return it != m.end() ? static_cast<int>(it->second.integer_value()) : def;
                };

                qi.id         = mGet("id");
                qi.singerName = singer.name;
                qi.songId     = mGet("songId");
                qi.songName   = mGet("songName");
                qi.songArtist = mGet("songArtist");
                qi.songVersion= mGet("songVersion");
                qi.status     = mGet("status");
                qi.profileId  = mGet("profileId");
                qi.deviceId   = mGet("deviceId");
                qi.duration   = mGetInt("duration");
                qi.order      = mGetInt("order");
                qi.songOrder  = mGetInt("songOrder");

                auto pitchIt = m.find("pitch");
                qi.pitch = (pitchIt != m.end()) ?
                    static_cast<float>(pitchIt->second.double_value()) : 0.0f;

                singer.songs.push_back(std::move(qi));
            }
        }

        result.push_back(std::move(singer));
    }

    // Sort by rotation order to match Angular's display order.
    std::sort(result.begin(), result.end(), [](const Singers& a, const Singers& b) {
        return a.rotationOrder < b.rotationOrder;
    });

    return result;
}

VenueItem FirebaseManager::parseVenue(const void* ptr)
{
    const auto& snap = *static_cast<const fbfs::DocumentSnapshot*>(ptr);

    if (!snap.exists())
        return {};

    // Flatten the Firestore typed fields into a plain JSON object and reuse
    // VenueItem::fromJson so there is a single parsing path.
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    auto setStr = [&](const char* field) {
        auto v = snap.Get(field);
        if (v.is_valid())
            obj->setProperty(field, juce::String(v.string_value().c_str()));
    };
    auto setInt = [&](const char* field) {
        auto v = snap.Get(field);
        if (v.is_valid())
            obj->setProperty(field, static_cast<int>(v.integer_value()));
    };
    auto setBool = [&](const char* field) {
        auto v = snap.Get(field);
        if (v.is_valid())
            obj->setProperty(field, v.boolean_value());
    };

    setStr ("name");        setStr ("address");    setStr ("city");
    setStr ("country");     setStr ("code");        setStr ("codePlus");
    setStr ("logoUrl");     setStr ("songBookUrl"); setStr ("language");
    setStr ("timezone");    setStr ("taggVersion"); setStr ("version");
    setStr ("encoreBackground"); setStr ("encodeBackgroundSize");
    setInt ("numSongs");    setInt ("numSingers");  setInt ("numStrikes");
    setInt ("background");
    setBool("repeatSongs"); setBool("autoapprove");
    setBool("showOnlineSongs"); setBool("showOnlineSongsEncore");

    return VenueItem::fromJson(juce::JSON::toString(juce::var(obj.get())));
}

//==============================================================================
// Internal helpers
//==============================================================================

void FirebaseManager::notifyConnection(ConnectionStatus s, const juce::String& msg)
{
    juce::MessageManager::callAsync([this, s, msg]()
    {
        if (connectionCb_) connectionCb_(s, msg);
    });
}

void FirebaseManager::notifyError(const juce::String& ctx, const juce::String& msg)
{
    juce::MessageManager::callAsync([this, ctx, msg]()
    {
        if (errorCb_) errorCb_(ctx, msg);
    });
}
