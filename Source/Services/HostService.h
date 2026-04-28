/*
  ==============================================================================

    HostService.h

    Tiny in-memory holder for the currently signed-in Host profile (loaded
    by LoginFlowController during the post-auth flow). Other UI surfaces
    (notably the QueueBar host pin) read it after login completes.

    Thread-safe: setCurrent / getCurrent / clear may be called from any
    thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/Host.h"

class HostService
{
public:
    static HostService& getInstance()
    {
        static HostService inst;
        return inst;
    }

    void setCurrent(const Host& h)
    {
        const juce::ScopedLock sl(lock_);
        current_ = h;
        hasCurrent_ = true;
    }

    Host getCurrent() const
    {
        const juce::ScopedLock sl(lock_);
        return current_;
    }

    bool hasCurrent() const
    {
        const juce::ScopedLock sl(lock_);
        return hasCurrent_;
    }

    void clear()
    {
        const juce::ScopedLock sl(lock_);
        current_ = {};
        hasCurrent_ = false;
    }

private:
    HostService() = default;

    mutable juce::CriticalSection lock_;
    Host current_;
    bool hasCurrent_ = false;

    JUCE_DECLARE_NON_COPYABLE(HostService)
};
