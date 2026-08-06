/*
  ==============================================================================
    EditSingerModal.h

    Modal dialog used by the queue bar when the user clicks a song chip on a
    singer row. Mirrors the behaviour of the Angular `edit-singer-modal`
    component: the user can reorder the singer's songs (up/down arrows),
    change pitch (± buttons), or remove a song. Pressing "Done" emits the
    final song list via `onApply`.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include "../Models/QueueItem.h"
#include "../Models/CdgSong.h"

class EditSingerModal : public juce::Component,
                        public juce::DragAndDropContainer,
                        public juce::DragAndDropTarget,
                        private juce::Button::Listener
{
public:
    EditSingerModal(const juce::String& singerName,
                    const std::vector<QueueItem>& songs);
    ~EditSingerModal() override = default;

    /** Fired when the user presses Done. The supplied vector is the final,
        ordered list of songs (with any pitch / removal edits applied). */
    std::function<void(const std::vector<QueueItem>& songs)> onApply;

    /** Show the modal centred over `parent`. The component is owned by the
        DialogWindow that JUCE creates internally. */
    static void show(juce::Component* parent,
                     const juce::String& singerName,
                     const std::vector<QueueItem>& songs,
                     std::function<void(const std::vector<QueueItem>&)> onApply);

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    // juce::DragAndDropTarget -- drag-reorder the song rows (Row::mouseDrag
    // below starts the drag; this class is both the container and the only
    // drop target since all rows are its direct children, laid out in a
    // simple vertical stack in resized()).
    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDragMove (const SourceDetails& details) override;
    void itemDragExit (const SourceDetails& details) override;
    void itemDropped (const SourceDetails& details) override;

private:
    void buttonClicked(juce::Button* b) override;
    void rebuildRows();
    void closeWindow();
    void moveSong (int fromIndex, int toIndex);
    void showVersionPickerFor (int songIndex, juce::Component* anchor);

    /** Looks up the full library record (with every version[]/code[]/
        rating[] entry) for a queued song, by id first and falling back to
        a name+artist search. Opens a throwaway SongDatabase instance --
        same idiom MainComponent::loadSingerIntoNowPlaying uses, since this
        modal (a separate DialogWindow) has no access to MainArea's
        in-memory library list. Returns an invalid CdgSong if not found. */
    CdgSong findFullSongRecord (const QueueItem& item) const;

    struct Row : public juce::Component
    {
        Row(EditSingerModal& owner, int idx);
        void resized() override;
        void paint(juce::Graphics& g) override;
        void mouseDrag(const juce::MouseEvent& e) override;

        EditSingerModal& owner;
        int index;

        juce::TextButton upBtn      { juce::String::fromUTF8("\xE2\x86\x91") };
        juce::TextButton downBtn    { juce::String::fromUTF8("\xE2\x86\x93") };
        juce::TextButton minusBtn   { "-" };
        juce::TextButton plusBtn    { "+" };
        juce::DrawableButton trashBtn { "trash", juce::DrawableButton::ImageFitted };
        juce::TextButton versionBtn;
        juce::Label      pitchLabel;
    };

    juce::String      singerName;
    std::vector<QueueItem> songs;
    juce::OwnedArray<Row>  rows;

    juce::Label       title;
    juce::TextButton  closeBtn  { "X" };
    juce::TextButton  doneBtn   { "Done" };

    // Drag-reorder insertion-line indicator, drawn in paintOverChildren().
    int dropIndicatorY = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditSingerModal)
};
