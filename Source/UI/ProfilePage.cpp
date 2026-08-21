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

    // Minimum width the two-column form is laid out at -- the ContentPanel
    // grows to at least this, so the viewport scrolls horizontally rather
    // than the columns getting squeezed into degenerate (negative-size)
    // rectangles on a narrow window.
    constexpr int kMinContentWidth = 640;

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
// ProfilePage::ContentPanel — the actual form, at a fixed natural size.
// Lives inside ProfilePage's Viewport; resized() grows itself to fit
// whatever the avatar grid and field columns actually need, so the
// viewport's scroll range always covers every child, however small the
// window gets.
class ProfilePage::ContentPanel : public juce::Component
{
public:
    ContentPanel()
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

        // Role is READ-ONLY here, deliberately -- see the header comment for
        // why (self-service role editing would be a privilege-escalation
        // hole given this app's Firestore rules place no field-level
        // restriction on a host's own document update).
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

    //--------------------------------------------------------------------------
    void paint (juce::Graphics& g) override
    {
        MenuTheme::drawPageBackground (g, getLocalBounds());

        auto bounds = getLocalBounds().reduced (22);
        auto header = bounds.removeFromTop (80);
        MenuTheme::drawHeaderPanel (g, header);

        bounds.removeFromTop (14);
        MenuTheme::drawHeaderPanel (g, bounds);
    }

    void resized() override
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

        // Positioned by an always-advancing cursor rather than by shrinking
        // leftColumn's own rectangle top-down: on the first layout pass (before
        // the self-growing logic below has corrected the component's height),
        // leftColumn can be shorter than everything it needs to hold. A
        // shrinking rectangle silently clamps once it runs out of height --
        // every field from that point on collapses to zero size at the same
        // spot -- which then also hides the bug from the self-growing check,
        // since it measures component bottoms that were themselves clamped.
        // A cursor that just keeps advancing past leftColumn's nominal bottom
        // always reports the true required height, so growth actually happens.
        const int leftX = leftColumn.getX();
        const int leftW = leftColumn.getWidth();
        int y = leftColumn.getY();

        auto layoutField = [&] (juce::Label& label, juce::TextEditor& editor)
        {
            label.setBounds (leftX, y, leftW, 16);
            y += 16;
            editor.setBounds (leftX, y + 2, leftW, 28 - 4);
            y += 28 + 10;
        };

        layoutField (fullNameLabel_,  fullNameEditor_);
        layoutField (emailLabel_,     emailEditor_);
        layoutField (stageNameLabel_, stageNameEditor_);
        layoutField (birthdayLabel_,  birthdayEditor_);
        layoutField (countryLabel_,   countryEditor_);
        layoutField (cityLabel_,      cityEditor_);

        genderLabel_.setBounds (leftX, y, leftW, 16);
        y += 16;
        genderBox_.setBounds (leftX, y, 200, 28);
        y += 28 + 10;

        roleLabel_.setBounds (leftX, y, leftW, 16);
        y += 16;
        roleValueLabel_.setBounds (leftX, y, leftW, 22);
        y += 22 + 14;

        saveButton_.setBounds (leftX, y, 200, 34);
        y += 34;

        //--- Right column: avatar picker grid --------------------------------
        avatarSectionLabel_.setBounds (rightColumn.removeFromTop (20));
        rightColumn.removeFromTop (6);

        const int tileSize = 56;
        const int gap = 8;
        const int cols = juce::jmax (1, (rightColumn.getWidth() + gap) / (tileSize + gap));
        const int rows = avatarTiles_.empty() ? 0
            : (((int) avatarTiles_.size() + cols - 1) / cols);
        const int gridHeight = rows * (tileSize + gap);

        // Height is exactly gridHeight, deliberately NOT jmax'd against
        // rightColumn.getHeight() -- rightColumn's height is itself derived
        // from this component's OWN current getHeight(), so doing that made
        // avatarGridHolder_'s measured bottom track getHeight() almost 1:1.
        // Combined with the self-growing check below, that fed back on
        // itself forever (each pass computing "needed" as a few pixels more
        // than whatever it just grew to) instead of ever converging --
        // a real infinite-recursion crash, not just a layout nitpick.
        avatarGridHolder_.setBounds (rightColumn.withHeight (gridHeight));

        for (int i = 0; i < (int) avatarTiles_.size(); ++i)
        {
            const int row = i / cols;
            const int col = i % cols;
            avatarTiles_[(size_t) i]->setBounds (col * (tileSize + gap), row * (tileSize + gap), tileSize, tileSize);
        }

        // Grow to fit whatever the taller of the two columns actually needed
        // -- this is what lets the viewport's vertical scrollbar reach every
        // field and every avatar tile, on any window size. Two-pass: the
        // second call sees an unchanged width, so this converges immediately
        // rather than looping.
        // Bottom margin below the button matches the panel's left-edge inset
        // (18px, from content.reduced(18, 14) below) so the whitespace reads
        // as even on both sides -- the outer 22px panel border inset is
        // already baked into getHeight(), so add 18 on top of that.
        const int neededHeight = juce::jmax (saveButton_.getBottom(), avatarGridHolder_.getBottom()) + 22 + 18;
        const int neededWidth  = juce::jmax (getWidth(), kMinContentWidth);
        if (neededHeight != getHeight() || neededWidth != getWidth())
        {
            setSize (neededWidth, neededHeight);
            return;
        }
    }

    //--------------------------------------------------------------------------
    void updateAllText()
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

    void loadFromCurrentHost()
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
        // "preset:<id>" (new TAGG) maps to "<id>.png" in the grid; a legacy
        // relative path already ends in "<file>.png"; a custom-photo URL
        // matches nothing here, which correctly leaves every tile unselected.
        const auto selectedFileName = selectedAvatarUrl_.startsWithIgnoreCase ("preset:")
            ? selectedAvatarUrl_.substring (7).trim() + ".png"
            : selectedAvatarUrl_.fromLastOccurrenceOf ("/", false, false);
        for (auto& t : avatarTiles_)
            t->selected = (t->tileIndex >= 0
                           && t->tileIndex < (int) avatarFileNames_.size()
                           && avatarFileNames_[(size_t) t->tileIndex] == selectedFileName);
        avatarGridHolder_.repaint();

        loaded_ = true;
        setStatus ({});
        updateSaveButtonState();
    }

    std::function<void()> onProfileSaved;

