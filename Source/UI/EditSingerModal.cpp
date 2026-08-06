/*
  ==============================================================================
    EditSingerModal.cpp
  ==============================================================================
*/

#include "EditSingerModal.h"
#include "SpriteIcon.h"
#include "../Services/SongDatabase.h"

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

    // Crisp vector icon (assets/images/sprite.svg's icon-trash) rather than
    // an emoji glyph -- the wastebasket emoji doesn't render reliably with
    // this app's fonts (was showing as mangled/blank glyphs).
    if (auto icon = SpriteIcon::create ("icon-trash", juce::Colour (0xffd0d0d0)))
        trashBtn.setImages (icon.get());
    trashBtn.addListener(&owner);
    addAndMakeVisible(trashBtn);

    versionBtn.addListener(&owner);
    addAndMakeVisible(versionBtn);

    pitchLabel.setJustificationType(juce::Justification::centred);
    pitchLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(pitchLabel);

    setComponentID("row" + juce::String(index));
}

void EditSingerModal::Row::mouseDrag (const juce::MouseEvent& e)
{
    // Don't start a drag from any of the buttons -- only the row's plain
    // "body" (the artist/song text area with no button over it).
    const auto start = e.getMouseDownPosition();
    if (upBtn.getBounds().contains (start) || downBtn.getBounds().contains (start)
        || minusBtn.getBounds().contains (start) || plusBtn.getBounds().contains (start)
        || trashBtn.getBounds().contains (start) || pitchLabel.getBounds().contains (start)
        || versionBtn.getBounds().contains (start))
        return;

    if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        if (! dnd->isDragAndDropActive())
        {
            auto img = createComponentSnapshot (getLocalBounds(), true);
            dnd->startDragging (juce::var (index), this, juce::ScaledImage (img),
                                /*allowDraggingToOtherWindows*/ false);
        }
    }
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
    r.removeFromRight(8);
    versionBtn.setBounds(r.removeFromRight(110));
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
    text.removeFromRight(36 + 8 + 28 + 48 + 28 + 8 + 110); // trash + pitch group + version

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
    // Plain ASCII hyphen rather than an em-dash: a literal multi-byte UTF-8
    // character embedded in a source string literal renders as mangled
    // bytes unless the compiler is explicitly told the source is UTF-8 --
    // safer to just not depend on that (see the row buttons below, which
    // sidestep the same issue via juce::String::fromUTF8 for real cases
    // where a non-ASCII glyph is unavoidable).
    title.setText("Edit Songs in the Queue - " + singerName,
                  juce::dontSendNotification);
    title.setFont(juce::Font(15.f).boldened());
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    addAndMakeVisible(closeBtn); closeBtn.addListener(this);
    addAndMakeVisible(doneBtn);  doneBtn.addListener(this);

    rebuildRows();
    setSize(670, kHeaderH + kFooterH + (int) songs.size() * kRowHeight + kPadding * 2);
}

void EditSingerModal::rebuildRows()
{
    rows.clear();
    for (int i = 0; i < (int) songs.size(); ++i)
    {
        auto* row = new Row(*this, i);
        row->pitchLabel.setText(juce::String((int) songs[(size_t) i].pitch),
                                juce::dontSendNotification);
        const auto ver = juce::String(songs[(size_t) i].songVersion);
        row->versionBtn.setButtonText(ver.isNotEmpty() ? ver : "Change Version");
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

void EditSingerModal::paintOverChildren (juce::Graphics& g)
{
    if (dropIndicatorY < 0)
        return;

    g.setColour (juce::Colour (0xff30daff));
    g.fillRect (juce::Rectangle<int> (kPadding, dropIndicatorY - 1, getWidth() - kPadding * 2, 3));
}

bool EditSingerModal::isInterestedInDragSource (const SourceDetails& details)
{
    return dynamic_cast<Row*> (details.sourceComponent.get()) != nullptr;
}

void EditSingerModal::itemDragMove (const SourceDetails& details)
{
    const int fromIndex = (int) details.description;
    const int targetY   = details.localPosition.y;

    int toIndex = 0;
    for (int i = 0; i < rows.size(); ++i)
    {
        if (i == fromIndex) continue;
        auto* r = rows[i];
        if (r->getY() + r->getHeight() / 2 < targetY)
            ++toIndex;
    }
    toIndex = juce::jlimit (0, (int) songs.size() - 1, toIndex);

    const int newY = (toIndex < rows.size()) ? rows[toIndex]->getY()
                                              : (rows.getLast() != nullptr ? rows.getLast()->getBottom() : -1);
    if (newY != dropIndicatorY)
    {
        dropIndicatorY = newY;
        repaint();
    }
}

void EditSingerModal::itemDragExit (const SourceDetails&)
{
    if (dropIndicatorY != -1)
    {
        dropIndicatorY = -1;
        repaint();
    }
}

void EditSingerModal::itemDropped (const SourceDetails& details)
{
    const int fromIndex = (int) details.description;
    const int targetY   = details.localPosition.y;

    int toIndex = 0;
    for (int i = 0; i < rows.size(); ++i)
    {
        if (i == fromIndex) continue;
        auto* r = rows[i];
        if (r->getY() + r->getHeight() / 2 < targetY)
            ++toIndex;
    }
    toIndex = juce::jlimit (0, (int) songs.size() - 1, toIndex);

    dropIndicatorY = -1;

    if (fromIndex < 0 || fromIndex >= (int) songs.size() || toIndex == fromIndex)
    {
        repaint();
        return;
    }

    moveSong (fromIndex, toIndex);
}

void EditSingerModal::moveSong (int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= (int) songs.size()) return;
    if (toIndex < 0 || toIndex >= (int) songs.size()) return;
    if (fromIndex == toIndex) return;

    auto moved = songs[(size_t) fromIndex];
    songs.erase (songs.begin() + fromIndex);
    songs.insert (songs.begin() + toIndex, moved);

    rebuildRows();
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
            juce::Component::SafePointer<EditSingerModal> safe(this);
            juce::MessageManager::callAsync([safe]() { if (safe != nullptr) safe->rebuildRows(); });
            return;
        }
        if (b == &row->downBtn && i < (int) songs.size() - 1)
        {
            std::swap(songs[(size_t) i], songs[(size_t) i + 1]);
            juce::Component::SafePointer<EditSingerModal> safe(this);
            juce::MessageManager::callAsync([safe]() { if (safe != nullptr) safe->rebuildRows(); });
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
            juce::Component::SafePointer<EditSingerModal> safe(this);
            juce::MessageManager::callAsync([safe]() { if (safe != nullptr) safe->rebuildRows(); });
            return;
        }
        if (b == &row->versionBtn)
        {
            showVersionPickerFor (i, &row->versionBtn);
            return;
        }
    }
}

