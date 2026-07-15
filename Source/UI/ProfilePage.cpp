/*
  ==============================================================================

    ProfilePage.cpp

  ==============================================================================
*/

#include "ProfilePage.h"
#include "MenuTheme.h"
#include "../Localization/LocalizationManager.h"
#include "../Services/FirestoreClient.h"
#include "../Services/HostService.h"
#include "../Models/AccessRights.h"
#include <algorithm>

namespace
{
    const juce::Colour kBg     { 0xff16213e };
    const juce::Colour kPanel  { 0x99182a52 };
    const juce::Colour kBorder { 0x664f78c4 };
    const juce::Colour kAccent { 0xff5a8fd8 };
    const juce::Colour kText   { 0xffffffff };
    const juce::Colour kMuted  { 0xffc7d2e0 };
    const juce::Colour kDanger { 0xffd9534f };

    void styleLabel (juce::Label& l, float height, bool bold, juce::Colour colour)
    {
        auto font = juce::Font (juce::FontOptions().withHeight (height));
        if (bold) font = font.boldened();
        l.setFont (font);
        l.setColour (juce::Label::textColourId, colour);
    }

    void styleEditor (juce::TextEditor& e, const juce::String& hint)
    {
        e.setTextToShowWhenEmpty (hint, kMuted);
        e.setColour (juce::TextEditor::backgroundColourId, kPanel);
        e.setColour (juce::TextEditor::textColourId, kText);
        e.setColour (juce::TextEditor::outlineColourId, kBorder);
        e.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
    }

    juce::File assetsDir()
    {
        return juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                   .getParentDirectory().getChildFile ("assets");
    }

    bool looksLikeValidEmail (const juce::String& s)
    {
        const auto at = s.indexOfChar ('@');
        if (at <= 0) return false;
        const auto rest = s.substring (at + 1);
        return rest.containsChar ('.') && rest.length() > 2;
    }
}

