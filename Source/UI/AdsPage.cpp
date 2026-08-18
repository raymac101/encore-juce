/*
  ==============================================================================

    AdsPage.cpp

  ==============================================================================
*/

#include "AdsPage.h"
#include "AdEditDialog.h"
#include "AdPreviewDialog.h"
#include "MenuTheme.h"
#include "../Services/AdMediaCache.h"
#include "../Services/AdVideoThumbnail.h"
#include "../Services/AdsService.h"
#include "../Services/ImageCache.h"

#include <algorithm>

namespace
{
    const juce::Colour kPanel        { 0x99182a52 };
    const juce::Colour kBorder       { 0x664f78c4 };
    const juce::Colour kAccent       { 0xff5a8fd8 };
    const juce::Colour kText         { 0xffffffff };
    const juce::Colour kMuted        { 0xffc7d2e0 };
    const juce::Colour kStatusActive { 0xff2e7d32 };
    const juce::Colour kStatusFuture { 0xff5a8fd8 };
    const juce::Colour kStatusPast   { 0xff6b7280 };
    const juce::Colour kDanger       { 0xffb3261e };

    constexpr int kTileWidth  = 210;
    constexpr int kTileHeight = 210;
    constexpr int kTileGap    = 16;

    bool isVideoExtension (const juce::String& ext)
    {
        static const juce::StringArray videoExts { ".mp4", ".m4v", ".mov", ".webm" };
        return videoExts.contains (ext.toLowerCase());
    }
}

//==============================================================================
// AdTile
//==============================================================================
AdsPage::AdTile::AdTile()
{
    nameLabel_.setColour (juce::Label::textColourId, kText);
    nameLabel_.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)).boldened());
    nameLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (nameLabel_);

    statusBadge_.setJustificationType (juce::Justification::centred);
    statusBadge_.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)).boldened());
    statusBadge_.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (statusBadge_);

    editButton_.setColour (juce::TextButton::buttonColourId, kPanel);
    editButton_.setColour (juce::TextButton::textColourOnId, kText);
    editButton_.setColour (juce::TextButton::textColourOffId, kText);
    editButton_.onClick = [this]() { if (onEdit) onEdit (ad_); };
    addAndMakeVisible (editButton_);

    deleteButton_.setColour (juce::TextButton::buttonColourId, kDanger.withAlpha (0.85f));
    deleteButton_.setColour (juce::TextButton::textColourOnId, kText);
    deleteButton_.setColour (juce::TextButton::textColourOffId, kText);
    deleteButton_.onClick = [this]() { if (onDelete) onDelete (ad_); };
    addAndMakeVisible (deleteButton_);
}

void AdsPage::AdTile::setAd (const AdMetadata& ad)
{
    ad_ = ad;
    thumbnail_ = {};

    nameLabel_.setText (ad_.name, juce::dontSendNotification);

    auto& lm = LocalizationManager::getInstance();
    const auto now = juce::Time::currentTimeMillis();

    juce::String statusText;
    juce::Colour statusColour;
    if (ad_.isActiveAt (now))
    {
        statusText = lm.getText ("ads.status_active");
        statusColour = kStatusActive;
    }
    else if (ad_.startDateMs > 0 && now < ad_.startDateMs)
    {
        statusText = lm.getText ("ads.status_scheduled");
        statusColour = kStatusFuture;
    }
    else
    {
        statusText = lm.getText ("ads.status_expired");
        statusColour = kStatusPast;
    }
    statusBadge_.setText (statusText, juce::dontSendNotification);
    statusBadge_.setColour (juce::Label::backgroundColourId, statusColour);

    editButton_.setButtonText (lm.getText ("ads.tile.edit"));

    if (ad_.isVideo())
        refreshVideoThumbnail();
    else
        refreshThumbnail();

    repaint();
}