CdgSong EditSingerModal::findFullSongRecord (const QueueItem& item) const
{
    SongDatabase db;
    if (! db.open())
        return {};

    const juce::String wantName   = juce::String (item.songName).trim();
    const juce::String wantArtist = juce::String (item.songArtist).trim();

    CdgSong byNameArtist;
    if (wantName.isNotEmpty())
        byNameArtist = db.findByNameAndArtist (wantName, wantArtist);

    const juce::String wantId = juce::String (item.songId).trim();
    CdgSong byId;
    if (wantId.isNotEmpty())
        byId = db.getById (wantId);

    // The library can contain duplicate rows for the same song/artist when
    // a scan didn't merge every manufacturer version into one record (seen
    // with "House Of The Rising Sun" -- one row left with only ["Unknown"],
    // another with 12 real versions). Trusting the queue item's stored
    // songId can land on the thin row and hide every other version, so
    // prefer whichever record actually has more versions listed.
    return (byNameArtist.version.size() > byId.version.size()) ? byNameArtist : byId;
}

void EditSingerModal::showVersionPickerFor (int songIndex, juce::Component* anchor)
{
    if (songIndex < 0 || songIndex >= (int) songs.size() || anchor == nullptr)
        return;

    const auto fullSong = findFullSongRecord (songs[(size_t) songIndex]);
    if (fullSong.version.empty())
    {
        const auto& item = songs[(size_t) songIndex];
        SongDatabase diagDb;
        const bool diagOpened = diagDb.open();
        juce::String diag;
        diag << "No other manufacturer versions of this song were found in the library.\n\n"
             << "[diagnostic]\n"
             << "songId: '" << juce::String (item.songId) << "'\n"
             << "songName: '" << juce::String (item.songName) << "'\n"
             << "songArtist: '" << juce::String (item.songArtist) << "'\n"
             << "db opened: " << (diagOpened ? "yes" : "NO") << "\n"
             << "db file: " << diagDb.getFile().getFullPathName();
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
            "No Other Versions", diag);
        return;
    }

    const juce::String currentVersion = juce::String (songs[(size_t) songIndex].songVersion).trim();
    const auto orderedIndices = fullSong.getVersionIndicesByRating();

    juce::PopupMenu menu;
    for (int vi : orderedIndices)
    {
        const bool isCurrent = juce::String (fullSong.version[(size_t) vi]).trim()
                                   .equalsIgnoreCase (currentVersion);
        menu.addItem (vi + 1, fullSong.getVersionLabel (vi), true, isCurrent);
    }

    juce::Component::SafePointer<EditSingerModal> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (anchor),
        [safe, songIndex, fullSong] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;

            const int vi = result - 1;
            if (vi < 0 || (size_t) vi >= fullSong.version.size())
                return;

            if (songIndex < 0 || songIndex >= (int) safe->songs.size())
                return;

            safe->songs[(size_t) songIndex].songVersion = fullSong.version[(size_t) vi];
            safe->rebuildRows();
        });
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
