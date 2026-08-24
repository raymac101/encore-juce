/*
  ==============================================================================

    CompanyAdminPage.cpp
  ==============================================================================
*/

#include "CompanyAdminPage.h"
#include "MenuTheme.h"
#include "EditVenueDialog.h"
#include "../Services/FirestoreClient.h"
#include "../Services/VenueService.h"
#include "../Services/SongbookStorageService.h"
#include "../Services/SongDeliveryService.h"
#include "../Services/CompanyService.h"
#include "../Services/InvitationService.h"
#include "../Services/ImageCache.h"
#include "../Firebase/FirebaseConfig.h"

namespace
{
juce::String mimeTypeForFile (const juce::File& file)
{
        const auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".png")  return "image/png";
        if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
        if (ext == ".gif")  return "image/gif";
        if (ext == ".webp") return "image/webp";
        return "application/octet-stream";
}
}

namespace
{
const juce::Colour kBg     { 0xff16213e };
const juce::Colour kPanel  { 0x99182a52 };
const juce::Colour kBorder { 0x664f78c4 };
const juce::Colour kAccent { 0xff5a8fd8 };
const juce::Colour kText   { 0xffffffff };
const juce::Colour kMuted  { 0xffc7d2e0 };
const juce::Colour kWarn   { 0xffe0a030 };

constexpr int kCardWidth  = 230;
constexpr int kCardHeight = 220;
constexpr int kCardGap    = 12;
}

//==============================================================================
// VenueCard
//==============================================================================
CompanyAdminPage::VenueCard::VenueCard()
{
    nameLabel_.setColour (juce::Label::textColourId, kText);
    nameLabel_.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)).boldened());
    addAndMakeVisible (nameLabel_);

    addressLabel_.setColour (juce::Label::textColourId, kMuted);
    addressLabel_.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addressLabel_.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (addressLabel_);

    countsLabel_.setColour (juce::Label::textColourId, kMuted);
    countsLabel_.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (countsLabel_);

    syncStatusLabel_.setColour (juce::Label::textColourId, kMuted);
    syncStatusLabel_.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (syncStatusLabel_);

    editButton_.setColour (juce::TextButton::buttonColourId, kPanel);
    editButton_.setColour (juce::TextButton::textColourOnId, kText);
    editButton_.setColour (juce::TextButton::textColourOffId, kText);
    editButton_.onClick = [this]() { if (onEdit) onEdit (venueId_); };
    addAndMakeVisible (editButton_);

    enableToggle_.setColour (juce::TextButton::buttonColourId, kPanel);
    enableToggle_.setColour (juce::TextButton::textColourOnId, kText);
    enableToggle_.setColour (juce::TextButton::textColourOffId, kText);
    enableToggle_.onClick = [this]() { if (onToggleEnabled) onToggleEnabled (venueId_); };
    addAndMakeVisible (enableToggle_);
}

void CompanyAdminPage::VenueCard::setVenue (const VenueItem& venue)
{
    venueId_ = juce::String (venue.id);
    logoUrl_ = juce::String (venue.logoUrl);
    enabled_ = venue.enabled;

    nameLabel_.setText (juce::String (venue.name), juce::dontSendNotification);

    juce::String address = juce::String (venue.address);
    juce::String city = juce::String (venue.city);
    if (city.isNotEmpty())
        address = address.isNotEmpty() ? address + "\n" + city : city;
    addressLabel_.setText (address, juce::dontSendNotification);

    setEnabledState (enabled_);
    refreshLogo();
    repaint();
}

void CompanyAdminPage::VenueCard::setEnabledState (bool enabled)
{
    enabled_ = enabled;
    auto& lm = LocalizationManager::getInstance();
    enableToggle_.setButtonText (enabled_ ? lm.getText ("company_admin.venue_disable")
                                           : lm.getText ("company_admin.venue_enable"));
}

void CompanyAdminPage::VenueCard::setCounts (const juce::String& text)
{
    countsLabel_.setText (text, juce::dontSendNotification);
}

void CompanyAdminPage::VenueCard::setSyncStatus (const juce::String& text, bool stale)
{
    syncStatusLabel_.setText (text, juce::dontSendNotification);
    syncStatusLabel_.setColour (juce::Label::textColourId, stale ? kWarn : kMuted);
}

void CompanyAdminPage::VenueCard::setSelected (bool selected)
{
    if (selected_ == selected)
        return;
    selected_ = selected;
    repaint();
}

void CompanyAdminPage::VenueCard::refreshLogo()
{
    if (logoUrl_.isEmpty())
    {
        logo_ = {};
        return;
    }

    juce::Component::SafePointer<VenueCard> safe (this);
    const auto url = logoUrl_;
    auto img = ArtworkCache::getInstance().getOrFetch (url, [safe, url]()
    {
        if (safe == nullptr || safe->logoUrl_ != url)
            return;
        auto loaded = ArtworkCache::getInstance().getOrFetch (url, nullptr);
        if (loaded.isValid())
            safe->logo_ = loaded;
        safe->repaint();
    });

    if (img.isValid())
        logo_ = img;
}

void CompanyAdminPage::VenueCard::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (selected_ ? kAccent : kBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, selected_ ? 2.0f : 1.0f);

    auto logoArea = getLocalBounds().reduced (10);
    logoArea = logoArea.removeFromTop (90);

    if (logo_.isValid())
    {
        g.drawImageWithin (logo_, logoArea.getX(), logoArea.getY(),
                           logoArea.getWidth(), logoArea.getHeight(),
                           juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
                           false);
    }
    else
    {
        g.setColour (kBorder);
        g.drawRoundedRectangle (logoArea.toFloat(), 6.0f, 1.0f);
        g.setColour (kMuted.withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
        g.drawFittedText (LocalizationManager::getInstance().getText ("company_admin.no_logo"),
                          logoArea, juce::Justification::centred, 1);
    }

    if (! enabled_)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (bounds, 10.0f);
    }
}

void CompanyAdminPage::VenueCard::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (90); // logo area, drawn in paint()
    area.removeFromTop (4);

    nameLabel_.setBounds (area.removeFromTop (18));
    addressLabel_.setBounds (area.removeFromTop (32));
    countsLabel_.setBounds (area.removeFromTop (16));
    syncStatusLabel_.setBounds (area.removeFromTop (16));
    area.removeFromTop (4);

    auto buttons = area.removeFromTop (26);
    editButton_.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
    buttons.removeFromLeft (8);
    enableToggle_.setBounds (buttons);
}

void CompanyAdminPage::VenueCard::mouseUp (const juce::MouseEvent&)
{
    if (onSelected)
        onSelected (venueId_);
}

