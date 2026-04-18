/*
  ==============================================================================

    FirebaseManager.h
    Created: 15 Apr 2026 7:18:45pm
    Author:  GitHub Copilot

    Firebase integration manager for real-time karaoke data synchronization

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/VenueItem.h"
#include "../Models/QueueItem.h"
#include "../Models/Singers.h"

//==============================================================================
/**
    Firebase manager for real-time synchronization of karaoke session data.
    Handles authentication, Firestore operations, and real-time listeners.
*/
class FirebaseManager : public juce::Thread
{
public:
    //==============================================================================
    // Connection Status
    enum class ConnectionStatus
    {
        Disconnected,
        Connecting,
        Connected,
        Error,
        Reconnecting
    };
    
    //==============================================================================
    // Callback Types
    using ConnectionCallback = std::function<void(ConnectionStatus, const juce::String&)>;
    using QueueUpdateCallback = std::function<void(const std::vector<QueueItem>&)>;
    using SingerUpdateCallback = std::function<void(const std::vector<Singer>&)>;
    using VenueUpdateCallback = std::function<void(const VenueItem&)>;
    using ErrorCallback = std::function<void(const juce::String&, const juce::String&)>;
    
    //==============================================================================
    FirebaseManager();
    ~FirebaseManager() override;
    
    //==============================================================================
    // Connection Management
    void initialize(const juce::String& apiKey, const juce::String& projectId);
    void shutdown();
    bool isConnected() const { return connectionStatus == ConnectionStatus::Connected; }
    ConnectionStatus getConnectionStatus() const { return connectionStatus; }
    
    //==============================================================================
    // Authentication
    void signInAnonymously();
    void signInWithVenueCode(const juce::String& venueCode, const juce::String& password = {});
    void signOut();
    bool isAuthenticated() const { return authenticated; }
    juce::String getCurrentUserId() const { return currentUserId; }
    
    //==============================================================================
    // Venue Management
    void setActiveVenue(const juce::String& venueId);
    void createVenue(const VenueItem& venue);
    void updateVenue(const VenueItem& venue);
    void deleteVenue(const juce::String& venueId);
    
    juce::String getActiveVenueId() const { return activeVenueId; }
    VenueItem getCurrentVenue() const { return currentVenue; }
    
    //==============================================================================
    // Queue Management
    void addSongToQueue(const QueueItem& queueItem);
    void removeSongFromQueue(const juce::String& queueItemId);
    void updateQueueItem(const QueueItem& queueItem);
    void moveSongInQueue(const juce::String& queueItemId, int newPosition);
    void clearQueue();
    
    std::vector<QueueItem> getCurrentQueue() const;
    
    //==============================================================================
    // Singer Management
    void addSinger(const Singer& singer);
    void updateSinger(const Singer& singer);
    void removeSinger(const juce::String& singerId);
    void setSingerActive(const juce::String& singerId, bool active);
    
    std::vector<Singer> getCurrentSingers() const;
    
    //==============================================================================
    // Real-time Listeners
    void enableQueueUpdates(bool enabled);
    void enableSingerUpdates(bool enabled);
    void enableVenueUpdates(bool enabled);
    
    //==============================================================================
    // Callback Registration
    void setConnectionCallback(ConnectionCallback callback);
    void setQueueUpdateCallback(QueueUpdateCallback callback);
    void setSingerUpdateCallback(SingerUpdateCallback callback);
    void setVenueUpdateCallback(VenueUpdateCallback callback);
    void setErrorCallback(ErrorCallback callback);
    
    //==============================================================================
    // Offline Mode
    void enableOfflineMode(bool enabled);
    bool isOfflineModeEnabled() const { return offlineModeEnabled; }
    void syncOfflineData(); // Sync when connection restored
    
    //==============================================================================
    // Statistics and Analytics
    void logEvent(const juce::String& eventName, const juce::var& parameters = {});
    void trackSongPlay(const juce::String& songId, const juce::String& artist, 
                      const juce::String& title);
    void trackUserAction(const juce::String& action, const juce::var& data = {});
    
    //==============================================================================
    // Data Caching
    void setCacheEnabled(bool enabled);
    void clearCache();
    size_t getCacheSize() const;
    
    //==============================================================================
    // Thread Interface
    void run() override;

private:
    //==============================================================================
    // Connection State
    std::atomic<ConnectionStatus> connectionStatus { ConnectionStatus::Disconnected };
    std::atomic<bool> authenticated { false };
    std::atomic<bool> initialized { false };
    
    juce::String apiKey;
    juce::String projectId;
    juce::String currentUserId;
    juce::String activeVenueId;
    
    //==============================================================================
    // Current Data Cache
    VenueItem currentVenue;
    std::vector<QueueItem> currentQueue;
    std::vector<Singer> currentSingers;
    mutable juce::ReadWriteLock dataLock;
    
    //==============================================================================
    // Callbacks
    ConnectionCallback connectionCallback;
    QueueUpdateCallback queueUpdateCallback;
    SingerUpdateCallback singerUpdateCallback;
    VenueUpdateCallback venueUpdateCallback;
    ErrorCallback errorCallback;
    
    //==============================================================================
    // Real-time Listener State
    std::atomic<bool> queueListenerEnabled { false };
    std::atomic<bool> singerListenerEnabled { false };
    std::atomic<bool> venueListenerEnabled { false };
    
    //==============================================================================
    // Offline Support
    std::atomic<bool> offlineModeEnabled { false };
    std::queue<juce::String> pendingOperations; // JSON queue of operations to sync
    juce::File offlineCacheFile;
    
    //==============================================================================
    // Network Management
    juce::CriticalSection networkLock;
    std::unique_ptr<juce::WebInputStream> activeConnection;
    juce::Time lastHeartbeat;
    int reconnectAttempts = 0;
    static const int maxReconnectAttempts = 5;
    
    //==============================================================================
    // Internal Methods
    void updateConnectionStatus(ConnectionStatus status, const juce::String& message = {});
    void handleConnectionError(const juce::String& error);
    void attemptReconnection();
    
    // Data Operations
    void fetchInitialData();
    void setupRealtimeListeners();
    void handleQueueSnapshot(const juce::var& data);
    void handleSingerSnapshot(const juce::var& data);
    void handleVenueSnapshot(const juce::var& data);
    
    // Network Requests
    bool sendFirestoreRequest(const juce::String& method, const juce::String& path, 
                             const juce::var& data = {});
    juce::var performHttpRequest(const juce::URL& url, const juce::String& method,
                                const juce::String& postData = {});
    
    // Offline Cache Management
    void saveToOfflineCache();
    void loadFromOfflineCache();
    void queueOfflineOperation(const juce::String& operation);
    void processOfflineQueue();
    
    // Authentication Helpers
    void handleAuthenticationResponse(const juce::var& response);
    juce::String generateAuthToken() const;
    
    // Data Validation
    bool validateVenueData(const VenueItem& venue) const;
    bool validateQueueItem(const QueueItem& item) const;
    bool validateSinger(const Singer& singer) const;
    
    // Heartbeat and Connection Monitoring
    void sendHeartbeat();
    void checkConnectionHealth();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FirebaseManager)
};