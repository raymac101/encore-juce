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

class EditSingerModal : public juce::Component,
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
    void resized() override;

private:
    void buttonClicked(juce::Button* b) override;
    void rebuildRows();
    void closeWindow();

    struct Row : public juce::Component
    {
        Row(EditSingerModal& owner, int idx);
        void resized() override;
        void paint(juce::Graphics& g) override;

        EditSingerModal& owner;
        int index;

        juce::TextButton upBtn      { juce::String::fromUTF8("\xE2\x86\x91") };
        juce::TextButton downBtn    { juce::String::fromUTF8("\xE2\x86\x93") };
        juce::TextButton minusBtn   { "-" };
        juce::TextButton plusBtn    { "+" };
        juce::TextButton trashBtn   { juce::String::fromUTF8("\xF0\x9F\x97\x91") };
        juce::Label      pitchLabel;
    };

    juce::String      singerName;
    std::vector<QueueItem> songs;
    juce::OwnedArray<Row>  rows;

    juce::Label       title;
    juce::TextButton  closeBtn  { "X" };
    juce::TextButton  doneBtn   { "Done" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditSingerModal)
};