//==============================================================================
CompanyAdminPage::CompanyAdminPage()
    : contentHolder_ (std::make_unique<ContentHolder>())
{
    setOpaque(true);
    addAndMakeVisible (viewport_);
    viewport_.setViewedComponent (contentHolder_.get(), false);
    // Horizontal too: contentHolder_ has a minimum width floor (see
    // resized()) that can exceed a narrow viewport, clipping content with
    // no way to reach it otherwise.
    viewport_.setScrollBarsShown (true, true);

    auto& lm = LocalizationManager::getInstance();

    title_.setFont (juce::Font (juce::FontOptions().withHeight (30.0f)).boldened());
    title_.setColour (juce::Label::textColourId, kText);
    contentHolder_->addAndMakeVisible (title_);

    subtitle_.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)));
    subtitle_.setColour (juce::Label::textColourId, kMuted);
    subtitle_.setText (lm.getText ("page.company_admin.subtitle"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (subtitle_);

    status_.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    status_.setColour (juce::Label::textColourId, kMuted);
    contentHolder_->addAndMakeVisible (status_);

    editCompanyToggle_.onClick = [this]()
    {
        companyEditFormVisible_ = ! companyEditFormVisible_;
        resized();
    };
    contentHolder_->addAndMakeVisible (editCompanyToggle_);

    companyInfoTitle_.setFont (juce::Font (juce::FontOptions().withHeight (16.0f)).boldened());
    companyInfoTitle_.setColour (juce::Label::textColourId, kText);
    contentHolder_->addAndMakeVisible (companyInfoTitle_);

    companyIdEditor_.setTextToShowWhenEmpty (lm.getText ("company_admin.company_id_hint"), kMuted);
    companyIdEditor_.setColour (juce::TextEditor::backgroundColourId, kPanel);
    companyIdEditor_.setColour (juce::TextEditor::textColourId, kText);
    companyIdEditor_.setColour (juce::TextEditor::outlineColourId, kBorder);
    companyIdEditor_.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
    contentHolder_->addAndMakeVisible (companyIdEditor_);

    companyIdLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    companyIdLabel_.setColour (juce::Label::textColourId, kMuted);
    contentHolder_->addAndMakeVisible (companyIdLabel_);

    companyNameLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)).boldened());
    companyNameLabel_.setColour (juce::Label::textColourId, kMuted);
    contentHolder_->addAndMakeVisible (companyNameLabel_);

    companyStatusLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)).boldened());
    companyStatusLabel_.setColour (juce::Label::textColourId, kMuted);
    contentHolder_->addAndMakeVisible (companyStatusLabel_);

    logoLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)).boldened());
    logoLabel_.setColour (juce::Label::textColourId, kMuted);
    contentHolder_->addAndMakeVisible (logoLabel_);

    logoPathLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    logoPathLabel_.setColour (juce::Label::textColourId, kMuted);
    logoPathLabel_.setJustificationType (juce::Justification::centredLeft);
    contentHolder_->addAndMakeVisible (logoPathLabel_);

    companyNameEditor_.setTextToShowWhenEmpty (lm.getText ("company_admin.company_name_hint"), kMuted);
    companyNameEditor_.setColour (juce::TextEditor::backgroundColourId, kPanel);
    companyNameEditor_.setColour (juce::TextEditor::textColourId, kText);
    companyNameEditor_.setColour (juce::TextEditor::outlineColourId, kBorder);
    companyNameEditor_.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
    contentHolder_->addAndMakeVisible (companyNameEditor_);

    companyStatusBox_.addItem (lm.getText ("company_admin.status_active"), 1);
    companyStatusBox_.addItem (lm.getText ("company_admin.status_suspended"), 2);
    companyStatusBox_.setColour (juce::ComboBox::backgroundColourId, kPanel);
    companyStatusBox_.setColour (juce::ComboBox::textColourId, kText);
    companyStatusBox_.setColour (juce::ComboBox::outlineColourId, kBorder);
    contentHolder_->addAndMakeVisible (companyStatusBox_);

    membersTitle_.setFont (juce::Font (juce::FontOptions().withHeight (16.0f)).boldened());
    membersTitle_.setColour (juce::Label::textColourId, kText);
    contentHolder_->addAndMakeVisible (membersTitle_);

    memberUserIdLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)).boldened());
    memberUserIdLabel_.setColour (juce::Label::textColourId, kMuted);
    contentHolder_->addAndMakeVisible (memberUserIdLabel_);

    memberRoleLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)).boldened());
    memberRoleLabel_.setColour (juce::Label::textColourId, kMuted);
    contentHolder_->addAndMakeVisible (memberRoleLabel_);

    memberUserIdEditor_.setTextToShowWhenEmpty (lm.getText ("company_admin.member_user_id_hint"), kMuted);
    memberUserIdEditor_.setColour (juce::TextEditor::backgroundColourId, kPanel);
    memberUserIdEditor_.setColour (juce::TextEditor::textColourId, kText);
    memberUserIdEditor_.setColour (juce::TextEditor::outlineColourId, kBorder);
    memberUserIdEditor_.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
    contentHolder_->addAndMakeVisible (memberUserIdEditor_);

    memberRoleBox_.addItem ("company_admin", 1);
    memberRoleBox_.addItem ("host", 2);
    memberRoleBox_.addItem ("viewer", 3);
    memberRoleBox_.setSelectedId (1, juce::dontSendNotification);
    memberRoleBox_.setColour (juce::ComboBox::backgroundColourId, kPanel);
    memberRoleBox_.setColour (juce::ComboBox::textColourId, kText);
    memberRoleBox_.setColour (juce::ComboBox::outlineColourId, kBorder);
    contentHolder_->addAndMakeVisible (memberRoleBox_);

    memberStatusLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)).boldened());
    memberStatusLabel_.setColour (juce::Label::textColourId, kMuted);
    memberStatusLabel_.setText (lm.getText ("company_admin.member_status"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (memberStatusLabel_);

    memberStatusBox_.addItem (lm.getText ("company_admin.status_active"), 1);
    memberStatusBox_.addItem (lm.getText ("company_admin.status_suspended"), 2);
    memberStatusBox_.setSelectedId (1, juce::dontSendNotification);
    memberStatusBox_.setColour (juce::ComboBox::backgroundColourId, kPanel);
    memberStatusBox_.setColour (juce::ComboBox::textColourId, kText);
    memberStatusBox_.setColour (juce::ComboBox::outlineColourId, kBorder);
    contentHolder_->addAndMakeVisible (memberStatusBox_);

    membersListLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    membersListLabel_.setColour (juce::Label::textColourId, kText);
    membersListLabel_.setJustificationType (juce::Justification::topLeft);
    contentHolder_->addAndMakeVisible (membersListLabel_);

    venuesTitle_.setFont (juce::Font (juce::FontOptions().withHeight (16.0f)).boldened());
    venuesTitle_.setColour (juce::Label::textColourId, kText);
    venuesTitle_.setText (lm.getText ("company_admin.venues_title"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (venuesTitle_);

    venuesEmptyLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    venuesEmptyLabel_.setColour (juce::Label::textColourId, kMuted);
    contentHolder_->addAndMakeVisible (venuesEmptyLabel_);

    // --- Venue staff section (populated when a card is selected) --------
    staffTitle_.setFont (juce::Font (juce::FontOptions().withHeight (16.0f)).boldened());
    staffTitle_.setColour (juce::Label::textColourId, kText);
    contentHolder_->addAndMakeVisible (staffTitle_);

    staffEmptyLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    staffEmptyLabel_.setColour (juce::Label::textColourId, kMuted);
    staffEmptyLabel_.setText (lm.getText ("company_admin.staff_select_venue"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (staffEmptyLabel_);

    staffInviteEmailEditor_.setTextToShowWhenEmpty (lm.getText ("company_admin.staff_email_hint"), kMuted);
    staffInviteEmailEditor_.setColour (juce::TextEditor::backgroundColourId, kPanel);
    staffInviteEmailEditor_.setColour (juce::TextEditor::textColourId, kText);
    staffInviteEmailEditor_.setColour (juce::TextEditor::outlineColourId, kBorder);
    staffInviteEmailEditor_.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
    contentHolder_->addAndMakeVisible (staffInviteEmailEditor_);

    staffInviteRoleBox_.addItem ("Host", 1);
    staffInviteRoleBox_.addItem ("Admin", 2);
    staffInviteRoleBox_.setSelectedId (1, juce::dontSendNotification);
    staffInviteRoleBox_.setColour (juce::ComboBox::backgroundColourId, kPanel);
    staffInviteRoleBox_.setColour (juce::ComboBox::textColourId, kText);
    staffInviteRoleBox_.setColour (juce::ComboBox::outlineColourId, kBorder);
    contentHolder_->addAndMakeVisible (staffInviteRoleBox_);

    staffInviteButton_.onClick = [this]() { inviteVenueStaff(); };
    contentHolder_->addAndMakeVisible (staffInviteButton_);

    // --- Song distribution section ---------------------------------------
    songSectionTitle_.setFont (juce::Font (juce::FontOptions().withHeight (16.0f)).boldened());
    songSectionTitle_.setColour (juce::Label::textColourId, kText);
    songSectionTitle_.setText (lm.getText ("company_admin.songs_title"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (songSectionTitle_);

    songFileLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    songFileLabel_.setColour (juce::Label::textColourId, kMuted);
    songFileLabel_.setText (lm.getText ("company_admin.songs_no_file"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (songFileLabel_);

    browseSongButton_.onClick = [this]() { chooseSongFile(); };
    contentHolder_->addAndMakeVisible (browseSongButton_);

    targetAllVenuesToggle_.setClickingTogglesState (true);
    targetAllVenuesToggle_.setToggleState (true, juce::dontSendNotification);
    targetAllVenuesToggle_.onClick = [this]()
    {
        targetAllVenues_ = targetAllVenuesToggle_.getToggleState();
        resized();
    };
    contentHolder_->addAndMakeVisible (targetAllVenuesToggle_);

    uploadSongButton_.onClick = [this]() { sendSongToTargetVenues(); };
    contentHolder_->addAndMakeVisible (uploadSongButton_);

    for (auto* button : { &applyCompanyIdButton_, &browseLogoButton_, &clearLogoButton_, &saveCompanyButton_,
                          &saveMemberButton_, &refreshMembersButton_,
                          &refreshButton_, &registerButton_,
                          &staffInviteButton_, &browseSongButton_, &uploadSongButton_,
                          &editCompanyToggle_, &targetAllVenuesToggle_ })
    {
        button->setColour (juce::TextButton::buttonColourId, kPanel);
        button->setColour (juce::TextButton::textColourOnId, kText);
        button->setColour (juce::TextButton::textColourOffId, kText);
    }

    applyCompanyIdButton_.onClick = [this]() { applyCompanyIdFromEditor(); };

    browseLogoButton_.onClick = [this]()
    {
        fileChooser_ = std::make_unique<juce::FileChooser> (
            LocalizationManager::getInstance().getText ("company_admin.select_logo"),
            juce::File::getSpecialLocation (juce::File::userHomeDirectory),
            "*.png;*.jpg;*.jpeg;*.gif;*.webp");

        juce::Component::SafePointer<CompanyAdminPage> safe (this);
        fileChooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe] (const juce::FileChooser& chooser)
            {
                if (safe == nullptr)
                    return;

                auto file = chooser.getResult();
                if (! file.existsAsFile())
                    return;

                safe->selectedLogoFile_ = file;
                safe->companyLogoFileName_ = file.getFileName();
                safe->logoPathLabel_.setText (safe->companyLogoFileName_, juce::dontSendNotification);
                safe->updateLogoPreviewFromFile (file);
            });
    };

    clearLogoButton_.onClick = [this]() { clearLogo(); };
    saveCompanyButton_.onClick = [this]() { saveCompanyInfo(); };
    refreshMembersButton_.onClick = [this]() { loadMembers(); };
    saveMemberButton_.onClick = [this]() { saveMemberMapping(); };
    refreshButton_.onClick = [this]() { loadCompanyVenues(); pushSongsToCompanyVenues(); };

    configureCard (venueCard_,    lm.getText ("company_admin.venues"),    "0");
    configureCard (hostCard_,     lm.getText ("company_admin.hosts"),     "0");
    configureCard (deviceCard_,   lm.getText ("company_admin.devices"),   "0");
    configureCard (packageCard_,  lm.getText ("company_admin.packages"),  "0");
    configureCard (campaignCard_, lm.getText ("company_admin.campaigns"), "0");

    // Decorative panels/cards, drawn against contentHolder_'s own bounds
    // (not CompanyAdminPage's) so they scroll along with the rest of the
    // content. Uses cached *_Bottom_ member rectangles set in layoutContent()
    // rather than re-deriving section heights independently here, since
    // several sections now have data-dependent heights.
    contentHolder_->onPaint = [this] (juce::Graphics& g)
    {
        auto bounds = contentHolder_->getLocalBounds().reduced (22);
        MenuTheme::drawHeaderPanel (g, bounds);

        MenuTheme::drawHeaderPanel (g, headerPanelBounds_);

        g.setColour (kBorder);
        g.drawRoundedRectangle (headerLogoBounds_.toFloat(), 10.0f, 1.0f);
        if (companyHeaderLogo_.isValid())
        {
            g.drawImageWithin (companyHeaderLogo_, headerLogoBounds_.getX() + 4, headerLogoBounds_.getY() + 4,
                               headerLogoBounds_.getWidth() - 8, headerLogoBounds_.getHeight() - 8,
                               juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
                               false);
        }

        if (companyEditFormVisible_)
            MenuTheme::drawHeaderPanel (g, editFormPanelBounds_);

        auto drawStatCard = [&g] (const StatCard& card)
        {
            auto rect = card.title.getBounds().getUnion (card.value.getBounds()).expanded (10, 8).toFloat();
            MenuTheme::drawGlassCard (g, rect, 12.0f);
        };

        drawStatCard (venueCard_);
        drawStatCard (hostCard_);
        drawStatCard (deviceCard_);
        drawStatCard (packageCard_);
        drawStatCard (campaignCard_);
    };

    setCompanyContext ({}, {});
}

CompanyAdminPage::~CompanyAdminPage() = default;

void CompanyAdminPage::updateLogoPreviewFromFile (const juce::File& file)
{
    logoPreview_ = {};
    if (file.existsAsFile())
        logoPreview_ = juce::ImageFileFormat::loadFrom (file);
    repaint();
}

void CompanyAdminPage::configureCard (StatCard& card, const juce::String& title, const juce::String& initialValue)
{
    card.title.setText (title, juce::dontSendNotification);
    card.title.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)).boldened());
    card.title.setColour (juce::Label::textColourId, kMuted);
    contentHolder_->addAndMakeVisible (card.title);

    card.value.setText (initialValue, juce::dontSendNotification);
    card.value.setFont (juce::Font (juce::FontOptions().withHeight (28.0f)).boldened());
    card.value.setColour (juce::Label::textColourId, kText);
    contentHolder_->addAndMakeVisible (card.value);
}

void CompanyAdminPage::setCompanyContext (const juce::String& companyId, const juce::String& companyRole)
{
    companyId_ = companyId;
    companyRole_ = companyRole;
    selectedVenueId_.clear();

    auto& lm = LocalizationManager::getInstance();
    title_.setText (companyId_.isNotEmpty() ? lm.getText ("page.company_admin") : lm.getText ("page.company_admin"), juce::dontSendNotification);

    juce::String context = companyId_.isNotEmpty() ? juce::String() : lm.getText ("company_admin.no_company");
    if (companyRole_.isNotEmpty())
        context << (context.isNotEmpty() ? "  |  " : "") << "Role: " << companyRole_;
    subtitle_.setText (context, juce::dontSendNotification);

    companyIdEditor_.setText (companyId_, juce::dontSendNotification);

    loadCompanyInfo();
    loadMembers();
    loadCompanyVenues();
    loadVenueStaff ({});
    repaint();
}

void CompanyAdminPage::applyCompanyIdFromEditor()
{
    const auto nextCompanyId = companyIdEditor_.getText().trim();
    if (nextCompanyId.isEmpty())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.company_id_required"));
        return;
    }

    companyId_ = nextCompanyId;
    companyDocExists_ = false;
    if (onCompanyIdChanged)
        onCompanyIdChanged (companyId_);

    loadCompanyInfo();
    loadMembers();
    loadCompanyVenues();
}

void CompanyAdminPage::loadCompanyInfo()
{
    companyDocExists_ = false;
    companyLogoUrl_.clear();
    companyLogoStoragePath_.clear();
    selectedLogoFile_ = juce::File();
    logoPreview_ = {};
    companyHeaderLogo_ = {};

    if (companyId_.isEmpty())
        return;

    companyInfoTitle_.setText (LocalizationManager::getInstance().getText ("company_admin.edit_info"), juce::dontSendNotification);
    companyIdLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.company_id") + ": " + companyId_, juce::dontSendNotification);
    status_.setText (LocalizationManager::getInstance().getText ("company_admin.loading_info"), juce::dontSendNotification);

    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    const auto companyId = companyId_;
    juce::Thread::launch ([safe, companyId]()
    {
        auto& fc = FirestoreClient::getInstance();
        int httpStatus = 0;
        auto doc = fc.getDocument ("companies/" + companyId, &httpStatus);

        juce::String name;
        juce::String status;
        juce::String logoUrl;
        juce::String logoStoragePath;
        juce::String logoFileName;
        const bool exists = (httpStatus == 200 && doc.isObject() && doc.hasProperty ("fields"));

        if (exists)
        {
            name = FirestoreClient::readString (doc, "name");
            status = FirestoreClient::readString (doc, "status");
            logoUrl = FirestoreClient::readString (doc, "logoUrl");
            logoStoragePath = FirestoreClient::readString (doc, "logoStoragePath");
            logoFileName = FirestoreClient::readString (doc, "logoFileName");
        }

        juce::MessageManager::callAsync ([safe, name, status, logoUrl, logoStoragePath, logoFileName, exists]()
        {
            if (safe == nullptr)
                return;

            safe->companyDocExists_ = exists;
            safe->companyNameEditor_.setText (name, juce::dontSendNotification);
            safe->companyStatusBox_.setSelectedId (status.equalsIgnoreCase ("suspended") ? 2 : 1, juce::dontSendNotification);
            safe->companyLogoUrl_ = logoUrl;
            safe->companyLogoStoragePath_ = logoStoragePath;
            safe->companyLogoFileName_ = logoFileName;
            const auto logoText = logoFileName.isNotEmpty() ? logoFileName
                : logoUrl.isNotEmpty() ? logoUrl
                : LocalizationManager::getInstance().getText ("company_admin.no_logo");
            safe->logoPathLabel_.setText (logoText, juce::dontSendNotification);
            safe->logoPreview_ = {};
            safe->title_.setText (name.isNotEmpty() ? name : LocalizationManager::getInstance().getText ("page.company_admin"),
                                  juce::dontSendNotification);

            if (logoUrl.isNotEmpty())
            {
                auto img = ArtworkCache::getInstance().getOrFetch (logoUrl, [safe, logoUrl]()
                {
                    if (safe == nullptr || safe->companyLogoUrl_ != logoUrl)
                        return;
                    auto loaded = ArtworkCache::getInstance().getOrFetch (logoUrl, nullptr);
                    if (loaded.isValid())
                        safe->companyHeaderLogo_ = loaded;
                    safe->repaint();
                });
                if (img.isValid())
                    safe->companyHeaderLogo_ = img;
            }

            safe->repaint();
            safe->status_.setText (exists ? LocalizationManager::getInstance().getText ("company_admin.info_loaded")
                                          : LocalizationManager::getInstance().getText ("company_admin.create_info"), juce::dontSendNotification);
        });
    });
}

void CompanyAdminPage::loadCompanyVenues()
{
    venueCards_.clear();

    if (companyId_.isEmpty())
    {
        venuesEmptyLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.venue_none"), juce::dontSendNotification);
        venuesEmptyLabel_.setVisible (true);
        venueCard_.value.setText ("0", juce::dontSendNotification);
        resized();
        return;
    }

    venuesEmptyLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.venues_loading"), juce::dontSendNotification);
    venuesEmptyLabel_.setVisible (true);
    resized();

    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    const auto companyId = companyId_;
    VenueService::getInstance().getVenuesForCompany (companyId,
        [safe, companyId] (bool ok, std::vector<VenueItem> venues, juce::String /*error*/)
        {
            if (safe == nullptr || companyId != safe->companyId_)
                return;

            if (! ok)
            {
                safe->venuesEmptyLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.venue_none"), juce::dontSendNotification);
                return;
            }

            safe->venueCards_.clear();
            safe->venueCard_.value.setText (juce::String ((int) venues.size()), juce::dontSendNotification);
            safe->venuesEmptyLabel_.setVisible (venues.empty());
            if (venues.empty())
                safe->venuesEmptyLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.venue_none"), juce::dontSendNotification);

            // Rebuild the song-target checkboxes to match the current venue
            // set (see layoutSongSection / sendSongToTargetVenues).
            safe->venueTargets_.clear();

            for (auto& v : venues)
            {
                auto card = std::make_unique<VenueCard>();
                card->setVenue (v);
                const auto venueId = juce::String (v.id);
                card->setSelected (venueId == safe->selectedVenueId_);
                card->onSelected = [safe] (const juce::String& id) { if (safe != nullptr) safe->selectVenue (id); };
                card->onEdit = [safe] (const juce::String& id) { if (safe != nullptr) safe->openEditVenueDialog (id); };
                card->onToggleEnabled = [safe] (const juce::String& id) { if (safe != nullptr) safe->toggleVenueEnabled (id); };
                card->setCounts (LocalizationManager::getInstance().getText ("company_admin.venue_loading_counts"));
                card->setSyncStatus (LocalizationManager::getInstance().getText ("company_admin.venue_sync_checking"), false);
                safe->contentHolder_->addAndMakeVisible (*card);
                safe->venueCards_.push_back (std::move (card));

                auto target = std::make_unique<VenueTargetToggle>();
                target->venueId = venueId;
                target->toggle.setButtonText (juce::String (v.name));
                target->toggle.setColour (juce::ToggleButton::textColourId, kText);
                safe->contentHolder_->addAndMakeVisible (target->toggle);
                safe->venueTargets_.push_back (std::move (target));

                VenueService::getInstance().checkExistingSessionData (venueId,
                    [safe, venueId] (bool countsOk, VenueService::SessionCounts counts, juce::String /*err*/)
                    {
                        if (safe == nullptr || ! countsOk)
                            return;

                        for (auto& c : safe->venueCards_)
                        {
                            if (c->getVenueId() != venueId)
                                continue;
                            auto& lm = LocalizationManager::getInstance();
                            c->setCounts (lm.getText ("company_admin.venue_queue") + ": " + juce::String (counts.queueCount)
                                + "  " + lm.getText ("company_admin.venue_requested") + ": " + juce::String (counts.requestedCount));
                            break;
                        }
                    });

                safe->refreshVenueSyncStatus (venueId);
            }

            safe->resized();
        });
}

void CompanyAdminPage::refreshVenueSyncStatus (const juce::String& venueId)
{
    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    SongbookStorageService::getInstance().checkSongbookInSync (venueId,
        [safe, venueId] (bool inSync, juce::String error)
        {
            if (safe == nullptr)
                return;

            for (auto& c : safe->venueCards_)
            {
                if (c->getVenueId() != venueId)
                    continue;

                auto& lm = LocalizationManager::getInstance();
                const bool stale = error.isEmpty() && ! inSync;
                c->setSyncStatus (
                    error.isNotEmpty() ? lm.getText ("company_admin.venue_sync_unknown")
                    : inSync           ? lm.getText ("company_admin.venue_sync_ok")
                                        : lm.getText ("company_admin.venue_sync_stale"),
                    stale);
                break;
            }
        });
}

void CompanyAdminPage::pushSongsToCompanyVenues()
{
    if (venueCards_.empty())
        return;

    std::vector<juce::String> venueIds;
    venueIds.reserve (venueCards_.size());
    for (auto& c : venueCards_)
        venueIds.push_back (c->getVenueId());

    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    for (auto& venueId : venueIds)
        refreshVenueSyncStatus (venueId);
}

void CompanyAdminPage::toggleVenueEnabled (const juce::String& venueId)
{
    VenueCard* target = nullptr;
    for (auto& c : venueCards_)
        if (c->getVenueId() == venueId) { target = c.get(); break; }
    if (target == nullptr)
        return;

    const bool nextEnabled = ! target->isVenueEnabled();
    target->setInteractionsEnabled (false);

    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    VenueService::getInstance().setVenueEnabled (venueId, nextEnabled,
        [safe, venueId, nextEnabled] (bool ok, juce::String error)
        {
            if (safe == nullptr)
                return;

            for (auto& c : safe->venueCards_)
            {
                if (c->getVenueId() != venueId)
                    continue;

                c->setInteractionsEnabled (true);
                if (ok)
                    c->setEnabledState (nextEnabled);
                else
                    safe->setStatusMessage (error.isNotEmpty() ? error
                        : LocalizationManager::getInstance().getText ("company_admin.venue_toggle_failed"));
                c->repaint();
                break;
            }
        });
}

void CompanyAdminPage::selectVenue (const juce::String& venueId)
{
    if (selectedVenueId_ == venueId)
        return;

    selectedVenueId_ = venueId;
    for (auto& c : venueCards_)
        c->setSelected (c->getVenueId() == venueId);

    loadVenueStaff (venueId);
    resized();
}

void CompanyAdminPage::openEditVenueDialog (const juce::String& venueId)
{
    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    EditVenueDialog::launch (this, venueId, [safe] (bool changed)
    {
        if (safe != nullptr && changed)
            safe->loadCompanyVenues();
    });
}

void CompanyAdminPage::clearLogo()
{
    companyLogoUrl_.clear();
    companyLogoStoragePath_.clear();
    companyLogoFileName_.clear();
    selectedLogoFile_ = juce::File();
    logoPreview_ = {};
    logoPathLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.no_logo"), juce::dontSendNotification);
    repaint();
}

bool CompanyAdminPage::uploadLogoToStorage (const juce::String& companyId,
                                            const juce::File& logoFile,
                                            juce::String& outLogoUrl,
                                            juce::String& outStoragePath,
                                            juce::String& outError) const
{
    juce::MemoryBlock data;
    if (! logoFile.existsAsFile() || ! logoFile.loadFileAsData (data))
    {
        outError = "Could not read logo file.";
        return false;
    }

    const auto ext = logoFile.getFileExtension().toLowerCase();
    const auto objectName = "companies/" + companyId + "/artwork/logo-"
                            + juce::String (juce::Time::currentTimeMillis()) + ext;
    const auto bucket = FirebaseConfig::storageBucket;
    auto url = juce::URL ("https://firebasestorage.googleapis.com/v0/b/" + bucket
                          + "/o?uploadType=media&name=" + juce::URL::addEscapeChars (objectName, true));
    url = url.withPOSTData (data);

    int status = 0;
    const auto headers = "Authorization: Bearer " + FirestoreClient::getInstance().getIdToken() + "\r\n"
                         + "Content-Type: " + mimeTypeForFile (logoFile) + "\r\n"
                         + "Accept: application/json";

    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs (20000)
        .withExtraHeaders (headers)
        .withHttpRequestCmd ("POST")
        .withStatusCode (&status);

    auto stream = std::unique_ptr<juce::InputStream> (url.createInputStream (opts));
    if (stream == nullptr)
    {
        outError = "Storage upload connection failed.";
        return false;
    }

    const auto response = stream->readEntireStreamAsString();
    if (status < 200 || status >= 300)
    {
        outError = "Storage upload failed (HTTP " + juce::String (status) + ")";
        return false;
    }

    juce::var parsed;
    if (juce::JSON::parse (response, parsed).failed() || ! parsed.isObject())
    {
        outError = "Storage upload response parse failed.";
        return false;
    }

    outStoragePath = parsed.getProperty ("name", objectName).toString();
    const auto token = parsed.getProperty ("downloadTokens", "").toString();
    outLogoUrl = "https://firebasestorage.googleapis.com/v0/b/" + bucket + "/o/"
                 + juce::URL::addEscapeChars (outStoragePath, true) + "?alt=media";
    if (token.isNotEmpty())
        outLogoUrl << "&token=" << juce::URL::addEscapeChars (token, true);

    return true;
}

void CompanyAdminPage::saveCompanyInfo()
{
    const auto companyId = companyIdEditor_.getText().trim();
    if (companyId.isEmpty())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.company_id_required"));
        return;
    }

    const auto companyName = companyNameEditor_.getText().trim();
    if (companyName.isEmpty())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.name_required"));
        return;
    }

    const auto statusValue = companyStatusBox_.getSelectedId() == 2 ? juce::String ("suspended") : juce::String ("active");

    setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.saving_info"));
    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    juce::Thread::launch ([safe, companyId, companyName, statusValue]()
    {
        if (safe == nullptr)
            return;

        auto& fc = FirestoreClient::getInstance();
        juce::String logoUrl = safe->companyLogoUrl_;
        juce::String logoStoragePath = safe->companyLogoStoragePath_;
        juce::String logoFileName = safe->companyLogoFileName_;

        if (safe->selectedLogoFile_.existsAsFile())
        {
            juce::String uploadError;
            if (! safe->uploadLogoToStorage (companyId, safe->selectedLogoFile_, logoUrl, logoStoragePath, uploadError))
            {
                juce::MessageManager::callAsync ([safe, uploadError]()
                {
                    if (safe != nullptr)
                        safe->setStatusMessage (uploadError);
                });
                return;
            }
        }

        auto fields = FirestoreClient::makeFields ({
            { "name",            FirestoreClient::stringValue (companyName) },
            { "status",          FirestoreClient::stringValue (statusValue) },
            { "ownerUserId",     FirestoreClient::stringValue (FirestoreClient::getInstance().getUserId()) },
            { "updatedAt",       FirestoreClient::timestampValue (juce::Time::getCurrentTime()) },
            { "logoUrl",         logoUrl.isNotEmpty() ? FirestoreClient::stringValue (logoUrl) : FirestoreClient::nullValue() },
            { "logoStoragePath", logoStoragePath.isNotEmpty() ? FirestoreClient::stringValue (logoStoragePath) : FirestoreClient::nullValue() },
            { "logoFileName",    logoFileName.isNotEmpty() ? FirestoreClient::stringValue (logoFileName) : FirestoreClient::nullValue() }
        });

        bool ok = false;
        if (safe != nullptr && safe->companyDocExists_)
            ok = fc.patchDocument ("companies/" + companyId, fields);
        else
            ok = fc.createDocument ("companies", fields, companyId).isObject();

        juce::MessageManager::callAsync ([safe, ok, companyId, logoUrl, logoStoragePath]()
        {
            if (safe == nullptr)
                return;

            if (ok)
            {
                safe->companyDocExists_ = true;
                safe->companyId_ = companyId;
                safe->companyIdEditor_.setText (companyId, juce::dontSendNotification);
                safe->companyLogoUrl_ = logoUrl;
                safe->companyLogoStoragePath_ = logoStoragePath;
                safe->selectedLogoFile_ = juce::File();
                if (safe->onCompanyIdChanged)
                    safe->onCompanyIdChanged (companyId);
                safe->setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.saved"));
                safe->loadMembers();
                safe->loadCompanyInfo();
            }
            else
            {
                safe->setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.save_failed"));
            }
        });
    });
}

void CompanyAdminPage::loadMembers()
{
    if (companyId_.isEmpty())
    {
        membersListLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.member_none"), juce::dontSendNotification);
        return;
    }

    membersListLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.members_loading"), juce::dontSendNotification);
    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    const auto companyId = companyId_;
    juce::Thread::launch ([safe, companyId]()
    {
        auto docs = FirestoreClient::getInstance().listCollection ("companies/" + companyId + "/members", 500);
        juce::StringArray lines;
        for (auto& d : docs)
        {
            const auto memberId = d.getProperty ("name", "").toString().fromLastOccurrenceOf ("/", false, false);
            const auto userId = FirestoreClient::readString (d, "userId");
            const auto role = FirestoreClient::readString (d, "role");
            const auto status = FirestoreClient::readString (d, "status");
            lines.add (memberId + "  |  " + (userId.isNotEmpty() ? userId : memberId)
                       + "  |  " + role + "  |  " + status);
        }

        juce::MessageManager::callAsync ([safe, lines]()
        {
            if (safe == nullptr)
                return;

            if (lines.isEmpty())
                safe->membersListLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.member_none"), juce::dontSendNotification);
            else
                safe->membersListLabel_.setText (lines.joinIntoString ("\n"), juce::dontSendNotification);
        });
    });
}

void CompanyAdminPage::saveMemberMapping()
{
    if (companyId_.isEmpty())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.company_id_required"));
        return;
    }

    const auto memberUserId = memberUserIdEditor_.getText().trim();
    if (memberUserId.isEmpty())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.member_user_id_required"));
        return;
    }

    const auto role = memberRoleBox_.getText().trim();
    const auto status = memberStatusBox_.getSelectedId() == 2 ? juce::String ("suspended") : juce::String ("active");
    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    const auto companyId = companyId_;
    setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.member_saving"));

    juce::Thread::launch ([safe, companyId, memberUserId, role, status]()
    {
        auto fields = FirestoreClient::makeFields ({
            { "userId",      FirestoreClient::stringValue (memberUserId) },
            { "role",        FirestoreClient::stringValue (role) },
            { "status",      FirestoreClient::stringValue (status) },
            { "updatedAt",   FirestoreClient::timestampValue (juce::Time::getCurrentTime()) },
            { "updatedBy",   FirestoreClient::stringValue (FirestoreClient::getInstance().getUserId()) }
        });

        const bool ok = FirestoreClient::getInstance().createDocument (
            "companies/" + companyId + "/members", fields, memberUserId).isObject();

        juce::MessageManager::callAsync ([safe, ok]()
        {
            if (safe == nullptr)
                return;
            safe->setStatusMessage (ok ? LocalizationManager::getInstance().getText ("company_admin.member_saved")
                                        : LocalizationManager::getInstance().getText ("company_admin.member_save_failed"));
            if (ok)
            {
                safe->memberUserIdEditor_.clear();
                safe->loadMembers();
            }
        });
    });
}