//==============================================================================
void ProfilePage::AvatarTile::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (selected)
    {
        g.setColour (kAccent.withAlpha (0.35f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (kAccent);
        g.drawRoundedRectangle (bounds.reduced (1.0f), 8.0f, 2.0f);
    }

    if (image.isValid())
        g.drawImageWithin (image, 4, 4, getWidth() - 8, getHeight() - 8,
                           juce::RectanglePlacement::centred, false);
}

//==============================================================================
ProfilePage::ProfilePage()
{
    setOpaque (true);
    auto& lm = LocalizationManager::getInstance();

    styleLabel (title_, 28.0f, true, kText);
    title_.setText (lm.getText ("page.profile.title"), juce::dontSendNotification);
    addAndMakeVisible (title_);

    styleLabel (subtitle_, 13.0f, false, kMuted);
    subtitle_.setText (lm.getText ("page.profile.subtitle"), juce::dontSendNotification);
    addAndMakeVisible (subtitle_);

    styleLabel (statusLabel_, 12.0f, false, kMuted);
    addAndMakeVisible (statusLabel_);

    auto setupField = [this] (juce::Label& label, juce::TextEditor& editor,
                              const juce::String& labelKey, const juce::String& hintKey)
    {
        auto& lmRef = LocalizationManager::getInstance();
        styleLabel (label, 12.0f, false, kMuted);
        label.setText (lmRef.getText (labelKey), juce::dontSendNotification);
        addAndMakeVisible (label);
        styleEditor (editor, hintKey.isNotEmpty() ? lmRef.getText (hintKey) : juce::String());
        editor.onTextChange = [this] { updateSaveButtonState(); };
        addAndMakeVisible (editor);
    };

    setupField (fullNameLabel_,  fullNameEditor_,  "page.profile.full_name", {});
    setupField (emailLabel_,     emailEditor_,     "page.profile.email", {});
    setupField (stageNameLabel_, stageNameEditor_, "page.profile.stage_name", {});
    setupField (birthdayLabel_,  birthdayEditor_,  "page.profile.birthday", "page.profile.birthday_hint");
    setupField (countryLabel_,   countryEditor_,   "page.profile.country", {});
    setupField (cityLabel_,      cityEditor_,      "page.profile.city", {});

    styleLabel (genderLabel_, 12.0f, false, kMuted);
    genderLabel_.setText (lm.getText ("page.profile.gender"), juce::dontSendNotification);
    addAndMakeVisible (genderLabel_);

    genderBox_.addItem (lm.getText ("page.profile.gender_male"), 1);
    genderBox_.addItem (lm.getText ("page.profile.gender_female"), 2);
    genderBox_.addItem (lm.getText ("page.profile.gender_other"), 3);
    genderBox_.setColour (juce::ComboBox::backgroundColourId, kPanel);
    genderBox_.setColour (juce::ComboBox::textColourId, kText);
    genderBox_.setColour (juce::ComboBox::outlineColourId, kBorder);
    addAndMakeVisible (genderBox_);

    // Role is READ-ONLY here, deliberately -- see the header comment for why
    // (self-service role editing would be a privilege-escalation hole given
    // this app's Firestore rules place no field-level restriction on a
    // host's own document update).
    styleLabel (roleLabel_, 12.0f, false, kMuted);
    roleLabel_.setText (lm.getText ("page.profile.role"), juce::dontSendNotification);
    addAndMakeVisible (roleLabel_);
    styleLabel (roleValueLabel_, 13.0f, true, kText);
    addAndMakeVisible (roleValueLabel_);

    styleLabel (avatarSectionLabel_, 13.0f, true, kText);
    avatarSectionLabel_.setText (lm.getText ("page.profile.avatar_section"), juce::dontSendNotification);
    addAndMakeVisible (avatarSectionLabel_);

    addAndMakeVisible (avatarGridHolder_);
    buildAvatarGrid();

    saveButton_.setColour (juce::TextButton::buttonColourId, kAccent);
    saveButton_.onClick = [this] { onSaveClicked(); };
    saveButton_.setEnabled (false);
    addAndMakeVisible (saveButton_);
}

ProfilePage::~ProfilePage() = default;

//==============================================================================
void ProfilePage::buildAvatarGrid()
{
    avatarFileNames_.clear();
    avatarTiles_.clear();

    auto iconDir = assetsDir().getChildFile ("icon");
    if (! iconDir.isDirectory())
        return;

    juce::Array<juce::File> files;
    iconDir.findChildFiles (files, juce::File::findFiles, false, "*.png");
    files.sort();

    for (auto& f : files)
        avatarFileNames_.push_back (f.getFileName());

    for (int i = 0; i < (int) avatarFileNames_.size(); ++i)
    {
        auto tile = std::make_unique<AvatarTile>();
        tile->tileIndex = i;
        tile->image = juce::ImageFileFormat::loadFrom (iconDir.getChildFile (avatarFileNames_[(size_t) i]));
        tile->onClicked = [this] (int idx) { selectAvatar (idx); };
        avatarGridHolder_.addAndMakeVisible (tile.get());
        avatarTiles_.push_back (std::move (tile));
    }
}

void ProfilePage::selectAvatar (int tileIndex)
{
    if (tileIndex < 0 || tileIndex >= (int) avatarFileNames_.size())
        return;

    selectedAvatarUrl_ = "assets/icon/" + avatarFileNames_[(size_t) tileIndex];

    for (auto& t : avatarTiles_)
        t->selected = (t->tileIndex == tileIndex);

    avatarGridHolder_.repaint();
    updateSaveButtonState();
}

//==============================================================================
void ProfilePage::loadFromCurrentHost()
{
    const auto host = HostService::getInstance().getCurrent();
    currentUid_ = juce::String (host.userId);

    fullNameEditor_.setText  (juce::String (host.fullName), false);
    emailEditor_.setText     (juce::String (host.email), false);
    stageNameEditor_.setText (juce::String (host.stageName), false);
    birthdayEditor_.setText  (juce::String (host.birthday), false);
    countryEditor_.setText   (juce::String (host.country), false);
    cityEditor_.setText      (juce::String (host.city), false);

    const auto genderStr = juce::String (host.gender).trim().toLowerCase();
    if (genderStr == "male")        genderBox_.setSelectedId (1, juce::dontSendNotification);
    else if (genderStr == "female") genderBox_.setSelectedId (2, juce::dontSendNotification);
    else if (genderStr.isNotEmpty()) genderBox_.setSelectedId (3, juce::dontSendNotification);
    else genderBox_.setSelectedId (0, juce::dontSendNotification);

    roleValueLabel_.setText (AccessRightsUtil::userRoleToString (host.role), juce::dontSendNotification);

    selectedAvatarUrl_ = juce::String (host.avatarUrl);
    const auto selectedFileName = selectedAvatarUrl_.fromLastOccurrenceOf ("/", false, false);
    for (auto& t : avatarTiles_)
        t->selected = (t->tileIndex >= 0
                       && t->tileIndex < (int) avatarFileNames_.size()
                       && avatarFileNames_[(size_t) t->tileIndex] == selectedFileName);
    avatarGridHolder_.repaint();

    loaded_ = true;
    setStatus ({});
    updateSaveButtonState();
}

//==============================================================================
void ProfilePage::updateSaveButtonState()
{
    const bool valid = loaded_
                     && fullNameEditor_.getText().trim().isNotEmpty()
                     && looksLikeValidEmail (emailEditor_.getText().trim());
    saveButton_.setEnabled (valid);
}

void ProfilePage::onSaveClicked()
{
    if (currentUid_.isEmpty())
    {
        setStatus ("No signed-in profile to save.", true);
        return;
    }

    juce::String genderStr;
    switch (genderBox_.getSelectedId())
    {
        case 1: genderStr = "Male";   break;
        case 2: genderStr = "Female"; break;
        case 3: genderStr = "Other";  break;
        default: break;
    }

    const auto fullName  = fullNameEditor_.getText().trim();
    const auto email     = emailEditor_.getText().trim();
    const auto stageName = stageNameEditor_.getText().trim();
    const auto birthday  = birthdayEditor_.getText().trim();
    const auto country   = countryEditor_.getText().trim();
    const auto city      = cityEditor_.getText().trim();
    const auto avatarUrl = selectedAvatarUrl_;
    const auto uid        = currentUid_;

    setStatus ("Saving...");
    saveButton_.setEnabled (false);

    juce::Component::SafePointer<ProfilePage> safe (this);
    juce::Thread::launch([safe, uid, fullName, email, stageName, birthday, country, city, avatarUrl, genderStr]
    {
        using FC = FirestoreClient;
        auto fields = FC::makeFields({
            { "fullName",  FC::stringValue (fullName) },
            { "email",     FC::stringValue (email) },
            { "stageName", FC::stringValue (stageName) },
            { "birthday",  FC::stringValue (birthday) },
            { "country",   FC::stringValue (country) },
            { "city",      FC::stringValue (city) },
            { "gender",    FC::stringValue (genderStr) },
            { "avatarUrl", FC::stringValue (avatarUrl) }
        });

        const auto path = "hosts/" + uid
                         + "?updateMask.fieldPaths=fullName"
                         + "&updateMask.fieldPaths=email"
                         + "&updateMask.fieldPaths=stageName"
                         + "&updateMask.fieldPaths=birthday"
                         + "&updateMask.fieldPaths=country"
                         + "&updateMask.fieldPaths=city"
                         + "&updateMask.fieldPaths=gender"
                         + "&updateMask.fieldPaths=avatarUrl";
        const bool ok = FC::getInstance().patchDocument (path, fields);

        juce::MessageManager::callAsync([safe, ok, fullName, email, stageName, birthday, country, city, avatarUrl, genderStr]
        {
            if (safe == nullptr) return;

            if (! ok)
            {
                safe->setStatus ("Save failed -- please try again.", true);
                safe->updateSaveButtonState();
                return;
            }

            // Keep the in-memory cache (read by TopBar, QueueBar host pin,
            // etc.) in sync with what was just written.
            auto host = HostService::getInstance().getCurrent();
            host.fullName  = fullName.toStdString();
            host.email     = email.toStdString();
            host.stageName = stageName.toStdString();
            host.birthday  = birthday.toStdString();
            host.country   = country.toStdString();
            host.city      = city.toStdString();
            host.gender    = genderStr.toStdString();
            host.avatarUrl = avatarUrl.toStdString();
            HostService::getInstance().setCurrent (host);

            safe->setStatus ("Profile saved.");
            safe->updateSaveButtonState();

            if (safe->onProfileSaved)
                safe->onProfileSaved();
        });
    });
}

//==============================================================================
void ProfilePage::setStatus (const juce::String& message, bool isError)
{
    statusLabel_.setColour (juce::Label::textColourId, isError ? kDanger : kMuted);
    statusLabel_.setText (message, juce::dontSendNotification);
}

//==============================================================================
void ProfilePage::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    title_.setText (lm.getText ("page.profile.title"), juce::dontSendNotification);
    subtitle_.setText (lm.getText ("page.profile.subtitle"), juce::dontSendNotification);
    fullNameLabel_.setText (lm.getText ("page.profile.full_name"), juce::dontSendNotification);
    emailLabel_.setText (lm.getText ("page.profile.email"), juce::dontSendNotification);
    stageNameLabel_.setText (lm.getText ("page.profile.stage_name"), juce::dontSendNotification);
    birthdayLabel_.setText (lm.getText ("page.profile.birthday"), juce::dontSendNotification);
    countryLabel_.setText (lm.getText ("page.profile.country"), juce::dontSendNotification);
    cityLabel_.setText (lm.getText ("page.profile.city"), juce::dontSendNotification);
    genderLabel_.setText (lm.getText ("page.profile.gender"), juce::dontSendNotification);
    roleLabel_.setText (lm.getText ("page.profile.role"), juce::dontSendNotification);
    avatarSectionLabel_.setText (lm.getText ("page.profile.avatar_section"), juce::dontSendNotification);
}

