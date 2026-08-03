/*
  ==============================================================================

    ChannelPluginChain.cpp

  ==============================================================================
*/

#include "ChannelPluginChain.h"

//==============================================================================
ChannelPluginChain::~ChannelPluginChain()
{
    // Destructor runs on the message thread (the owning AudioEngine /
    // BackgroundMusicPlayer is torn down there). The caller is responsible
    // for having stopped the audio callback first, so it's safe to delete
    // directly here rather than deferring via callAsync.
    for (auto& slot : slots_)
    {
        if (auto* instance = slot.active.exchange(nullptr))
            delete instance;
    }
}

void ChannelPluginChain::prepare(double sampleRate, int maxBlockSize, int numChannels)
{
    sampleRate_   = sampleRate;
    maxBlockSize_ = maxBlockSize;
    numChannels_  = numChannels;

    for (auto& slot : slots_)
        if (auto* instance = slot.active.load())
            instance->prepareToPlay(sampleRate_, maxBlockSize_);
}

void ChannelPluginChain::deleteOnMessageThread(juce::AudioPluginInstance* instance)
{
    if (instance == nullptr)
        return;

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        delete instance;
    else
        juce::MessageManager::callAsync([instance]() { delete instance; });
}

void ChannelPluginChain::loadPlugin(int slotIndex,
                                    const juce::PluginDescription& desc,
                                    juce::AudioPluginFormatManager& formatManager,
                                    LoadCallback onDone,
                                    const juce::MemoryBlock* initialState)
{
    jassert(slotIndex >= 0 && slotIndex < numSlots);
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    const double sampleRate = sampleRate_;
    const int    blockSize  = maxBlockSize_;
    const juce::MemoryBlock stateToRestore = (initialState != nullptr ? *initialState : juce::MemoryBlock());
    const bool hasStateToRestore = (initialState != nullptr);

    formatManager.createPluginInstanceAsync(
        desc, sampleRate, blockSize,
        [this, slotIndex, onDone = std::move(onDone), stateToRestore, hasStateToRestore]
        (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) mutable
        {
            if (instance == nullptr)
            {
                if (onDone)
                    onDone(false, error.isNotEmpty() ? error : "Failed to load plugin.");
                return;
            }

            instance->prepareToPlay(sampleRate_, maxBlockSize_);

            if (hasStateToRestore && stateToRestore.getSize() > 0)
                instance->setStateInformation(stateToRestore.getData(), (int) stateToRestore.getSize());

            auto& slot = slots_[(size_t) slotIndex];
            auto* raw = instance.release();
            auto* previous = slot.active.exchange(raw);
            slot.description = raw->getPluginDescription();

            deleteOnMessageThread(previous);

            if (onDone)
                onDone(true, {});
        });
}

void ChannelPluginChain::unloadPlugin(int slotIndex)
{
    jassert(slotIndex >= 0 && slotIndex < numSlots);
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto& slot = slots_[(size_t) slotIndex];
    auto* previous = slot.active.exchange(nullptr);
    slot.description = {};

    deleteOnMessageThread(previous);
}

void ChannelPluginChain::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (bypassed_.load())
        return;

    const auto startTicks = juce::Time::getHighResolutionTicks();

    for (auto& slot : slots_)
        if (auto* instance = slot.active.load())
            instance->processBlock(buffer, midi);

    const auto elapsedSeconds = juce::Time::highResolutionTicksToSeconds(
        juce::Time::getHighResolutionTicks() - startTicks);
    const double budgetSeconds = sampleRate_ > 0.0
        ? (double) buffer.getNumSamples() / sampleRate_
        : 0.0;

    overBudget_ = (budgetSeconds > 0.0 && elapsedSeconds > budgetSeconds);
}

bool ChannelPluginChain::hasPluginInSlot(int slotIndex) const noexcept
{
    jassert(slotIndex >= 0 && slotIndex < numSlots);
    return slots_[(size_t) slotIndex].active.load() != nullptr;
}

juce::PluginDescription ChannelPluginChain::getDescriptionForSlot(int slotIndex) const
{
    jassert(slotIndex >= 0 && slotIndex < numSlots);
    return slots_[(size_t) slotIndex].description;
}

juce::AudioProcessorEditor* ChannelPluginChain::getEditorForSlot(int slotIndex)
{
    jassert(slotIndex >= 0 && slotIndex < numSlots);
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (auto* instance = slots_[(size_t) slotIndex].active.load())
        return instance->createEditorIfNeeded();

    return nullptr;
}

int ChannelPluginChain::getLatencySamples() const
{
    int total = 0;
    for (auto& slot : slots_)
        if (auto* instance = slot.active.load())
            total += instance->getLatencySamples();
    return total;
}

juce::MemoryBlock ChannelPluginChain::getStateForSlot(int slotIndex) const
{
    jassert(slotIndex >= 0 && slotIndex < numSlots);
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    juce::MemoryBlock block;
    if (auto* instance = slots_[(size_t) slotIndex].active.load())
        instance->getStateInformation(block);
    return block;
}