//==============================================================================
// Venue staff
//==============================================================================
void CompanyAdminPage::loadVenueStaff (const juce::String& venueId)
{
    staffRows_.clear();

    if (venueId.isEmpty())
    {
        staffEmptyLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.staff_select_venue"), juce::dontSendNotification);
        staffEmptyLabel_.setVisible (true);
        resized();
        return;
    }

    staffEmptyLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.staff_loading"), juce::dontSendNotification);
    staffEmptyLabel_.setVisible (true);
    resized();

    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    CompanyService::getInstance().getVenueMembers (venueId,
        [safe, venueId] (bool ok, std::vector<CompanyService::VenueMember> members, juce::String /*error*/)
        {
            if (safe == nullptr || venueId != safe->selectedVenueId_)
                return;

            safe->staffRows_.clear();
            safe->staffEmptyLabel_.setVisible (! ok || members.empty());
            if (! ok || members.empty())
                safe->staffEmptyLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.staff_none"), juce::dontSendNotification);

            for (auto& m : members)
            {
                auto row = std::make_unique<StaffRow>();
                row->userId = m.userId;

                juce::String display = m.stageName.isNotEmpty() ? m.stageName
                                       : m.email.isNotEmpty() ? m.email : m.userId;
                row->nameLabel.setText (display, juce::dontSendNotification);
                row->nameLabel.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)).boldened());
                row->nameLabel.setColour (juce::Label::textColourId, kText);
                safe->contentHolder_->addAndMakeVisible (row->nameLabel);

                row->roleStatusLabel.setText (m.role + "  |  " + m.status, juce::dontSendNotification);
                row->roleStatusLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
                row->roleStatusLabel.setColour (juce::Label::textColourId, kMuted);
                safe->contentHolder_->addAndMakeVisible (row->roleStatusLabel);

                row->removeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7f1d1d));
                row->removeButton.setColour (juce::TextButton::textColourOnId, kText);
                row->removeButton.setColour (juce::TextButton::textColourOffId, kText);
                const auto userId = m.userId;
                row->removeButton.onClick = [safe, venueId, userId]() { if (safe != nullptr) safe->removeVenueStaffMember (venueId, userId); };
                safe->contentHolder_->addAndMakeVisible (row->removeButton);

                safe->staffRows_.push_back (std::move (row));
            }

            safe->resized();
        });
}

