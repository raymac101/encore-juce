/*
  ==============================================================================

    GlobalProgressService.cpp

  ==============================================================================
*/

#include "GlobalProgressService.h"

GlobalProgressService& GlobalProgressService::getInstance()
{
    static GlobalProgressService instance;
    return instance;
}

int GlobalProgressService::beginTask(const juce::String& message)
{
    bool busy = false;
    juce::String displayMessage;
    int taskId = 0;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        taskId = nextTaskId_++;
        activeTasks_[taskId] = { message, juce::Time::currentTimeMillis() };
        busy = ! activeTasks_.empty();
        displayMessage = resolveDisplayMessageLocked();
    }

    notifyListenersAsync(busy, displayMessage);
    return taskId;
}

void GlobalProgressService::endTask(int taskId)
{
    bool busy = false;
    juce::String displayMessage;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto erased = activeTasks_.erase(taskId);
        if (erased == 0)
            return;

        busy = ! activeTasks_.empty();
        displayMessage = resolveDisplayMessageLocked();
    }

    notifyListenersAsync(busy, displayMessage);
}

bool GlobalProgressService::isBusy() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return ! activeTasks_.empty();
}

juce::String GlobalProgressService::getDisplayMessage() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return resolveDisplayMessageLocked();
}

void GlobalProgressService::addListener(Listener* listener)
{
    if (listener == nullptr)
        return;

    listeners_.add(listener);
}

void GlobalProgressService::removeListener(Listener* listener)
{
    if (listener == nullptr)
        return;

    listeners_.remove(listener);
}

void GlobalProgressService::notifyListenersAsync(bool busy, juce::String message)
{
    juce::MessageManager::callAsync([this, busy, message = std::move(message)]()
    {
        listeners_.call([&](Listener& l)
        {
            l.globalProgressChanged(busy, message);
        });
    });
}

juce::String GlobalProgressService::resolveDisplayMessageLocked() const
{
    if (activeTasks_.empty())
        return {};

    const TaskInfo* newest = nullptr;
    for (const auto& kv : activeTasks_)
    {
        const auto& info = kv.second;
        if (newest == nullptr || info.startedAtMs >= newest->startedAtMs)
            newest = &info;
    }

    if (newest == nullptr || newest->message.isEmpty())
        return "Working...";

    return newest->message;
}
