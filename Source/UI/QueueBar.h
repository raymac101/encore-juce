/*
  ==============================================================================

    QueueBar.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Right-side singer queue bar.
    - Venue header (name + code)
    - "Now Singing" card with play/pause overlay on avatar
    - Drag-reorderable singer list with round indicators (blue first, red last)
    - Each singer card shows avatar, name, performance icons, and 1-5 song chips
    - Bottom status bar: singer count, song count, total time, queue toggle,
      auto-play toggle, delay slider, clear queue button
    - Left-edge resize handle

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/Singers.h"
#include "../Models/QueueItem.h"
#include "../Localization/LocalizationManager.h"
#include <vector>
#include <functional>

//==============================================================================
/**
    Right-hand queue bar displaying the karaoke singer rotation.
*/
class QueueBar : public juce::Component
{
public:
    QueueBar();
    ~QueueBar() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Resize handle
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    //==============================================================================
    // Data
    void setVenueInfo(const juce::String& name, const juce::String& code);
    void setNowPlaying(const Singers& singer);
    void clearNowPlaying();
    void setSingers(const std::vector<Singers>& singers);
    void addSinger(const Singers& singer);
    void removeSinger(int index);
    void moveSinger(int fromIndex, int toIndex);

    // State
    void setPlaying(bool playing);
    void setQueueRunning(bool running);
    void setAutoPlay(bool enabled);
    void setDelaySec(int seconds);

    // Width
    int getBarWidth() const { return barWidth; }
    void setBarWidth(int w);

    // Localization
    void updateAllText();

    //==============================================================================
    // Callbacks
    std::function<void(int singerIndex)>              onPlaySinger;
    std::function<void()>                             onPlayCurrent;
    std::function<void()>                             onPauseCurrent;
    std::function<void(int from, int to)>             onReorder;
    std::function<void()>                             onClearQueue;
    std::function<void(bool)>                         onQueueToggled;
    std::function<void(bool)>                         onAutoPlayToggled;
    std::function<void(int)>                          onDelayChanged;
    std::function<void(int singerIdx, int songIdx)>   onSongClicked;
    std::function<void(int)>                          onWidthChanged;

private:
    //==============================================================================
    // Internal component: a single singer row in the queue list
    class SingerRow : public juce::Component
    {
    public:
        SingerRow();
        void paint(juce::Graphics& g) override;
        void mouseEnter(const juce::MouseEvent&) override { hovering = true;  repaint(); }
        void mouseExit(const juce::MouseEvent&) override  { hovering = false; repaint(); }
        void mouseUp(const juce::MouseEvent& e) override;

        Singers singer;
        int     index = 0;
        bool    isFirst = false;   // blue border (round leader)
        bool    isLast  = false;   // red border (round tail)
        bool    isHost  = false;   // red border (host pin) — wins over isFirst
        bool    hovering = false;

        std::function<void(int)> onPlayClicked;
        std::function<void(int, int)> onSongChipClicked;
    };

    //==============================================================================
    // Now-playing card
    class NowPlayingCard : public juce::Component
    {
    public:
        NowPlayingCard();
        void paint(juce::Graphics& g) override;
        void mouseEnter(const juce::MouseEvent&) override { hovering = true;  repaint(); }
        void mouseExit(const juce::MouseEvent&) override  { hovering = false; repaint(); }
        void mouseUp(const juce::MouseEvent& e) override;

        Singers singer;
        bool    isPlaying = false;
        bool    hovering = false;
        bool    hasSinger = false;

        std::function<void()> onPlayClicked;
        std::function<void()> onPauseClicked;
    };

    //==============================================================================
    // Sub-components
    std::unique_ptr<NowPlayingCard>         nowPlayingCard;
    juce::OwnedArray<SingerRow>             singerRows;
    juce::Viewport                          listViewport;
    juce::Component                         listContent;

    // Venue header labels
    std::unique_ptr<juce::Label> venueNameLabel;
    std::unique_ptr<juce::Label> venueCodeLabel;
    std::unique_ptr<juce::Label> nowSingingLabel;

    // Bottom status bar
    std::unique_ptr<juce::Label>      singerCountLabel;
    std::unique_ptr<juce::Label>      songCountLabel;
    std::unique_ptr<juce::Label>      totalTimeLabel;
    std::unique_ptr<juce::TextButton> clearQueueButton;
    std::unique_ptr<juce::ToggleButton> queueToggle;
    std::unique_ptr<juce::ToggleButton> autoPlayToggle;
    std::unique_ptr<juce::Slider>     delaySlider;
    std::unique_ptr<juce::Label>      delayLabel;

    //==============================================================================
    // State
    std::vector<Singers> singers;
    Singers              currentSinger;
    bool                 hasCurrentSinger = false;
    bool                 isPlayingState = false;
    bool                 queueRunning = false;
    bool                 autoPlayEnabled = false;
    int                  delaySec = 0;

    // Sizing
    int  barWidth = 280;
    int  minWidth = 60;
    int  maxWidth = 600;
    int  resizeHandleWidth = 5;
    bool draggingResize = false;
    int  dragStartX = 0;
    int  dragStartWidth = 0;

    // Colours
    juce::Colour bgColour          { 0xff262626 };
    juce::Colour headerBgColour    { 0xff333333 };
    juce::Colour nowPlayingBg      { 0xff30daff };
    juce::Colour cardBgColour      { 0xff262626 };
    juce::Colour firstBorderColour { 0xff30daff };  // blue = round leader
    juce::Colour lastBorderColour  { 0xffdb3d40 };  // red  = round tail
    juce::Colour textColour        { 0xffe4e4e4 };
    juce::Colour accentColour      { 0xff30daff };
    juce::Colour separatorColour   { 0xff474747 };

    //==============================================================================
    void rebuildSingerRows();
    void updateStatusLabels();
    bool isOverResizeHandle(const juce::Point<int>& pos) const;

    static constexpr int venueHeaderHeight   = 56;
    static constexpr int nowPlayingHeight    = 100;
    static constexpr int statusBarHeight     = 110;
    static constexpr int singerRowHeight     = 64;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QueueBar)
};