void CompanyAdminPage::inviteVenueStaff()
{
    if (selectedVenueId_.isEmpty())
        return;

    const auto email = staffInviteEmailEditor_.getText().trim();
    if (email.isEmpty())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.staff_email_required"));
        return;
    }

    const auto role = staffInviteRoleBox_.getText().trim();
    const auto venueId = selectedVenueId_;
    setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.staff_inviting"));

    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    InvitationService::getInstance().addVenueMember (venueId, email, role,
        [safe, venueId] (bool ok, bool /*activated*/, juce::String error)
        {
            if (safe == nullptr)
                return;

            safe->setStatusMessage (ok ? LocalizationManager::getInstance().getText ("company_admin.staff_invited")
                : (error.isNotEmpty() ? error : LocalizationManager::getInstance().getText ("company_admin.staff_invite_failed")));

            if (ok)
            {
                safe->staffInviteEmailEditor_.clear();
                safe->loadVenueStaff (venueId);
            }
        });
}

void CompanyAdminPage::removeVenueStaffMember (const juce::String& venueId, const juce::String& userId)
{
    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    CompanyService::getInstance().removeVenueMember (venueId, userId,
        [safe, venueId] (bool ok, juce::String error)
        {
            if (safe == nullptr)
                return;

            if (ok)
                safe->loadVenueStaff (venueId);
            else
                safe->setStatusMessage (error.isNotEmpty() ? error
                    : LocalizationManager::getInstance().getText ("company_admin.staff_remove_failed"));
        });
}

