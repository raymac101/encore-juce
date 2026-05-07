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
class QueueBar : public juce::Component,
                 public juce::DragAndDropContainer,
                 public juce::Timer
{
public:
    QueueBar();
    ~QueueBar() override { stopTimer(); }

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Resize handle
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    // juce::Timer — drives countdown between songs
    void timerCallback() override;

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

    // Countdown (shown between songs when autoplay + delay > 0)
    void startCountdown(int seconds);
    void stopCountdown();

    // Read-only accessors used by MainComponent
    bool isAutoPlayEnabled() const noexcept { return autoPlayEnabled; }
    int  getDelaySec()       const noexcept { return delaySec; }

    // When true, all incoming requests from mobile (TAGG) are auto-rejected
    // with a "no longer accepting song requests" reason. Defaults to false.
    void setQueueClosed (bool closed) { queueClosed = closed; }
    bool isQueueClosed() const noexcept { return queueClosed; }

    // Read-only access to the live queue state — needed for the
    // request-pipeline checks in MainComponent (max songs per singer,
    // duplicate detection, etc.).
    const std::vector<Singers>& getSingers() const noexcept { return singers; }

    // Width
    int getBarWidth() const { return barWidth; }
    void setBarWidth(int w);

    // Expanded mode (queue takes the main workspace area)
    void setExpanded(bool shouldExpand);
    bool isExpanded() const noexcept { return expandedMode; }

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
    std::function<void(bool)>                         onExpandToggled;

    // Context-menu actions — wired by MainComponent
    std::function<void(int singerIndex)>  onRemoveSinger;     // remove singer from queue
    std::function<void(int singerIndex)>  onMoveSingerUp;     // move up in order
    std::function<void(int singerIndex)>  onMoveSingerDown;   // move down in order
    std::function<void()>                 onReturnCurrentToQueueNext; // now-playing → front
    std::function<void()>                 onReturnCurrentToQueueEnd;  // now-playing → back
    std::function<void()>                 onSkipCurrentSinger;        // skip + clear now-playing
    std::function<void()>                 onCountdownFinished;        // delay countdown elapsed
    std::function<void()>                 onAddSinger;        // KJ manually adds a singer

private:
    class ExpandArrowButton : public juce::Button,
                              private juce::Timer
    {
    public:
        ExpandArrowButton();
        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        void setExpanded(bool expanded);

    private:
        void timerCallback() override;
        float angle = 0.0f;
        float targetAngle = 0.0f;
    };

    //==============================================================================
    // Internal component: a single singer row in the queue list
    class SingerRow : public juce::Component
    {
    public:
        SingerRow();
        void paint(juce::Graphics& g) override;
        void mouseEnter(const juce::MouseEvent&) override { hovering = true;  repaint(); }
        void mouseExit (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp   (const juce::MouseEvent& e) override;

        // Hit-testing helpers (also used by paint to highlight hover zones).
        juce::Rectangle<int> getAvatarRect()      const;
        juce::Rectangle<int> getMenuButtonRect()  const;   // 3-dot tap zone
        int  songChipIndexAt (juce::Point<int> p) const;   // -1 if none
        bool isOverAvatar    (juce::Point<int> p) const;
        bool isOverMenuButton(juce::Point<int> p) const;

        Singers singer;
        int     index = 0;
        bool    isFirst      = false;  // blue border (round leader)
        bool    isLast       = false;  // red border (round tail)
        bool    isHost       = false;  // green border (host pin)
        bool    isNewlyAdded = false;  // yellow-green glow on first appearance
        bool    hovering    = false;
        bool    hoverAvatar = false;
        bool    hoverMenu   = false;   // cursor/touch over 3-dot button
        int     hoverSongIdx = -1;
        juce::Image avatarImage;

        std::function<void(int)>      onPlayClicked;
        std::function<void(int, int)> onSongChipClicked;
        std::function<void(int)>      onMoveUpClicked;
        std::function<void(int)>      onMoveDownClicked;
        std::function<void(int)>      onRemoveClicked;
    };

    //==============================================================================
    // Now-playing card
    class NowPlayingCard : public juce::Component
    {
    public:
        NowPlayingCard();
        void paint(juce::Graphics& g) override;
        void mouseEnter(const juce::MouseEvent&) override { hovering = true;  repaint(); }
        void mouseExit (const juce::MouseEvent&) override { hovering = false; hoverMenu = false; repaint(); }
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseUp   (const juce::MouseEvent& e) override;

        Singers singer;
        bool    isPlaying = false;
        bool    hovering = false;
        bool    hoverMenu = false;   // cursor/touch over 3-dot button
        bool    hasSinger = false;
        juce::Image avatarImage;

        juce::Rectangle<int> getMenuButtonRect() const;
        bool isOverMenuButton(juce::Point<int> p) const;

        std::function<void()> onPlayClicked;
        std::function<void()> onPauseClicked;
        std::function<void()> onReturnToQueueNext;
        std::function<void()> onReturnToQueueEnd;
        std::function<void()> onSkipAndRemove;
    };

    //==============================================================================
    // List container — accepts the singer-row drag and computes the new
    // index from the drop position. Owned by `listViewport`.
    class ListContent : public juce::Component,
                        public juce::DragAndDropTarget
    {
    public:
        ListContent (QueueBar& o) : owner (o) {}

        bool isInterestedInDragSource (const SourceDetails& d) override;
        void itemDragMove (const SourceDetails& d) override;
        void itemDragExit (const SourceDetails&) override;
        void itemDropped (const SourceDetails& d) override;
        void paintOverChildren (juce::Graphics& g) override;

    private:
        QueueBar& owner;
        int      dropIndicatorY = -1;     // legacy line indicator (collapsed mode)
        juce::Rectangle<int> dropIndicatorRect;
    };

    //==============================================================================
    // Sub-components
    std::unique_ptr<NowPlayingCard>         nowPlayingCard;
    juce::OwnedArray<SingerRow>             singerRows;
    juce::Viewport                          listViewport;
    ListContent                             listContent { *this };

    // Venue header labels
    std::unique_ptr<juce::Label> venueNameLabel;
    std::unique_ptr<juce::Label> venueCodeLabel;
    std::unique_ptr<juce::Label> nowSingingLabel;
    std::unique_ptr<ExpandArrowButton> expandButton;

    // Bottom status bar
    std::unique_ptr<juce::Label>      singerCountLabel;
    std::unique_ptr<juce::Label>      songCountLabel;
    std::unique_ptr<juce::Label>      totalTimeLabel;
    std::unique_ptr<juce::TextButton> clearQueueButton;
    std::unique_ptr<juce::TextButton> addSingerButton;
    std::unique_ptr<juce::ToggleButton> queueToggle;
    std::unique_ptr<juce::ToggleButton> autoPlayToggle;
    std::unique_ptr<juce::Slider>     delaySlider;
    std::unique_ptr<juce::Label>      delayLabel;
    std::unique_ptr<juce::Label>      countdownLabel;  // shows "Next in 5…" between songs

    //==============================================================================
    // State
    std::vector<Singers> singers;
    Singers              currentSinger;
    bool                 hasCurrentSinger = false;
    bool                 isPlayingState = false;
    bool                 queueRunning = false;
    bool                 autoPlayEnabled = false;
    int                  delaySec = 0;
    bool                 queueClosed = false;
    int                  countdownSecondsLeft = 0;
    bool                 expandedMode = false;

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
    static constexpr int expandedCardWidth   = 320;
    static constexpr int expandedCardHeight  = 84;
    static constexpr int expandedCardGap     = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QueueBar)
};
