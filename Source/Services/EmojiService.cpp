/*
  ==============================================================================

    EmojiService.cpp

  ==============================================================================
*/

#include "EmojiService.h"
#include "VenueService.h"

//==============================================================================
EmojiService& EmojiService::getInstance()
{
    static EmojiService instance;
    return instance;
}

EmojiService::~EmojiService()
{
    stop();
}

void EmojiService::start(const juce::String& venueId)
{
    if (venueId.isEmpty())
    {
        DBG ("[Emoji] start ignored: empty venueId");
        return;
    }

    // Restart cleanly when switching venue.
    if (running_ && venueId == venueId_)
        return;

    stop();

    venueId_ = venueId;
    running_ = true;
    seenIds_.clear();
    DBG ("[Emoji] start polling venues/" << venueId_ << "/emojis every "
         << pollIntervalMs_ << "ms");

    // Fire one poll immediately, then on a recurring timer.
    juce::Timer::startTimer(pollIntervalMs_);
    poll();
}

void EmojiService::stop()
{
    if (! running_)
        return;
    running_ = false;
    juce::Timer::stopTimer();
    seenIds_.clear();
    DBG ("[Emoji] stop");
}

void EmojiService::timerCallback()
{
    poll();
}

void EmojiService::poll()
{
    if (! running_ || pollInFlight_ || venueId_.isEmpty())
        return;

    pollInFlight_ = true;
    const auto venueId = venueId_;

    VenueService::getInstance().getNewEmoji(venueId,
        [this, venueId](bool ok, std::vector<Emoji> list, juce::String /*error*/)
        {
            pollInFlight_ = false;

            // Drop late results if the user switched venue or stopped while
            // this fetch was in flight.
            if (! running_ || venueId != venueId_ || ! ok)
                return;

            std::vector<Emoji> fresh;
            fresh.reserve(list.size());
            for (auto& e : list)
            {
                if (e.id.empty())
                    continue;
                if (seenIds_.insert(e.id).second)
                    fresh.push_back(std::move(e));
            }

            if (! fresh.empty() && onNewEmoji)
                onNewEmoji(fresh);
        });
}