//==============================================================================
// Song distribution
//==============================================================================
void CompanyAdminPage::chooseSongFile()
{
    fileChooser_ = std::make_unique<juce::FileChooser> (
        LocalizationManager::getInstance().getText ("company_admin.songs_choose_file"),
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.cdg;*.zip;*.mp4;*.m4a");

    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    fileChooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe] (const juce::FileChooser& chooser)
        {
            if (safe == nullptr)
                return;

            auto file = chooser.getResult();
            if (! file.existsAsFile())
                return;

            safe->selectedSongFile_ = file;
            safe->songFileLabel_.setText (file.getFileName(), juce::dontSendNotification);
        });
}

void CompanyAdminPage::sendSongToTargetVenues()
{
    if (! selectedSongFile_.existsAsFile())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.songs_no_file"));
        return;
    }

    std::vector<juce::String> targetVenueIds;
    if (targetAllVenues_)
    {
        for (auto& t : venueTargets_)
            targetVenueIds.push_back (t->venueId);
    }
    else
    {
        for (auto& t : venueTargets_)
            if (t->toggle.getToggleState())
                targetVenueIds.push_back (t->venueId);
    }

    if (targetVenueIds.empty())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.songs_no_target"));
        return;
    }

    uploadSongButton_.setEnabled (false);
    setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.songs_uploading"));

    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    SongDeliveryService::getInstance().uploadSongToVenues (selectedSongFile_, targetVenueIds,
        [safe] (bool ok, int succeeded, int total, juce::String error)
        {
            if (safe == nullptr)
                return;

            safe->uploadSongButton_.setEnabled (true);
            auto& lm = LocalizationManager::getInstance();
            safe->setStatusMessage (ok
                ? lm.getText ("company_admin.songs_upload_done").replace ("{ok}", juce::String (succeeded)).replace ("{total}", juce::String (total))
                : (error.isNotEmpty() ? error : lm.getText ("company_admin.songs_upload_failed")));
        });
}