void AdsPage::AdTile::refreshThumbnail()
{
    juce::Component::SafePointer<AdTile> safe (this);
    const auto url = ad_.url;

    auto img = ArtworkCache::getInstance().getOrFetch (url, [safe, url]()
    {
        if (safe == nullptr || safe->ad_.url != url)
            return;

        auto loaded = ArtworkCache::getInstance().getOrFetch (url, nullptr);
        if (loaded.isValid())
            safe->thumbnail_ = loaded;
        safe->repaint();
    });

    if (img.isValid())
        thumbnail_ = img;
}

void AdsPage::AdTile::refreshVideoThumbnail()
{
    const auto url = ad_.url;
    juce::Component::SafePointer<AdTile> safe (this);
    AdMediaCache::getOrFetch (url, ad_.name,
        [safe, url] (bool ok, juce::File file, const juce::String&)
        {
            if (safe == nullptr || safe->ad_.url != url || ! ok)
                return;

            juce::Thread::launch ([safe, url, file]
            {
                auto frame = AdVideoThumbnail::create (file, 640);
                juce::MessageManager::callAsync ([safe, url, frame]() mutable
                {
                    if (safe == nullptr || safe->ad_.url != url)
                        return;

                    safe->thumbnail_ = frame;
                    safe->repaint();
                });
            });
        });
}

void AdsPage::AdTile::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    auto thumbArea = getLocalBounds().reduced (8);
    thumbArea = thumbArea.removeFromTop (thumbArea.getHeight() - 56);

    if (thumbnail_.isValid())
    {
        g.drawImageWithin (thumbnail_, thumbArea.getX(), thumbArea.getY(),
                           thumbArea.getWidth(), thumbArea.getHeight(),
                           juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
                           false);

        if (ad_.isVideo())
        {
            auto playArea = thumbArea.withSizeKeepingCentre (38, 38).toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.62f));
            g.fillEllipse (playArea);
            juce::Path play;
            play.addTriangle (playArea.getX() + 15.0f, playArea.getY() + 10.0f,
                              playArea.getX() + 15.0f, playArea.getBottom() - 10.0f,
                              playArea.getRight() - 10.0f, playArea.getCentreY());
            g.setColour (juce::Colours::white);
            g.fillPath (play);
        }
    }
}

void AdsPage::AdTile::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto footer = area.removeFromBottom (52);

    nameLabel_.setBounds (footer.removeFromTop (18));

    auto row = footer.removeFromTop (24);
    editButton_.setBounds (row.removeFromRight (46));
    row.removeFromRight (4);
    deleteButton_.setBounds (row.removeFromRight (28));
    row.removeFromRight (6);
    statusBadge_.setBounds (row);
}

void AdsPage::AdTile::mouseUp (const juce::MouseEvent&)
{
    if (onPreview)
        onPreview (ad_);
}

//==============================================================================
// UploadZone
//==============================================================================
AdsPage::UploadZone::UploadZone()
{
    promptLabel_.setJustificationType (juce::Justification::centredLeft);
    promptLabel_.setColour (juce::Label::textColourId, kMuted);
    promptLabel_.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    addAndMakeVisible (promptLabel_);

    browseButton_.setColour (juce::TextButton::buttonColourId, kPanel);
    browseButton_.setColour (juce::TextButton::textColourOnId, kText);
    browseButton_.setColour (juce::TextButton::textColourOffId, kText);
    browseButton_.onClick = [this]()
    {
        fileChooser_ = std::make_unique<juce::FileChooser> (
            LocalizationManager::getInstance().getText ("ads.upload.select_file"),
            juce::File::getSpecialLocation (juce::File::userHomeDirectory),
            "*.png;*.jpg;*.jpeg;*.gif;*.webp;*.mp4;*.m4v;*.mov;*.webm");

        juce::Component::SafePointer<UploadZone> safe (this);
        fileChooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe] (const juce::FileChooser& chooser)
            {
                if (safe == nullptr)
                    return;

                auto file = chooser.getResult();
                if (file.existsAsFile() && safe->onFileChosen)
                    safe->onFileChosen (file);
            });
    };
    addAndMakeVisible (browseButton_);

    updateAllText();
}

