/*
  ==============================================================================
    EditSingerModal.cpp
  ==============================================================================
*/

#include "EditSingerModal.h"

namespace
{
    constexpr int kRowHeight = 44;
    constexpr int kHeaderH   = 40;
    constexpr int kFooterH   = 48;
    constexpr int kPadding   = 12;
}

//==============================================================================
EditSingerModal::Row::Row(EditSingerModal& o, int idx) : owner(o), index(idx)
{
    addAndMakeVisible(upBtn);     upBtn.addListener(&owner);
    addAndMakeVisible(downBtn);   downBtn.addListener(&owner);
    addAndMakeVisible(minusBtn);  minusBtn.addListener(&owner);
    addAndMakeVisible(plusBtn);   plusBtn.addListener(&owner);
    addAndMakeVisible(trashBtn);  trashBtn.addListener(&owner);

    pitchLabel.setJustificationType(juce::Justification::centred);
    pitchLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(pitchLabel);

    setComponentID("row" + juce::String(index));
}

void EditSingerModal::Row::resized()
{
    auto r = getLocalBounds().reduced(4, 4);
    upBtn  .setBounds(r.removeFromLeft(28));
    r.removeFromLeft(2);
    downBtn.setBounds(r.removeFromLeft(28));
    r.removeFromLeft(8);

    trashBtn.setBounds(r.removeFromRight(36));
    r.removeFromRight(8);
    plusBtn .setBounds(r.removeFromRight(28));
    pitchLabel.setBounds(r.removeFromRight(48));
    minusBtn.setBounds(r.removeFromRight(28));
}

void EditSingerModal::Row::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().reduced(4, 2);
    g.setColour(juce::Colour(0xff2b2b2b));
    g.fillRoundedRectangle(r.toFloat(), 4.f);

    if (index < 0 || index >= (int) owner.songs.size())
        return;

    const auto& s = owner.songs[(size_t) index];

    // Text area sits between the up/down buttons and the pitch group.
    auto text = r;
    text.removeFromLeft(28 + 2 + 28 + 8); // up/down
    text.removeFromRight(36 + 8 + 28 + 48 + 28 + 8); // trash + pitch group

    auto top = text.removeFromTop(text.getHeight() / 2);
    g.setColour(juce::Colour(0xffd0d0d0));
    g.setFont(juce::Font(13.f).boldened());
    g.drawText(juce::String(s.songArtist), top.reduced(4, 0),
               juce::Justification::centredLeft, true);

    g.setColour(juce::Colour(0xffe4e4e4));
    g.setFont(juce::Font(12.f));
    juce::String name = juce::String(s.songName);
    if (! juce::String(s.songVersion).isEmpty()
        && juce::String(s.songVersion).compareIgnoreCase("Unknown") != 0)
        name += "  (" + juce::String(s.songVersion) + ")";
    g.drawText(name, text.reduced(4, 0),
               juce::Justification::centredLeft, true);
}

//==============================================================================
EditSingerModal::EditSingerModal(const juce::String& name,
                                 const std::vector<QueueItem>& s)
    : singerName(name), songs(s)
{
    title.setText("Edit Songs in the Queue — " + singerName,
                  juce::dontSendNotification);
    title.setFont(juce::Font(15.f).boldened());
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    addAndMakeVisible(closeBtn); closeBtn.addListener(this);
    addAndMakeVisible(doneBtn);  doneBtn.addListener(this);

    rebuildRows();
    setSize(560, kHeaderH + kFooterH + (int) songs.size() * kRowHeight + kPadding * 2);
}

void EditSingerModal::rebuildRows()
{
    rows.clear();
    for (int i = 0; i < (int) songs.size(); ++i)
    {
        auto* row = new Row(*this, i);
        row->pitchLabel.setText(juce::String((int) songs[(size_t) i].pitch),
                                juce::dontSendNotification);
        addAndMakeVisible(row);
        rows.add(row);
    }
    resized();
    repaint();
}

void EditSingerModal::resized()
{
    auto r = getLocalBounds().reduced(kPadding);

    auto header = r.removeFromTop(kHeaderH);
    closeBtn.setBounds(header.removeFromRight(32).reduced(2));
    title.setBounds(header);

    auto footer = r.removeFromBottom(kFooterH);
    doneBtn.setBounds(footer.removeFromRight(100).reduced(4, 8));

    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds(r.removeFromTop(kRowHeight));
}

void EditSingerModal::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void EditSingerModal::buttonClicked(juce::Button* b)
{
    if (b == &closeBtn) { closeWindow(); return; }
    if (b == &doneBtn)
    {
        if (onApply) onApply(songs);
        closeWindow();
        return;
    }

    // Row buttons: locate which row owns the button.
    for (int i = 0; i < rows.size(); ++i)
    {
        auto* row = rows[i];
        if (b == &row->upBtn && i > 0)
        {
            std::swap(songs[(size_t) i], songs[(size_t) i - 1]);
            rebuildRows();
            return;
        }
        if (b == &row->downBtn && i < (int) songs.size() - 1)
        {
            std::swap(songs[(size_t) i], songs[(size_t) i + 1]);
            rebuildRows();
            return;
        }
        if (b == &row->minusBtn)
        {
            songs[(size_t) i].pitch -= 1.0f;
            row->pitchLabel.setText(juce::String((int) songs[(size_t) i].pitch),
                                    juce::dontSendNotification);
            return;
        }
        if (b == &row->plusBtn)
        {
            songs[(size_t) i].pitch += 1.0f;
            row->pitchLabel.setText(juce::String((int) songs[(size_t) i].pitch),
                                    juce::dontSendNotification);
            return;
        }
        if (b == &row->trashBtn)
        {
            songs.erase(songs.begin() + i);
            rebuildRows();
            return;
        }
    }
}

void EditSingerModal::closeWindow()
{
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(0);
}

//==============================================================================
void EditSingerModal::show(juce::Component* parent,
                           const juce::String& singerName,
                           const std::vector<QueueItem>& songs,
                           std::function<void(const std::vector<QueueItem>&)> onApply)
{
    auto* modal = new EditSingerModal(singerName, songs);
    modal->onApply = std::move(onApply);

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle             = "Edit Singer";
    opts.dialogBackgroundColour  = juce::Colour(0xff1a1a1a);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar       = true;
    opts.resizable               = false;
    opts.componentToCentreAround = parent;
    opts.content.setOwned(modal);
    opts.launchAsync();
}