//==============================================================================
void CompanyAdminPage::setSummary (const Summary& summary)
{
    summary_ = summary;
    venueCard_.value.setText (juce::String (summary_.venues), juce::dontSendNotification);
    hostCard_.value.setText (juce::String (summary_.hosts), juce::dontSendNotification);
    deviceCard_.value.setText (juce::String (summary_.devices), juce::dontSendNotification);
    packageCard_.value.setText (juce::String (summary_.songPackages), juce::dontSendNotification);
    campaignCard_.value.setText (juce::String (summary_.campaigns), juce::dontSendNotification);
}

void CompanyAdminPage::setStatusMessage (const juce::String& message)
{
    statusMessage_ = message;
    status_.setText (statusMessage_, juce::dontSendNotification);
}

void CompanyAdminPage::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    subtitle_.setText ((companyId_.isNotEmpty() ? juce::String() : lm.getText ("company_admin.no_company"))
                       + (companyRole_.isNotEmpty() ? "  |  Role: " + companyRole_ : ""),
                       juce::dontSendNotification);
    status_.setText (statusMessage_, juce::dontSendNotification);
    editCompanyToggle_.setButtonText (lm.getText ("company_admin.edit_company_toggle"));
    companyInfoTitle_.setText (lm.getText ("company_admin.edit_info"), juce::dontSendNotification);
    companyIdLabel_.setText (lm.getText ("company_admin.company_id") + ": " + companyId_, juce::dontSendNotification);
    companyIdEditor_.setTextToShowWhenEmpty (lm.getText ("company_admin.company_id_hint"), kMuted);
    applyCompanyIdButton_.setButtonText (lm.getText ("company_admin.use_company_id"));
    companyNameLabel_.setText (lm.getText ("company_admin.company_name"), juce::dontSendNotification);
    companyStatusLabel_.setText (lm.getText ("company_admin.company_status"), juce::dontSendNotification);
    logoLabel_.setText (lm.getText ("company_admin.logo"), juce::dontSendNotification);
    if (logoPathLabel_.getText().isEmpty())
        logoPathLabel_.setText (lm.getText ("company_admin.no_logo"), juce::dontSendNotification);
    venueCard_.title.setText (lm.getText ("company_admin.venues"), juce::dontSendNotification);
    hostCard_.title.setText (lm.getText ("company_admin.hosts"), juce::dontSendNotification);
    deviceCard_.title.setText (lm.getText ("company_admin.devices"), juce::dontSendNotification);
    packageCard_.title.setText (lm.getText ("company_admin.packages"), juce::dontSendNotification);
    campaignCard_.title.setText (lm.getText ("company_admin.campaigns"), juce::dontSendNotification);
    browseLogoButton_.setButtonText (lm.getText ("company_admin.select_logo"));
    clearLogoButton_.setButtonText (lm.getText ("company_admin.clear_logo"));
    saveCompanyButton_.setButtonText (lm.getText ("company_admin.save_info"));
    membersTitle_.setText (lm.getText ("company_admin.members_title"), juce::dontSendNotification);
    memberUserIdLabel_.setText (lm.getText ("company_admin.member_user_id"), juce::dontSendNotification);
    memberRoleLabel_.setText (lm.getText ("company_admin.member_role"), juce::dontSendNotification);
    memberStatusLabel_.setText (lm.getText ("company_admin.member_status"), juce::dontSendNotification);
    memberUserIdEditor_.setTextToShowWhenEmpty (lm.getText ("company_admin.member_user_id_hint"), kMuted);
    saveMemberButton_.setButtonText (lm.getText ("company_admin.member_save"));
    refreshMembersButton_.setButtonText (lm.getText ("company_admin.member_refresh"));
    venuesTitle_.setText (lm.getText ("company_admin.venues_title"), juce::dontSendNotification);
    if (venueCards_.empty())
        venuesEmptyLabel_.setText (lm.getText ("company_admin.venue_none"), juce::dontSendNotification);
    refreshButton_.setButtonText (lm.getText ("company_admin.refresh"));
    registerButton_.setButtonText (lm.getText ("company_admin.register_device"));
    staffTitle_.setText (lm.getText ("company_admin.staff_title"), juce::dontSendNotification);
    staffInviteEmailEditor_.setTextToShowWhenEmpty (lm.getText ("company_admin.staff_email_hint"), kMuted);
    staffInviteButton_.setButtonText (lm.getText ("company_admin.staff_add"));
    songSectionTitle_.setText (lm.getText ("company_admin.songs_title"), juce::dontSendNotification);
    browseSongButton_.setButtonText (lm.getText ("company_admin.songs_choose_file"));
    targetAllVenuesToggle_.setButtonText (lm.getText ("company_admin.songs_all_venues"));
    uploadSongButton_.setButtonText (lm.getText ("company_admin.songs_send"));
}