void AdsPage::UploadZone::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    promptLabel_.setText (lm.getText ("ads.upload.prompt"), juce::dontSendNotification);
    browseButton_.setButtonText (lm.getText ("ads.upload.browse"));
}

bool AdsPage::UploadZone::isSupportedFile (const juce::File& f)
{
    static const juce::StringArray exts { ".png", ".jpg", ".jpeg", ".gif", ".webp", ".mp4", ".m4v", ".mov", ".webm" };
    return exts.contains (f.getFileExtension().toLowerCase());
}

bool AdsPage::UploadZone::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (isSupportedFile (juce::File (f)))
            return true;
    return false;
}

void AdsPage::UploadZone::fileDragEnter (const juce::StringArray&, int, int)
{
    dragHover_ = true;
    repaint();
}

void AdsPage::UploadZone::fileDragExit (const juce::StringArray&)
{
    dragHover_ = false;
    repaint();
}

void AdsPage::UploadZone::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover_ = false;
    repaint();

    for (auto& f : files)
    {
        juce::File file (f);
        if (isSupportedFile (file) && onFileChosen)
        {
            onFileChosen (file);
            break; // The edit dialog gates one upload at a time.
        }
    }
}

void AdsPage::UploadZone::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (dragHover_ ? kAccent.withAlpha (0.20f) : kAccent.withAlpha (0.08f));
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (dragHover_ ? kAccent : kBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.5f);
}

void AdsPage::UploadZone::resized()
{
    auto area = getLocalBounds().reduced (14, 12);
    browseButton_.setBounds (area.removeFromRight (110).withSizeKeepingCentre (100, 32));
    area.removeFromRight (10);
    promptLabel_.setBounds (area);
}

//==============================================================================
// AdsPage
//==============================================================================
AdsPage::AdsPage()
    : contentHolder_ (std::make_unique<ContentHolder>())
{
    setOpaque (true);
    addAndMakeVisible (viewport_);
    viewport_.setViewedComponent (contentHolder_.get(), false);
    viewport_.setScrollBarsShown (true, false);

    title_.setFont (juce::Font (juce::FontOptions().withHeight (30.0f)).boldened());
    title_.setColour (juce::Label::textColourId, kText);
    contentHolder_->addAndMakeVisible (title_);

    subtitle_.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)));
    subtitle_.setColour (juce::Label::textColourId, kMuted);
    contentHolder_->addAndMakeVisible (subtitle_);

    status_.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    status_.setColour (juce::Label::textColourId, kMuted);
    contentHolder_->addAndMakeVisible (status_);

    emptyStateLabel_.setJustificationType (juce::Justification::centred);
    emptyStateLabel_.setColour (juce::Label::textColourId, kMuted);
    emptyStateLabel_.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)));
    contentHolder_->addAndMakeVisible (emptyStateLabel_);

    uploadZone_ = std::make_unique<UploadZone>();
    uploadZone_->onFileChosen = [this] (const juce::File& file) { handleFileChosen (file); };
    contentHolder_->addAndMakeVisible (uploadZone_.get());

    contentHolder_->onPaint = [this] (juce::Graphics& g)
    {
        auto bounds = contentHolder_->getLocalBounds().reduced (16);
        MenuTheme::drawHeaderPanel (g, bounds);
    };

    updateAllText();
}

AdsPage::~AdsPage() = default;

void AdsPage::paint (juce::Graphics& g)
{
    MenuTheme::drawPageBackground (g, getLocalBounds());
}

