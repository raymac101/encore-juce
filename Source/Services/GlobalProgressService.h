/*
  ==============================================================================

    GlobalProgressService.h

    App-wide busy-state coordinator for long-running tasks.

    Any subsystem can call beginTask()/endTask(). UI components (e.g. BottomBar)
    can listen for state changes and render a shared loading affordance.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <map>
#include <mutex>

class GlobalProgressService
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void globalProgressChanged(bool isBusy, const juce::String& message) = 0;
    };

    static GlobalProgressService& getInstance();

    /** Start a long-running task and return a task id for endTask(). */
    int beginTask(const juce::String& message = {});

    /** End a task previously returned by beginTask(). Safe to call redundantly. */
    void endTask(int taskId);

    bool isBusy() const;
    juce::String getDisplayMessage() const;

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    GlobalProgressService() = default;

    struct TaskInfo
    {
        juce::String message;
        juce::int64 startedAtMs = 0;
    };

    void notifyListenersAsync(bool busy, juce::String message);
    juce::String resolveDisplayMessageLocked() const;

    mutable std::mutex mutex_;
    int nextTaskId_ = 1;
    std::map<int, TaskInfo> activeTasks_;

    juce::ListenerList<Listener> listeners_;
};