//==============================================================================
void ProfilePage::paint (juce::Graphics& g)
{
    MenuTheme::drawPageBackground (g, getLocalBounds());

    auto bounds = getLocalBounds().reduced (22);
    auto header = bounds.removeFromTop (80);
    MenuTheme::drawHeaderPanel (g, header);

    bounds.removeFromTop (14);
    MenuTheme::drawHeaderPanel (g, bounds);
}

void ProfilePage::resized()
{
    auto bounds = getLocalBounds().reduced (22);

    auto header = bounds.removeFromTop (80).reduced (16, 10);
    title_.setBounds (header.removeFromTop (30));
    subtitle_.setBounds (header.removeFromTop (20));
    statusLabel_.setBounds (header.removeFromTop (18));

    bounds.removeFromTop (14);
    auto content = bounds.reduced (18, 14);

    auto leftColumn = content.removeFromLeft (content.getWidth() / 2).reduced (0, 0);
    content.removeFromLeft (20);
    auto rightColumn = content;

    auto layoutField = [] (juce::Rectangle<int>& area, juce::Label& label, juce::Component& editor)
    {
        label.setBounds (area.removeFromTop (16));
        editor.setBounds (area.removeFromTop (28).reduced (0, 2));
        area.removeFromTop (10);
    };

    layoutField (leftColumn, fullNameLabel_,  fullNameEditor_);
    layoutField (leftColumn, emailLabel_,     emailEditor_);
    layoutField (leftColumn, stageNameLabel_, stageNameEditor_);
    layoutField (leftColumn, birthdayLabel_,  birthdayEditor_);
    layoutField (leftColumn, countryLabel_,   countryEditor_);
    layoutField (leftColumn, cityLabel_,      cityEditor_);

    genderLabel_.setBounds (leftColumn.removeFromTop (16));
    genderBox_.setBounds (leftColumn.removeFromTop (28).withWidth (200));
    leftColumn.removeFromTop (10);

    roleLabel_.setBounds (leftColumn.removeFromTop (16));
    roleValueLabel_.setBounds (leftColumn.removeFromTop (22));
    leftColumn.removeFromTop (14);

    saveButton_.setBounds (leftColumn.removeFromTop (34).withWidth (200));

    //--- Right column: avatar picker grid --------------------------------
    avatarSectionLabel_.setBounds (rightColumn.removeFromTop (20));
    rightColumn.removeFromTop (6);
    avatarGridHolder_.setBounds (rightColumn);

    const int tileSize = 56;
    const int gap = 8;
    const int cols = juce::jmax (1, (avatarGridHolder_.getWidth() + gap) / (tileSize + gap));

    for (int i = 0; i < (int) avatarTiles_.size(); ++i)
    {
        const int row = i / cols;
        const int col = i % cols;
        avatarTiles_[(size_t) i]->setBounds (col * (tileSize + gap), row * (tileSize + gap), tileSize, tileSize);
    }
}