void AdsPage::resized()
{
    viewport_.setBounds (getLocalBounds());

    const int contentWidth = juce::jmax (420, getWidth());
    contentHolder_->setSize (contentWidth, juce::jmax (getHeight(), 1));

    constexpr int margin = 24;
    const int innerWidth = contentWidth - margin * 2;
    int y = margin;

    title_.setBounds (margin, y, innerWidth, 34); y += 38;
    subtitle_.setBounds (margin, y, innerWidth, 18); y += 22;
    status_.setBounds (margin, y, innerWidth, 16); y += 24;

    uploadZone_->setBounds (margin, y, innerWidth, 60); y += 60 + 20;

    if (tiles_.isEmpty())
    {
        emptyStateLabel_.setVisible (true);
        emptyStateLabel_.setBounds (margin, y, innerWidth, 60);
        y += 60;
    }
    else
    {
        emptyStateLabel_.setVisible (false);

        const int cols = juce::jmax (1, (innerWidth + kTileGap) / (kTileWidth + kTileGap));
        int col = 0;
        int rowY = y;

        for (auto* tile : tiles_)
        {
            const int x = margin + col * (kTileWidth + kTileGap);
            tile->setBounds (x, rowY, kTileWidth, kTileHeight);
            ++col;
            if (col >= cols)
            {
                col = 0;
                rowY += kTileHeight + kTileGap;
            }
        }

        y = rowY + (col > 0 ? kTileHeight : 0);
    }

    y += margin;
    contentHolder_->setSize (contentWidth, juce::jmax (getHeight(), y));
}

void AdsPage::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();

    title_.setText (lm.getText ("page.ads"), juce::dontSendNotification);
    subtitle_.setText (venueName_.isNotEmpty() ? venueName_ : lm.getText ("ads.no_venue"), juce::dontSendNotification);
    emptyStateLabel_.setText (lm.getText ("ads.empty_state"), juce::dontSendNotification);

    if (uploadZone_ != nullptr)
        uploadZone_->updateAllText();

    resized();
}

void AdsPage::setVenueContext (const juce::String& venueId, const juce::String& venueName)
{
    venueId_ = venueId;
    venueName_ = venueName;
    ++loadToken_;

    updateAllText();
    refreshAds();
}

void AdsPage::refreshAds()
{
    auto& lm = LocalizationManager::getInstance();

    if (venueId_.isEmpty())
    {
        ads_.clear();
        rebuildTiles();
        setStatusMessage ({});
        return;
    }

    setBusy (true);
    setStatusMessage (lm.getText ("ads.loading"));

    const int token = loadToken_;
    juce::Component::SafePointer<AdsPage> safe (this);

    AdsService::getInstance().listAllAds (venueId_, [safe, token] (bool ok, std::vector<AdMetadata> ads)
    {
        if (safe == nullptr || token != safe->loadToken_)
            return;

        safe->setBusy (false);

        if (! ok)
        {
            safe->setStatusMessage (LocalizationManager::getInstance().getText ("ads.load_failed"));
            return;
        }

        safe->ads_ = std::move (ads);
        safe->rebuildTiles();

        safe->setStatusMessage (safe->ads_.empty() ? juce::String()
            : LocalizationManager::getInstance().getText ("ads.count_prefix") + juce::String ((int) safe->ads_.size()));
    });
}

void AdsPage::rebuildTiles()
{
    tiles_.clear();

    for (auto& ad : ads_)
    {
        auto* tile = tiles_.add (new AdTile());
        tile->setAd (ad);
        tile->onPreview = [this] (const AdMetadata& a) { AdPreviewDialog::launch (this, a); };
        tile->onEdit    = [this] (const AdMetadata& a) { openEditDialog (a, false, {}); };
        tile->onDelete  = [this] (const AdMetadata& a) { doDelete (a); };
        contentHolder_->addAndMakeVisible (tile);
    }

    resized();
}