private:
    //==========================================================================
    class AvatarTile : public juce::Component
    {
    public:
        int tileIndex = -1;
        juce::Image image;
        bool selected = false;
        std::function<void (int)> onClicked;

        void paint (juce::Graphics& g) override
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

        void mouseUp (const juce::MouseEvent&) override { if (onClicked) onClicked (tileIndex); }
    };

    void buildAvatarGrid()
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

    void selectAvatar (int tileIndex)
    {
        if (tileIndex < 0 || tileIndex >= (int) avatarFileNames_.size())
            return;

        selectedAvatarUrl_ = "assets/icon/" + avatarFileNames_[(size_t) tileIndex];

        for (auto& t : avatarTiles_)
            t->selected = (t->tileIndex == tileIndex);

        avatarGridHolder_.repaint();
        updateSaveButtonState();
    }

    void updateSaveButtonState()
    {
        // Save silently staying disabled with no explanation is exactly
        // what reads as "there's no Save button" -- surface *why* whenever
        // it's disabled, instead of just disabling it.
        juce::String reason;
        if (loaded_ && fullNameEditor_.getText().trim().isEmpty())
            reason = "Enter your full name to save.";
        else if (loaded_ && ! looksLikeValidEmail (emailEditor_.getText().trim()))
            reason = "Enter a valid email address to save.";

        saveButton_.setEnabled (loaded_ && reason.isEmpty());

        if (reason.isNotEmpty())
            setStatus (reason, true);
    }

    void onSaveClicked()
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

        juce::Component::SafePointer<ContentPanel> safe (this);
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

                // Keep the in-memory cache (read by TopBar, QueueBar host
                // pin, etc.) in sync with what was just written.
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

    void setStatus (const juce::String& message, bool isError = false)
    {
        statusLabel_.setColour (juce::Label::textColourId, isError ? kDanger : kMuted);
        statusLabel_.setText (message, juce::dontSendNotification);
    }

    //==========================================================================
    juce::Label title_, subtitle_, statusLabel_;

    juce::Label fullNameLabel_;
    juce::TextEditor fullNameEditor_;
    juce::Label emailLabel_;
    juce::TextEditor emailEditor_;
    juce::Label stageNameLabel_;
    juce::TextEditor stageNameEditor_;
    juce::Label birthdayLabel_;
    juce::TextEditor birthdayEditor_;
    juce::Label countryLabel_;
    juce::TextEditor countryEditor_;
    juce::Label cityLabel_;
    juce::TextEditor cityEditor_;
    juce::Label genderLabel_;
    juce::ComboBox genderBox_;
    juce::Label roleLabel_;
    juce::Label roleValueLabel_;

    juce::Label avatarSectionLabel_;
    juce::Component avatarGridHolder_;
    std::vector<juce::String> avatarFileNames_;
    std::vector<std::unique_ptr<AvatarTile>> avatarTiles_;
    juce::String selectedAvatarUrl_;   // relative asset path, e.g. "assets/images/avatars/1064391.png"

    juce::TextButton saveButton_ { "Save Profile" };

    juce::String currentUid_;
    bool loaded_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentPanel)
};

//==============================================================================
ProfilePage::ProfilePage()
    : content_ (std::make_unique<ContentPanel>())
{
    setOpaque (true);
    addAndMakeVisible (viewport_);
    viewport_.setViewedComponent (content_.get(), false);
    // Horizontal too: content_ is floored at kMinContentWidth (see
    // resized()), which can exceed a narrow viewport and clip content with
    // no way to reach it otherwise.
    viewport_.setScrollBarsShown (true, true);

    content_->onProfileSaved = [this] { if (onProfileSaved) onProfileSaved(); };
}

ProfilePage::~ProfilePage() = default;

void ProfilePage::paint (juce::Graphics& g)
{
    MenuTheme::drawPageBackground (g, getLocalBounds());
}

void ProfilePage::resized()
{
    viewport_.setBounds (getLocalBounds());

    // Give the content its natural width first; ContentPanel::resized()
    // grows itself (both width and height) to whatever it actually needs,
    // so this is just a starting point, not a hard constraint. The height
    // floor avoids ever handing ContentPanel a 0-tall first layout pass
    // (before its own self-correction has run once) -- degenerate
    // negative-size rectangles during that pass were the likely cause of
    // the crash this replaced.
    const int startingWidth  = juce::jmax (kMinContentWidth, viewport_.getWidth() - viewport_.getScrollBarThickness());
    const int startingHeight = juce::jmax (content_->getHeight(), 600);
    if (content_->getWidth() != startingWidth || content_->getHeight() != startingHeight)
        content_->setSize (startingWidth, startingHeight);
}

void ProfilePage::updateAllText()          { content_->updateAllText(); }
void ProfilePage::loadFromCurrentHost()    { content_->loadFromCurrentHost(); }