void CompanyAdminPage::paint (juce::Graphics& g)
{
    MenuTheme::drawPageBackground (g, getLocalBounds());
}

void CompanyAdminPage::layoutVenueCards (juce::Rectangle<int> area)
{
    if (venueCards_.empty())
    {
        venuesEmptyLabel_.setVisible (true);
        venuesEmptyLabel_.setBounds (area.removeFromTop (24));
        return;
    }

    venuesEmptyLabel_.setVisible (false);
    int x = 0, y = 0;
    for (auto& card : venueCards_)
    {
        card->setBounds (area.getX() + x, area.getY() + y, kCardWidth, kCardHeight);
        x += kCardWidth + kCardGap;
        if (x + kCardWidth > area.getWidth())
        {
            x = 0;
            y += kCardHeight + kCardGap;
        }
    }
}

void CompanyAdminPage::layoutStaffRows (juce::Rectangle<int> area)
{
    if (staffRows_.empty())
    {
        staffEmptyLabel_.setVisible (true);
        staffEmptyLabel_.setBounds (area.removeFromTop (24));
        return;
    }

    staffEmptyLabel_.setVisible (false);
    for (auto& row : staffRows_)
    {
        auto rowArea = area.removeFromTop (36);
        row->removeButton.setBounds (rowArea.removeFromRight (90));
        rowArea.removeFromRight (8);
        row->nameLabel.setBounds (rowArea.removeFromTop (18));
        row->roleStatusLabel.setBounds (rowArea);
    }
}

void CompanyAdminPage::layoutSongSection (juce::Rectangle<int> area)
{
    songFileLabel_.setBounds (area.removeFromTop (20));
    auto pickerRow = area.removeFromTop (28);
    browseSongButton_.setBounds (pickerRow.removeFromLeft (170));
    pickerRow.removeFromLeft (8);
    uploadSongButton_.setBounds (pickerRow.removeFromLeft (150));
    area.removeFromTop (6);
    targetAllVenuesToggle_.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);

    if (targetAllVenues_)
        return;

    const int cols = juce::jmax (1, area.getWidth() / 180);
    int col = 0;
    juce::Rectangle<int> row;
    for (auto& t : venueTargets_)
    {
        if (col == 0)
            row = area.removeFromTop (22);
        t->toggle.setBounds (row.removeFromLeft (180));
        if (++col >= cols)
            col = 0;
    }
}

void CompanyAdminPage::layoutCard (StatCard& card, juce::Rectangle<int> area)
{
    card.title.setBounds (area.removeFromTop (24));
    card.value.setBounds (area.reduced (0, 8));
}

void CompanyAdminPage::resized()
{
    viewport_.setBounds (getLocalBounds());

    const int startingWidth  = juce::jmax (900, viewport_.getWidth() - viewport_.getScrollBarThickness());
    const int startingHeight = juce::jmax (contentHolder_->getHeight(), 700);
    contentHolder_->setSize (startingWidth, startingHeight);
    layoutContent();

    // Grow to fit the stat-card grid at the bottom -- lets the viewport's
    // scrollbar reach it on any window size. layoutContent() only depends
    // on width, so a second pass at the corrected height reproduces the
    // same positions.
    const int neededHeight = juce::jmax (deviceCard_.value.getBottom(), campaignCard_.value.getBottom()) + 16;
    if (neededHeight != contentHolder_->getHeight())
    {
        contentHolder_->setSize (startingWidth, neededHeight);
        layoutContent();
    }
}