void AdsPage::handleFileChosen (const juce::File& file)
{
    auto& lm = LocalizationManager::getInstance();

    if (venueId_.isEmpty())
    {
        setStatusMessage (lm.getText ("ads.no_venue"));
        return;
    }

    const auto docId = AdsService::sanitizeAdDocId (file.getFileName());
    const bool collides = std::any_of (ads_.begin(), ads_.end(),
                                       [&] (const AdMetadata& a) { return a.docId == docId; });

    AdMetadata defaults;
    defaults.docId = docId;
    defaults.name = file.getFileName();
    defaults.mediaType = isVideoExtension (file.getFileExtension()) ? "video" : "image";
    defaults.durationSec = defaults.mediaType == "video" ? 8 : 10;
    defaults.frequency = 1;

    if (! collides)
    {
        openEditDialog (defaults, true, file);
        return;
    }

    juce::AlertWindow::showOkCancelBox (
        juce::MessageBoxIconType::WarningIcon,
        lm.getText ("ads.upload.replace_title"),
        lm.getText ("ads.upload.replace_body").replace ("{name}", file.getFileName()),
        lm.getText ("ads.upload.replace_confirm"),
        lm.getText ("button.cancel"),
        nullptr,
        juce::ModalCallbackFunction::create (
            [safe = juce::Component::SafePointer<AdsPage> (this), defaults, file] (int result)
            {
                if (safe != nullptr && result == 1)
                    safe->openEditDialog (defaults, true, file);
            }));
}

void AdsPage::openEditDialog (AdMetadata defaults, bool isNewUpload, const juce::File& uploadFile)
{
    AdEditDialog::launch (this, defaults, [this, isNewUpload, uploadFile] (const AdEditResult& r)
    {
        if (r.action != AdEditResult::Action::Save)
            return;

        auto& lm = LocalizationManager::getInstance();
        setBusy (true);
        setStatusMessage (lm.getText ("ads.saving"));

        const int token = loadToken_;
        const auto venueId = venueId_;
        juce::Component::SafePointer<AdsPage> safe (this);

        auto onDone = [safe, token] (bool ok, juce::String error)
        {
            if (safe == nullptr || token != safe->loadToken_)
                return;

            safe->setBusy (false);
            if (! ok)
            {
                safe->setStatusMessage (error.isNotEmpty() ? error : LocalizationManager::getInstance().getText ("ads.save_failed"));
                return;
            }

            safe->refreshAds();
        };

        if (isNewUpload)
            AdsService::getInstance().uploadAd (venueId, uploadFile, r.meta, onDone);
        else
            AdsService::getInstance().updateAdMetadata (venueId, r.meta.docId, r.meta, onDone);
    });
}

void AdsPage::doDelete (const AdMetadata& ad)
{
    auto& lm = LocalizationManager::getInstance();

    juce::AlertWindow::showOkCancelBox (
        juce::MessageBoxIconType::WarningIcon,
        lm.getText ("ads.delete_confirm_title"),
        lm.getText ("ads.delete_confirm_body").replace ("{name}", ad.name),
        lm.getText ("ads.delete_confirm_delete"),
        lm.getText ("button.cancel"),
        nullptr,
        juce::ModalCallbackFunction::create (
            [safe = juce::Component::SafePointer<AdsPage> (this), ad] (int result)
            {
                if (safe == nullptr || result != 1)
                    return;

                safe->setBusy (true);
                safe->setStatusMessage (LocalizationManager::getInstance().getText ("ads.deleting"));

                const int token = safe->loadToken_;
                const auto venueId = safe->venueId_;

                AdsService::getInstance().deleteAd (venueId, ad.docId,
                    [safe, token] (bool ok, juce::String error)
                    {
                        if (safe == nullptr || token != safe->loadToken_)
                            return;

                        safe->setBusy (false);
                        if (! ok)
                        {
                            safe->setStatusMessage (error.isNotEmpty() ? error : LocalizationManager::getInstance().getText ("ads.delete_failed"));
                            return;
                        }

                        safe->refreshAds();
                    });
            }));
}

void AdsPage::setStatusMessage (const juce::String& message)
{
    status_.setText (message, juce::dontSendNotification);
}

void AdsPage::setBusy (bool busy)
{
    busy_ = busy;
    uploadZone_->setEnabled (! busy);
    for (auto* tile : tiles_)
        tile->setEnabled (! busy);
}