void CompanyAdminPage::layoutContent()
{
    auto bounds = contentHolder_->getLocalBounds().reduced (28);

    // --- Header: logo left, name in title font, right --------------------
    auto header = bounds.removeFromTop (110).reduced (18, 16);
    headerPanelBounds_ = header.expanded (18, 16);

    auto logoArea = header.removeFromLeft (78);
    headerLogoBounds_ = logoArea.withHeight (78);
    header.removeFromLeft (16);

    title_.setBounds (header.removeFromTop (36));
    subtitle_.setBounds (header.removeFromTop (22));
    status_.setBounds (header.removeFromTop (18));

    auto buttonsRow = header.removeFromTop (28);
    refreshButton_.setBounds (buttonsRow.removeFromLeft (100));
    buttonsRow.removeFromLeft (8);
    registerButton_.setBounds (buttonsRow.removeFromLeft (140));
    buttonsRow.removeFromLeft (8);
    editCompanyToggle_.setBounds (buttonsRow.removeFromLeft (150));

    bounds.removeFromTop (16);

    // --- Edit Company Info (collapsed by default) -------------------------
    if (companyEditFormVisible_)
    {
        auto formArea = bounds.removeFromTop (220);
        editFormPanelBounds_ = formArea;
        auto preview = formArea.removeFromRight (180).reduced (16);
        juce::ignoreUnused (preview);

        companyInfoTitle_.setBounds (formArea.removeFromTop (24));
        companyIdEditor_.setBounds (formArea.removeFromTop (28).reduced (0, 2));
        formArea.removeFromTop (4);
        applyCompanyIdButton_.setBounds (formArea.removeFromTop (28).withWidth (180));
        formArea.removeFromTop (4);
        companyIdLabel_.setBounds (formArea.removeFromTop (20));
        formArea.removeFromTop (6);
        companyNameLabel_.setBounds (formArea.removeFromTop (20));
        companyNameEditor_.setBounds (formArea.removeFromTop (28).reduced (0, 2));
        formArea.removeFromTop (4);
        companyStatusLabel_.setBounds (formArea.removeFromTop (20));
        companyStatusBox_.setBounds (formArea.removeFromTop (28).withWidth (160));
        formArea.removeFromTop (6);
        logoLabel_.setBounds (formArea.removeFromTop (20));
        logoPathLabel_.setBounds (formArea.removeFromTop (22));
        auto logoButtons = formArea.removeFromTop (30);
        browseLogoButton_.setBounds (logoButtons.removeFromLeft (120));
        logoButtons.removeFromLeft (8);
        clearLogoButton_.setBounds (logoButtons.removeFromLeft (100));
        logoButtons.removeFromLeft (8);
        saveCompanyButton_.setBounds (logoButtons.removeFromLeft (150));

        bounds.removeFromTop (12);
    }

    for (juce::Component* c : std::initializer_list<juce::Component*> { &companyInfoTitle_, &companyIdEditor_, &applyCompanyIdButton_, &companyIdLabel_,
                     &companyNameLabel_, &companyNameEditor_, &companyStatusLabel_, &companyStatusBox_,
                     &logoLabel_, &logoPathLabel_, &browseLogoButton_, &clearLogoButton_, &saveCompanyButton_ })
        c->setVisible (companyEditFormVisible_);

    // --- Venue cards -------------------------------------------------------
    venuesTitle_.setBounds (bounds.removeFromTop (22));
    bounds.removeFromTop (4);
    const int cardColumns = juce::jmax (1, (bounds.getWidth() + kCardGap) / (kCardWidth + kCardGap));
    const int cardRows = venueCards_.empty() ? 1
        : (int) ((venueCards_.size() + (size_t) cardColumns - 1) / (size_t) cardColumns);
    const int venuesAreaHeight = venueCards_.empty() ? 30
        : cardRows * (kCardHeight + kCardGap);
    auto venuesArea = bounds.removeFromTop (venuesAreaHeight);
    layoutVenueCards (venuesArea);

    bounds.removeFromTop (16);

    // --- Venue staff (selected card) ---------------------------------------
    staffTitle_.setBounds (bounds.removeFromTop (22));
    bounds.removeFromTop (4);
    const int staffAreaHeight = 40 + (staffRows_.empty() ? 24 : (int) staffRows_.size() * 36);
    auto staffArea = bounds.removeFromTop (staffAreaHeight).reduced (12, 0);
    layoutStaffRows (staffArea);
    auto inviteRow = bounds.removeFromTop (34).reduced (12, 0);
    staffInviteEmailEditor_.setBounds (inviteRow.removeFromLeft (240));
    inviteRow.removeFromLeft (8);
    staffInviteRoleBox_.setBounds (inviteRow.removeFromLeft (120));
    inviteRow.removeFromLeft (8);
    staffInviteButton_.setBounds (inviteRow.removeFromLeft (100));
    staffInviteEmailEditor_.setVisible (selectedVenueId_.isNotEmpty());
    staffInviteRoleBox_.setVisible (selectedVenueId_.isNotEmpty());
    staffInviteButton_.setVisible (selectedVenueId_.isNotEmpty());

    bounds.removeFromTop (16);

    // --- Song distribution ---------------------------------------------------
    songSectionTitle_.setBounds (bounds.removeFromTop (22));
    bounds.removeFromTop (4);
    const int targetRows = targetAllVenues_ ? 0
        : (venueTargets_.empty() ? 0 : ((int) venueTargets_.size() + juce::jmax (1, bounds.getWidth() / 180) - 1) / juce::jmax (1, bounds.getWidth() / 180));
    const int songAreaHeight = 96 + targetRows * 22;
    auto songArea = bounds.removeFromTop (songAreaHeight).reduced (12, 0);
    layoutSongSection (songArea);

    bounds.removeFromTop (12);

    // --- Members (company-wide) ---------------------------------------------
    auto membersArea = bounds.removeFromTop (160).reduced (12);
    membersTitle_.setBounds (membersArea.removeFromTop (22));
    membersArea.removeFromTop (4);
    memberUserIdLabel_.setBounds (membersArea.removeFromTop (18));
    memberUserIdEditor_.setBounds (membersArea.removeFromTop (26));
    membersArea.removeFromTop (4);
    {
        auto row = membersArea.removeFromTop (18);
        memberRoleLabel_.setBounds (row.removeFromLeft (160));
        row.removeFromLeft (8);
        memberStatusLabel_.setBounds (row.removeFromLeft (160));

        auto boxRow = membersArea.removeFromTop (24);
        memberRoleBox_.setBounds (boxRow.removeFromLeft (160));
        boxRow.removeFromLeft (8);
        memberStatusBox_.setBounds (boxRow.removeFromLeft (160));
    }
    membersArea.removeFromTop (6);
    auto memberButtons = membersArea.removeFromTop (28);
    saveMemberButton_.setBounds (memberButtons.removeFromLeft (170));
    memberButtons.removeFromLeft (8);
    refreshMembersButton_.setBounds (memberButtons.removeFromLeft (140));
    membersArea.removeFromTop (8);
    membersListLabel_.setBounds (membersArea);

    bounds.removeFromTop (12);
    // Fixed height, not "whatever's left" -- the content now grows to fit
    // its own needs rather than being squeezed into a given window size, so
    // there's no natural "remainder" to fill.
    auto grid = bounds.removeFromTop (212);
    auto row1 = grid.removeFromTop ((grid.getHeight() / 2) - 8);
    auto row2 = grid;
    auto cardW = (row1.getWidth() - 16) / 3;
    layoutCard (venueCard_,   row1.removeFromLeft (cardW));
    row1.removeFromLeft (8);
    layoutCard (hostCard_,    row1.removeFromLeft (cardW));
    row1.removeFromLeft (8);
    layoutCard (deviceCard_,  row1);

    auto cardW2 = (row2.getWidth() - 8) / 2;
    layoutCard (packageCard_, row2.removeFromLeft (cardW2));
    row2.removeFromLeft (8);
    layoutCard (campaignCard_, row2);
}
