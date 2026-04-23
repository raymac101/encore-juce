/*
  ==============================================================================

    SettingsPage.cpp
    Created: 22 Apr 2026
    Author:  GitHub Copilot

  ==============================================================================
*/

#include "SettingsPage.h"

//==============================================================================
// Layout constants
//==============================================================================
namespace
{
    constexpr int kPadX       = 24;
    constexpr int kRowH       = 36;
    constexpr int kSectionH   = 40;
    constexpr int kFieldGap   = 6;
    constexpr int kSectionGap = 20;
    constexpr int kLabelW     = 260;
    constexpr int kCtrlX      = kPadX + kLabelW + 16;
    constexpr int kComboW     = 210;
    constexpr int kToggleW    = 40;
    constexpr int kCardOuterPad = 8;   // gap from panel edge to card edge
    constexpr int kCardRadius   = 10;

    constexpr uint32_t kBg          = 0xff16213e;
    constexpr uint32_t kSectionBg   = 0xff1e2d5a;
    constexpr uint32_t kCardFill    = 0xff1a2030;   // card background (alpha applied at paint)
    constexpr uint32_t kCardBorder  = 0xff2d3a5a;   // card border
    constexpr uint32_t kAccent      = 0xff7b5ea7;
    constexpr uint32_t kAccentSoft  = 0xff9d7fc9;   // lighter accent for headers
    constexpr uint32_t kBtnNormal   = 0xff2d2d3a;
    constexpr uint32_t kBtnDanger   = 0xff7b2d2d;
    constexpr uint32_t kBtnSuccess  = 0xff2d6b3a;
    constexpr uint32_t kTextPrimary = 0xffe4e4e4;
    constexpr uint32_t kTextSecond  = 0xffa3a6a8;
    constexpr uint32_t kSaved       = 0xff4caf50;
    constexpr uint32_t kTagActive   = 0xff2d8a3e;
    constexpr uint32_t kTagAdmin    = 0xffb03030;
    constexpr uint32_t kTagHost     = 0xff1a6a9a;
}

//==============================================================================
// UserRowComponent
//==============================================================================
class UserRowComponent : public juce::Component
{
public:
    std::function<void(const juce::String& role)>  onRoleChanged;
    std::function<void()>                           onDeactivate;
    std::function<void()>                           onRemove;

    explicit UserRowComponent(const SettingsPage::VenueUser& user)
    {
        nameLabel_.setText(user.email, juce::dontSendNotification);
        nameLabel_.setFont(juce::Font(juce::FontOptions().withHeight(13.f)).boldened());
        nameLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
        addAndMakeVisible(nameLabel_);

        emailLabel_.setText(user.email, juce::dontSendNotification);
        emailLabel_.setFont(juce::Font(juce::FontOptions().withHeight(12.f)));
        emailLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextSecond));
        addAndMakeVisible(emailLabel_);

        statusBadge_.setText(user.active ? "ACTIVE" : "INACTIVE", juce::dontSendNotification);
        statusBadge_.setFont(juce::Font(juce::FontOptions().withHeight(10.f)).boldened());
        statusBadge_.setColour(juce::Label::textColourId,       juce::Colours::white);
        statusBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(user.active ? kTagActive : kBtnNormal));
        statusBadge_.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(statusBadge_);

        auto roleCol = (user.role == "Admin") ? kTagAdmin : (user.role == "Host") ? kTagHost : kBtnNormal;
        roleBadge_.setText(user.role.toUpperCase(), juce::dontSendNotification);
        roleBadge_.setFont(juce::Font(juce::FontOptions().withHeight(10.f)).boldened());
        roleBadge_.setColour(juce::Label::textColourId,       juce::Colours::white);
        roleBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(roleCol));
        roleBadge_.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(roleBadge_);

        roleCombo_.addItem("Basic", 1);
        roleCombo_.addItem("Host",  2);
        roleCombo_.addItem("Admin", 3);
        if (user.role == "Basic")      roleCombo_.setSelectedId(1, juce::dontSendNotification);
        else if (user.role == "Host")  roleCombo_.setSelectedId(2, juce::dontSendNotification);
        else if (user.role == "Admin") roleCombo_.setSelectedId(3, juce::dontSendNotification);
        roleCombo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d1527));
        roleCombo_.setColour(juce::ComboBox::textColourId,       juce::Colour(kTextPrimary));
        roleCombo_.setColour(juce::ComboBox::outlineColourId,    juce::Colour(kAccent).withAlpha(0.4f));
        roleCombo_.onChange = [this]() {
            if (onRoleChanged) {
                juce::StringArray roles = { "Basic", "Host", "Admin" };
                onRoleChanged(roles[roleCombo_.getSelectedId() - 1]);
            }
        };
        addAndMakeVisible(roleCombo_);

        auto makeBtn = [this](juce::TextButton& btn, const juce::String& text, uint32_t col) {
            btn.setButtonText(text);
            btn.setColour(juce::TextButton::buttonColourId,  juce::Colour(col));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            addAndMakeVisible(btn);
        };
        makeBtn(btnDeactivate_, "DEACTIVATE", kBtnNormal);
        makeBtn(btnRemove_,     "REMOVE",     kBtnDanger);
        btnDeactivate_.onClick = [this]() { if (onDeactivate) onDeactivate(); };
        btnRemove_.onClick     = [this]() { if (onRemove)     onRemove(); };
    }

    void resized() override
    {
        const int w   = getWidth();
        nameLabel_.setBounds(kPadX, 4, 260, 18);
        emailLabel_.setBounds(kPadX, 22, 260, 16);
        statusBadge_.setBounds(kPadX, 42, 52, 16);
        roleBadge_.setBounds(kPadX + 58, 42, 52, 16);

        const int btnW = 100;
        const int gap  = 8;
        int rx = w - kPadX - btnW;
        btnRemove_.setBounds(rx, 16, btnW, 28);     rx -= gap + btnW;
        btnDeactivate_.setBounds(rx, 16, btnW, 28); rx -= gap + 90;
        roleCombo_.setBounds(rx, 16, 90, 28);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(kTextSecond).withAlpha(0.2f));
        g.drawLine(0, (float)getHeight() - 1, (float)getWidth(), (float)getHeight() - 1);
    }

private:
    juce::Label      nameLabel_, emailLabel_, statusBadge_, roleBadge_;
    juce::ComboBox   roleCombo_;
    juce::TextButton btnDeactivate_, btnRemove_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UserRowComponent)
};

//==============================================================================
// SettingsContentPanel
//==============================================================================
class SettingsContentPanel : public juce::Component
{
public:
    explicit SettingsContentPanel(SettingsPage& owner)
        : owner_(owner)
    {
        auto& lm = LocalizationManager::getInstance();

        // Title + top action buttons
        initSectionLabel(titleLabel_, lm.getText("settings.title"));
        titleLabel_.setFont(juce::Font(juce::FontOptions().withHeight(26.f)).boldened());
        titleLabel_.setColour(juce::Label::textColourId,       juce::Colours::white);
        titleLabel_.setColour(juce::Label::backgroundColourId, juce::Colour(0));

        initButton(btnEditVenue_,   lm.getText("settings.btn_edit_venue"),   kBtnNormal);
        initButton(btnDeleteVenue_, lm.getText("settings.btn_delete_venue"), kBtnDanger);
        btnEditVenue_.onClick   = [this]() { setVenueEditMode(true); };
        btnDeleteVenue_.onClick = [this]() { onDeleteVenue(); };

        // ── Section 1: Venue Information ─────────────────────────────────────
        initSectionLabel(secVenueInfo_, lm.getText("settings.sec_venue_info"));
        initFieldLabel(lblVenueName_, lm.getText("settings.lbl_venue_name"));
        initFieldLabel(lblAddress_,   lm.getText("settings.lbl_address"));
        initFieldLabel(lblCity_,      lm.getText("settings.lbl_city"));
        initFieldLabel(lblCountry_,   lm.getText("settings.lbl_country"));
        initValueLabel(valVenueName_);
        initValueLabel(valAddress_);
        initValueLabel(valCity_);
        initValueLabel(valCountry_);
        initEditor(edVenueName_, lm.getText("settings.ph_venue_name"));
        initEditor(edAddress_,   lm.getText("settings.ph_address"));
        initEditor(edCity_,      lm.getText("settings.ph_city"));
        initEditor(edCountry_,   lm.getText("settings.ph_country"));
        edVenueName_.setVisible(false);
        edAddress_.setVisible(false);
        edCity_.setVisible(false);
        edCountry_.setVisible(false);
        initButton(btnSaveVenue_,   lm.getText("settings.btn_save_venue"), kBtnNormal);
        initButton(btnCancelVenue_, lm.getText("settings.btn_cancel"),     kBtnNormal);
        btnSaveVenue_.setVisible(false);
        btnCancelVenue_.setVisible(false);
        btnSaveVenue_.onClick   = [this]() { onSaveVenueInfo(); };
        btnCancelVenue_.onClick = [this]() { setVenueEditMode(false); };
        savedLabel_.setFont(juce::Font(juce::FontOptions().withHeight(12.f)));
        savedLabel_.setColour(juce::Label::textColourId, juce::Colour(kSaved));
        savedLabel_.setJustificationType(juce::Justification::centredLeft);
        savedLabel_.setVisible(false);
        addAndMakeVisible(savedLabel_);

        // License key + Venue ID (read-only)
        initFieldLabel(lblLicenseKey_, lm.getText("settings.lbl_license_key"));
        initFieldLabel(lblVenueIdLbl_, lm.getText("settings.lbl_venue_id"));
        initValueLabel(valLicenseKey_);
        initValueLabel(valVenueId_);

        // ── Section 2: Venue Code Management ─────────────────────────────────
        initSectionLabel(secVenueCode_, lm.getText("settings.sec_venue_code"));
        initFieldLabel(lblCurrentCode_, lm.getText("settings.lbl_current_code"));
        initValueLabel(valCurrentCode_);
        valCurrentCode_.setFont(juce::Font(juce::FontOptions().withHeight(18.f)).boldened());
        valCurrentCode_.setColour(juce::Label::textColourId,       juce::Colours::white);
        valCurrentCode_.setColour(juce::Label::backgroundColourId, juce::Colour(kAccent).withAlpha(0.85f));
        valCurrentCode_.setJustificationType(juce::Justification::centred);
        initFieldLabel(lblManualCode_, lm.getText("settings.lbl_manual_code"));
        initEditor(edManualCode_,      lm.getText("settings.ph_manual_code"));
        initButton(btnSetCode_, lm.getText("settings.btn_set_code"), kBtnNormal);
        initButton(btnGenCode_, lm.getText("settings.btn_gen_code"), kBtnSuccess);
        btnSetCode_.onClick = [this]() {
            auto code = edManualCode_.getText().trim().toUpperCase();
            if (code.length() != 6) {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Invalid Code", "Please enter exactly 6 letters.");
                return;
            }
            if (owner_.onSetVenueCode) owner_.onSetVenueCode(code);
        };
        btnGenCode_.onClick = [this]() {
            if (owner_.onGenerateVenueCode) owner_.onGenerateVenueCode();
        };

        // Emergency code sub-section
        initFieldLabel(lblEmergHeader_, lm.getText("settings.lbl_emerg_header"));
        lblEmergHeader_.setFont(juce::Font(juce::FontOptions().withHeight(14.f)).boldened());
        lblEmergHeader_.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
        initFieldLabel(lblCurrentEmerg_, lm.getText("settings.lbl_current_emerg"));
        initValueLabel(valCurrentEmerg_);
        initEditor(edEmergCode_, lm.getText("settings.ph_emerg_code"));
        initButton(btnSetEmerg_, lm.getText("settings.btn_set_emerg"), kBtnNormal);
        initButton(btnGenEmerg_, lm.getText("settings.btn_gen_emerg"), kBtnSuccess);
        btnSetEmerg_.onClick = [this]() {
            auto code = edEmergCode_.getText().trim().toUpperCase();
            if (code.length() != 6) {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Invalid Code", "Please enter exactly 6 letters.");
                return;
            }
            if (owner_.onSetEmergencyCode) owner_.onSetEmergencyCode(code);
        };
        btnGenEmerg_.onClick = [this]() {
            if (owner_.onGenerateEmergencyCode) owner_.onGenerateEmergencyCode();
        };

        // ── Section 3: User Management ────────────────────────────────────────
        initSectionLabel(secUsers_, lm.getText("settings.sec_users"));
        initFieldLabel(lblInviteHeader_, lm.getText("settings.lbl_invite_header"));
        lblInviteHeader_.setFont(juce::Font(juce::FontOptions().withHeight(14.f)).boldened());
        lblInviteHeader_.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
        initEditor(edInviteEmail_, lm.getText("settings.ph_invite_email"));
        cbInviteRole_.addItem("Basic User", 1);
        cbInviteRole_.addItem("Host",       2);
        cbInviteRole_.setSelectedId(1, juce::dontSendNotification);
        cbInviteRole_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d1527));
        cbInviteRole_.setColour(juce::ComboBox::textColourId,       juce::Colour(kTextPrimary));
        cbInviteRole_.setColour(juce::ComboBox::outlineColourId,    juce::Colour(kAccent).withAlpha(0.4f));
        addAndMakeVisible(cbInviteRole_);
        initButton(btnInviteUser_, lm.getText("settings.btn_invite_user"), kBtnNormal);
        btnInviteUser_.onClick = [this]() { onInviteUser(); };
        initFieldLabel(lblCurrentUsersHeader_, lm.getText("settings.lbl_current_users"));
        lblCurrentUsersHeader_.setFont(juce::Font(juce::FontOptions().withHeight(14.f)).boldened());
        lblCurrentUsersHeader_.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
        userListPanel_ = std::make_unique<juce::Component>();
        addAndMakeVisible(*userListPanel_);

        // ── Section 4: Logo Management ────────────────────────────────────────
        initSectionLabel(secLogo_, lm.getText("settings.sec_logo"));
        initFieldLabel(lblLogo_, lm.getText("settings.lbl_logo"));
        logoPathLabel_.setText(lm.getText("settings.ph_logo_file"), juce::dontSendNotification);
        logoPathLabel_.setFont(juce::Font(juce::FontOptions().withHeight(12.f)));
        logoPathLabel_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0d1527));
        logoPathLabel_.setColour(juce::Label::textColourId,       juce::Colour(kTextSecond));
        logoPathLabel_.setJustificationType(juce::Justification::centredLeft);
        logoPathLabel_.setBorderSize(juce::BorderSize<int>(0, 8, 0, 0));
        addAndMakeVisible(logoPathLabel_);
        initButton(btnBrowseLogo_,  lm.getText("settings.btn_browse"),       kBtnNormal);
        initButton(btnSaveLogo_,    lm.getText("settings.btn_save_logo"),    kBtnNormal);
        initButton(btnDefaultLogo_, lm.getText("settings.btn_default_logo"), kBtnNormal);
        btnBrowseLogo_.onClick = [this]() {
            fc_ = std::make_unique<juce::FileChooser>(
                "Select Logo Image",
                juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                "*.png;*.jpg;*.jpeg;*.gif");
            fc_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& chooser) {
                    auto result = chooser.getResult();
                    if (result.existsAsFile()) {
                        selectedLogoFile_ = result;
                        logoPathLabel_.setText(result.getFileName(), juce::dontSendNotification);
                        logoPathLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
                    }
                });
        };
        btnSaveLogo_.onClick = [this]() {
            if (selectedLogoFile_.existsAsFile() && owner_.onUploadLogo)
                owner_.onUploadLogo(selectedLogoFile_);
        };
        btnDefaultLogo_.onClick = [this]() {
            if (owner_.onResetLogo) owner_.onResetLogo();
        };

        // ── Section 5: Queue / Display Settings ──────────────────────────────
        initSectionLabel(secQueue_, lm.getText("settings.sec_queue"));
        initFieldLabel(lblLyricsBg_,        lm.getText("settings.lbl_lyrics_bg"));
        initFieldLabel(lblNumSongs_,         lm.getText("settings.lbl_num_songs"));
        initFieldLabel(lblNumSingers_,       lm.getText("settings.lbl_num_singers"));
        initFieldLabel(lblNumSkips_,         lm.getText("settings.lbl_num_skips"));
        initFieldLabel(lblRepeat_,           lm.getText("settings.lbl_repeat_songs"));
        initFieldLabel(lblAutoApprove_,      lm.getText("settings.lbl_auto_approve"));
        initFieldLabel(lblShowOnline_,       lm.getText("settings.lbl_show_online"));
        initFieldLabel(lblShowOnlineEncore_, lm.getText("settings.lbl_show_online_encore"));
        initFieldLabel(lblShowMemory_,       lm.getText("settings.lbl_show_memory"));

        initCombo(cbLyricsBg_);
        cbLyricsBg_.addItem(lm.getText("settings.bg_none"),     1);
        cbLyricsBg_.addItem(lm.getText("settings.bg_squares"),  2);
        cbLyricsBg_.addItem(lm.getText("settings.bg_lines"),    3);
        cbLyricsBg_.addItem(lm.getText("settings.bg_fading"),   4);
        cbLyricsBg_.addItem(lm.getText("settings.bg_circles"),  5);
        cbLyricsBg_.addItem(lm.getText("settings.bg_rotating"), 6);
        cbLyricsBg_.addItem(lm.getText("settings.bg_snow"),     7);
        cbLyricsBg_.onChange = [this]() {
            owner_.venue_.background = cbLyricsBg_.getSelectedId() - 1;
            owner_.notifyChanged();
        };

        initCombo(cbNumSongs_);
        cbNumSongs_.addItem(lm.getText("settings.opt_one"),   1);
        cbNumSongs_.addItem(lm.getText("settings.opt_two"),   2);
        cbNumSongs_.addItem(lm.getText("settings.opt_three"), 3);
        cbNumSongs_.addItem(lm.getText("settings.opt_four"),  4);
        cbNumSongs_.addItem(lm.getText("settings.opt_five"),  5);
        cbNumSongs_.addItem(lm.getText("settings.opt_ten"),   6);
        cbNumSongs_.addItem(lm.getText("settings.opt_any"),   7);
        cbNumSongs_.onChange = [this]() {
            owner_.venue_.numSongs = cbNumSongs_.getSelectedId();
            owner_.notifyChanged();
        };

        initCombo(cbNumSingers_);
        cbNumSingers_.addItem(lm.getText("settings.opt_hide"),  1);
        cbNumSingers_.addItem(lm.getText("settings.opt_next"),  2);
        cbNumSingers_.addItem(lm.getText("settings.opt_two"),   3);
        cbNumSingers_.addItem(lm.getText("settings.opt_three"), 4);
        cbNumSingers_.addItem(lm.getText("settings.opt_four"),  5);
        cbNumSingers_.addItem(lm.getText("settings.opt_five"),  6);
        cbNumSingers_.addItem(lm.getText("settings.opt_six"),   7);
        cbNumSingers_.addItem(lm.getText("settings.opt_all"),   8);
        cbNumSingers_.onChange = [this]() {
            owner_.venue_.numSingers = cbNumSingers_.getSelectedId() - 1;
            owner_.notifyChanged();
        };

        initCombo(cbNumSkips_);
        cbNumSkips_.addItem(lm.getText("settings.opt_zero"),  1);
        cbNumSkips_.addItem(lm.getText("settings.opt_one"),   2);
        cbNumSkips_.addItem(lm.getText("settings.opt_two"),   3);
        cbNumSkips_.addItem(lm.getText("settings.opt_three"), 4);
        cbNumSkips_.onChange = [this]() {
            owner_.venue_.numStrikes = cbNumSkips_.getSelectedId() - 1;
            owner_.notifyChanged();
        };

        initToggle(tbRepeat_);
        tbRepeat_.onStateChange = [this]() {
            owner_.venue_.repeatSongs = tbRepeat_.getToggleState();
            owner_.notifyChanged();
        };
        initToggle(tbAutoApprove_);
        tbAutoApprove_.onStateChange = [this]() {
            owner_.venue_.autoapprove = tbAutoApprove_.getToggleState();
            owner_.notifyChanged();
        };
        initToggle(tbShowOnline_);
        tbShowOnline_.onStateChange = [this]() {
            owner_.venue_.showOnlineSongs = tbShowOnline_.getToggleState();
            owner_.notifyChanged();
        };
        initToggle(tbShowOnlineEncore_);
        tbShowOnlineEncore_.onStateChange = [this]() {
            owner_.venue_.showOnlineSongsEncore = tbShowOnlineEncore_.getToggleState();
            owner_.notifyChanged();
        };
        initToggle(tbShowMemory_);
        tbShowMemory_.onStateChange = [this]() {
            owner_.venue_.showMemoryStats = tbShowMemory_.getToggleState();
            owner_.notifyChanged();
        };

        // ── Section 6: Session Management ────────────────────────────────────
        initSectionLabel(secSession_, lm.getText("settings.sec_session"));
        auto makeStatRow = [this](juce::Label& lbl, juce::Label& val, const juce::String& text) {
            initFieldLabel(lbl, text);
            val.setFont(juce::Font(juce::FontOptions().withHeight(14.f)).boldened());
            val.setColour(juce::Label::textColourId, juce::Colour(kAccent));
            val.setJustificationType(juce::Justification::centredRight);
            val.setText("0", juce::dontSendNotification);
            addAndMakeVisible(val);
        };
        makeStatRow(lblSongsToday_,    valSongsToday_,    lm.getText("settings.lbl_songs_today"));
        makeStatRow(lblActiveMembers_, valActiveMembers_, lm.getText("settings.lbl_active_members"));
        makeStatRow(lblSingersQueue_,  valSingersQueue_,  lm.getText("settings.lbl_singers_queue"));
        makeStatRow(lblReqSongs_,      valReqSongs_,      lm.getText("settings.lbl_req_songs"));

        initButton(btnClearRecent_, lm.getText("settings.btn_clear_recent"), kBtnNormal);
        initButton(btnEndSession_,  lm.getText("settings.btn_end_session"),  kBtnDanger);
        initButton(btnViewArchive_, lm.getText("settings.btn_view_archive"), kBtnNormal);
        btnClearRecent_.onClick = [this]() { onClearRecent(); };
        btnEndSession_.onClick  = [this]() { onEndSession(); };
        btnViewArchive_.onClick = [this]() {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                "Archive History", "Archive history viewer coming soon.");
        };
    }

    //--------------------------------------------------------------------------
    void paint(juce::Graphics& g) override
    {
        // Draw a rounded translucent "card" behind each section, plus a left
        // accent stripe and a subtle outline. This gives the page a modern,
        // visually-grouped feel while still letting the parent tiled
        // background show through around the cards.
        for (const auto& r : cardRects_)
        {
            auto rf = r.toFloat();

            // Soft drop shadow
            juce::DropShadow shadow(juce::Colours::black.withAlpha(0.35f), 10, {0, 2});
            juce::Path shadowPath;
            shadowPath.addRoundedRectangle(rf, (float)kCardRadius);
            shadow.drawForPath(g, shadowPath);

            // Card fill
            g.setColour(juce::Colour(kCardFill).withAlpha(0.82f));
            g.fillRoundedRectangle(rf, (float)kCardRadius);

            // Border
            g.setColour(juce::Colour(kCardBorder).withAlpha(0.8f));
            g.drawRoundedRectangle(rf.reduced(0.5f), (float)kCardRadius, 1.f);

            // Left accent stripe
            auto stripe = rf.withWidth(3.f).reduced(0.f, 1.f);
            g.setColour(juce::Colour(kAccentSoft));
            g.fillRoundedRectangle(stripe, 2.f);
        }

        // Page-title accent underline (gradient from accent to transparent)
        if (titleLabel_.getWidth() > 0)
        {
            auto tb = titleLabel_.getBounds();
            int uy = tb.getBottom() + 2;
            juce::ColourGradient grad(juce::Colour(kAccentSoft), (float)tb.getX(), (float)uy,
                                      juce::Colour(kAccentSoft).withAlpha(0.f),
                                      (float)tb.getX() + 220.f, (float)uy, false);
            g.setGradientFill(grad);
            g.fillRect(tb.getX(), uy, 220, 2);
        }
    }

    //--------------------------------------------------------------------------
    void resized() override
    {
        const int w = getWidth();
        int y = 20;

        cardRects_.clear();
        auto cardStart = [&]() { return y - 8; };
        auto cardEnd   = [&](int cs) {
            cardRects_.push_back(juce::Rectangle<int>(kCardOuterPad,
                                                      cs,
                                                      w - kCardOuterPad * 2,
                                                      (y + 4) - cs));
        };

        auto comboRow = [&](juce::Label& lbl, juce::ComboBox& cb) {
            lbl.setBounds(kPadX, y, kLabelW, kRowH);
            cb.setBounds(kCtrlX, y, kComboW, kRowH);
            y += kRowH + kFieldGap;
        };
        auto toggleRow = [&](juce::Label& lbl, juce::ToggleButton& tb) {
            lbl.setBounds(kPadX, y, kLabelW, kRowH);
            tb.setBounds(kCtrlX, y, kToggleW, kRowH);
            y += kRowH + kFieldGap;
        };
        auto readRow = [&](juce::Label& lbl, juce::Label& val) {
            lbl.setBounds(kPadX, y, kLabelW, kRowH);
            val.setBounds(kCtrlX, y, w - kCtrlX - kPadX, kRowH);
            y += kRowH + kFieldGap;
        };
        auto statRow = [&](juce::Label& lbl, juce::Label& val) {
            lbl.setBounds(kPadX, y, w - kPadX * 2 - 60, kRowH);
            val.setBounds(w - kPadX - 60, y, 60, kRowH);
            y += kRowH + kFieldGap;
        };

        // Title + top buttons
        titleLabel_.setBounds(kPadX, y, w - kPadX * 2, 36);
        y += 36 + 10;
        btnEditVenue_.setBounds(kPadX, y, 160, 34);
        btnDeleteVenue_.setBounds(kPadX + 168, y, 140, 34);
        y += 34 + kSectionGap;

        // Section 1: Venue Info
        int cs = cardStart();
        secVenueInfo_.setBounds(0, y, w, kSectionH); y += kSectionH + kFieldGap;
        if (!venueEditMode_)
        {
            readRow(lblVenueName_, valVenueName_);
            readRow(lblAddress_,   valAddress_);
            readRow(lblCity_,      valCity_);
            readRow(lblCountry_,   valCountry_);
        }
        else
        {
            juce::Label* lbls[] = { &lblVenueName_, &lblAddress_, &lblCity_, &lblCountry_ };
            juce::Component* eds[] = { &edVenueName_, &edAddress_, &edCity_, &edCountry_ };
            for (int i = 0; i < 4; ++i) {
                lbls[i]->setBounds(kPadX, y, kLabelW, kRowH);
                eds[i]->setBounds(kCtrlX, y, w - kCtrlX - kPadX, kRowH);
                y += kRowH + kFieldGap;
            }
            int bx = w - kPadX - 140;
            btnCancelVenue_.setBounds(bx, y, 130, kRowH);
            btnSaveVenue_.setBounds(bx - 148, y, 140, kRowH);
            savedLabel_.setBounds(kPadX, y, 200, kRowH);
            y += kRowH + kFieldGap;
        }
        y += 4;
        readRow(lblLicenseKey_, valLicenseKey_);
        readRow(lblVenueIdLbl_, valVenueId_);
        cardEnd(cs);
        y += kSectionGap;

        // Section 2: Venue Code Management
        cs = cardStart();
        secVenueCode_.setBounds(0, y, w, kSectionH); y += kSectionH + kFieldGap;
        lblCurrentCode_.setBounds(kPadX, y, kLabelW, kRowH);
        valCurrentCode_.setBounds(kCtrlX, y, 120, kRowH);
        y += kRowH + kFieldGap;
        lblManualCode_.setBounds(kPadX, y, kLabelW, kRowH);
        edManualCode_.setBounds(kCtrlX, y, 130, kRowH);
        btnSetCode_.setBounds(kCtrlX + 138, y, 100, kRowH);
        y += kRowH + kFieldGap;
        btnGenCode_.setBounds(kPadX, y, 220, kRowH);
        y += kRowH + kSectionGap;

        lblEmergHeader_.setBounds(kPadX, y, w - kPadX * 2, 26); y += 30;
        lblCurrentEmerg_.setBounds(kPadX, y, kLabelW, kRowH);
        valCurrentEmerg_.setBounds(kCtrlX, y, 160, kRowH);
        y += kRowH + kFieldGap;
        edEmergCode_.setBounds(kPadX, y, 180, kRowH);
        btnSetEmerg_.setBounds(kPadX + 188, y, 100, kRowH);
        y += kRowH + kFieldGap;
        btnGenEmerg_.setBounds(kPadX, y, 260, kRowH);
        y += kRowH;
        cardEnd(cs);
        y += kSectionGap;

        // Section 3: User Management
        cs = cardStart();
        secUsers_.setBounds(0, y, w, kSectionH); y += kSectionH + kFieldGap;
        lblInviteHeader_.setBounds(kPadX, y, w - kPadX * 2, 26); y += 30;
        edInviteEmail_.setBounds(kPadX, y, w - kPadX * 2 - 280, kRowH);
        cbInviteRole_.setBounds(w - kPadX - 260, y, 120, kRowH);
        btnInviteUser_.setBounds(w - kPadX - 132, y, 132, kRowH);
        y += kRowH + kSectionGap;
        lblCurrentUsersHeader_.setBounds(kPadX, y, w - kPadX * 2, 26); y += 30;
        if (userListPanel_)
        {
            const int userRowH = 68;
            int numRows = (int)userRows_.size();
            userListPanel_->setBounds(0, y, w, userRowH * juce::jmax(1, numRows));
            for (int i = 0; i < numRows; ++i)
                userRows_[(size_t)i]->setBounds(0, i * userRowH, w, userRowH);
            y += userListPanel_->getHeight();
        }
        cardEnd(cs);
        y += kSectionGap;

        // Section 4: Logo
        cs = cardStart();
        secLogo_.setBounds(0, y, w, kSectionH); y += kSectionH + kFieldGap;
        lblLogo_.setBounds(kPadX, y, kLabelW, kRowH);
        logoPathLabel_.setBounds(kCtrlX, y, w - kCtrlX - kPadX - 90, kRowH);
        btnBrowseLogo_.setBounds(w - kPadX - 82, y, 82, kRowH);
        y += kRowH + kFieldGap;
        btnSaveLogo_.setBounds(kPadX, y, 140, kRowH);
        btnDefaultLogo_.setBounds(kPadX + 148, y, 150, kRowH);
        y += kRowH;
        cardEnd(cs);
        y += kSectionGap;

        // Section 5: Queue / Display
        cs = cardStart();
        secQueue_.setBounds(0, y, w, kSectionH); y += kSectionH + kFieldGap;
        comboRow(lblLyricsBg_,        cbLyricsBg_);
        comboRow(lblNumSongs_,         cbNumSongs_);
        comboRow(lblNumSingers_,       cbNumSingers_);
        comboRow(lblNumSkips_,         cbNumSkips_);
        toggleRow(lblRepeat_,           tbRepeat_);
        toggleRow(lblAutoApprove_,      tbAutoApprove_);
        toggleRow(lblShowOnline_,       tbShowOnline_);
        toggleRow(lblShowOnlineEncore_, tbShowOnlineEncore_);
        toggleRow(lblShowMemory_,       tbShowMemory_);
        cardEnd(cs);
        y += kSectionGap;

        // Section 6: Session Management
        cs = cardStart();
        secSession_.setBounds(0, y, w, kSectionH); y += kSectionH + kFieldGap;
        statRow(lblSongsToday_,    valSongsToday_);
        statRow(lblActiveMembers_, valActiveMembers_);
        statRow(lblSingersQueue_,  valSingersQueue_);
        statRow(lblReqSongs_,      valReqSongs_);
        y += kFieldGap;
        btnClearRecent_.setBounds(kPadX,       y, 200, kRowH);
        btnEndSession_.setBounds(kPadX + 208,  y, 200, kRowH);
        btnViewArchive_.setBounds(kPadX + 416, y, 200, kRowH);
        y += kRowH;
        cardEnd(cs);
        y += kSectionGap + 8;

        setSize(w, y);
        repaint();
    }

    //--------------------------------------------------------------------------
    void updateAllText()
    {
        auto& lm = LocalizationManager::getInstance();

        titleLabel_.setText(lm.getText("settings.title"),             juce::dontSendNotification);
        btnEditVenue_.setButtonText(lm.getText("settings.btn_edit_venue"));
        btnDeleteVenue_.setButtonText(lm.getText("settings.btn_delete_venue"));

        secVenueInfo_.setText(lm.getText("settings.sec_venue_info"),   juce::dontSendNotification);
        lblVenueName_.setText(lm.getText("settings.lbl_venue_name"),   juce::dontSendNotification);
        lblAddress_.setText(lm.getText("settings.lbl_address"),        juce::dontSendNotification);
        lblCity_.setText(lm.getText("settings.lbl_city"),              juce::dontSendNotification);
        lblCountry_.setText(lm.getText("settings.lbl_country"),        juce::dontSendNotification);
        btnSaveVenue_.setButtonText(lm.getText("settings.btn_save_venue"));
        btnCancelVenue_.setButtonText(lm.getText("settings.btn_cancel"));
        lblLicenseKey_.setText(lm.getText("settings.lbl_license_key"), juce::dontSendNotification);
        lblVenueIdLbl_.setText(lm.getText("settings.lbl_venue_id"),    juce::dontSendNotification);

        secVenueCode_.setText(lm.getText("settings.sec_venue_code"),    juce::dontSendNotification);
        lblCurrentCode_.setText(lm.getText("settings.lbl_current_code"), juce::dontSendNotification);
        lblManualCode_.setText(lm.getText("settings.lbl_manual_code"),   juce::dontSendNotification);
        btnSetCode_.setButtonText(lm.getText("settings.btn_set_code"));
        btnGenCode_.setButtonText(lm.getText("settings.btn_gen_code"));
        lblEmergHeader_.setText(lm.getText("settings.lbl_emerg_header"),   juce::dontSendNotification);
        lblCurrentEmerg_.setText(lm.getText("settings.lbl_current_emerg"), juce::dontSendNotification);
        btnSetEmerg_.setButtonText(lm.getText("settings.btn_set_emerg"));
        btnGenEmerg_.setButtonText(lm.getText("settings.btn_gen_emerg"));

        secUsers_.setText(lm.getText("settings.sec_users"),                     juce::dontSendNotification);
        lblInviteHeader_.setText(lm.getText("settings.lbl_invite_header"),      juce::dontSendNotification);
        btnInviteUser_.setButtonText(lm.getText("settings.btn_invite_user"));
        lblCurrentUsersHeader_.setText(lm.getText("settings.lbl_current_users"), juce::dontSendNotification);

        secLogo_.setText(lm.getText("settings.sec_logo"),                  juce::dontSendNotification);
        lblLogo_.setText(lm.getText("settings.lbl_logo"),                  juce::dontSendNotification);
        btnBrowseLogo_.setButtonText(lm.getText("settings.btn_browse"));
        btnSaveLogo_.setButtonText(lm.getText("settings.btn_save_logo"));
        btnDefaultLogo_.setButtonText(lm.getText("settings.btn_default_logo"));

        secQueue_.setText(lm.getText("settings.sec_queue"),                     juce::dontSendNotification);
        lblLyricsBg_.setText(lm.getText("settings.lbl_lyrics_bg"),              juce::dontSendNotification);
        lblNumSongs_.setText(lm.getText("settings.lbl_num_songs"),              juce::dontSendNotification);
        lblNumSingers_.setText(lm.getText("settings.lbl_num_singers"),          juce::dontSendNotification);
        lblNumSkips_.setText(lm.getText("settings.lbl_num_skips"),              juce::dontSendNotification);
        lblRepeat_.setText(lm.getText("settings.lbl_repeat_songs"),             juce::dontSendNotification);
        lblAutoApprove_.setText(lm.getText("settings.lbl_auto_approve"),        juce::dontSendNotification);
        lblShowOnline_.setText(lm.getText("settings.lbl_show_online"),          juce::dontSendNotification);
        lblShowOnlineEncore_.setText(lm.getText("settings.lbl_show_online_encore"), juce::dontSendNotification);
        lblShowMemory_.setText(lm.getText("settings.lbl_show_memory"),          juce::dontSendNotification);

        secSession_.setText(lm.getText("settings.sec_session"),              juce::dontSendNotification);
        lblSongsToday_.setText(lm.getText("settings.lbl_songs_today"),       juce::dontSendNotification);
        lblActiveMembers_.setText(lm.getText("settings.lbl_active_members"), juce::dontSendNotification);
        lblSingersQueue_.setText(lm.getText("settings.lbl_singers_queue"),   juce::dontSendNotification);
        lblReqSongs_.setText(lm.getText("settings.lbl_req_songs"),           juce::dontSendNotification);
        btnClearRecent_.setButtonText(lm.getText("settings.btn_clear_recent"));
        btnEndSession_.setButtonText(lm.getText("settings.btn_end_session"));
        btnViewArchive_.setButtonText(lm.getText("settings.btn_view_archive"));

        auto rebuildCombo = [](juce::ComboBox& cb,
                               std::initializer_list<std::pair<const char*, int>> items) {
            int prev = cb.getSelectedId();
            cb.clear(juce::dontSendNotification);
            auto& lm2 = LocalizationManager::getInstance();
            for (auto& [key, id] : items)
                cb.addItem(lm2.getText(key), id);
            cb.setSelectedId(prev, juce::dontSendNotification);
        };
        rebuildCombo(cbNumSongs_, {
            {"settings.opt_one",1},{"settings.opt_two",2},{"settings.opt_three",3},
            {"settings.opt_four",4},{"settings.opt_five",5},{"settings.opt_ten",6},
            {"settings.opt_any",7}
        });
        rebuildCombo(cbNumSingers_, {
            {"settings.opt_hide",1},{"settings.opt_next",2},{"settings.opt_two",3},
            {"settings.opt_three",4},{"settings.opt_four",5},{"settings.opt_five",6},
            {"settings.opt_six",7},{"settings.opt_all",8}
        });
        rebuildCombo(cbNumSkips_, {
            {"settings.opt_zero",1},{"settings.opt_one",2},
            {"settings.opt_two",3},{"settings.opt_three",4}
        });
        rebuildCombo(cbLyricsBg_, {
            {"settings.bg_none",1},{"settings.bg_squares",2},{"settings.bg_lines",3},
            {"settings.bg_fading",4},{"settings.bg_circles",5},
            {"settings.bg_rotating",6},{"settings.bg_snow",7}
        });
    }

    //--------------------------------------------------------------------------
    void loadFromVenue(const VenueItem& v)
    {
        valVenueName_.setText(juce::String(v.name),    juce::dontSendNotification);
        valAddress_.setText(juce::String(v.address),   juce::dontSendNotification);
        valCity_.setText(juce::String(v.city),         juce::dontSendNotification);
        valCountry_.setText(juce::String(v.country),   juce::dontSendNotification);
        edVenueName_.setText(juce::String(v.name),     false);
        edAddress_.setText(juce::String(v.address),    false);
        edCity_.setText(juce::String(v.city),          false);
        edCountry_.setText(juce::String(v.country),    false);
        valLicenseKey_.setText(v.registrationKey.empty() ? "(not set)" : juce::String(v.registrationKey), juce::dontSendNotification);
        valVenueId_.setText(v.id.empty() ? "(not set)" : juce::String(v.id), juce::dontSendNotification);
        valCurrentCode_.setText(v.code.empty() ? "(not set)" : juce::String(v.code), juce::dontSendNotification);
        valCurrentEmerg_.setText(v.codePlus.empty() ? "(not set)" : juce::String(v.codePlus), juce::dontSendNotification);

        cbNumSongs_.setSelectedId(juce::jlimit(1, 7, v.numSongs),          juce::dontSendNotification);
        cbNumSingers_.setSelectedId(juce::jlimit(0, 7, v.numSingers) + 1,  juce::dontSendNotification);
        cbNumSkips_.setSelectedId(juce::jlimit(0, 3, v.numStrikes) + 1,    juce::dontSendNotification);
        cbLyricsBg_.setSelectedId(juce::jlimit(0, 6, v.background) + 1,    juce::dontSendNotification);
        tbRepeat_.setToggleState(v.repeatSongs,             juce::dontSendNotification);
        tbAutoApprove_.setToggleState(v.autoapprove,        juce::dontSendNotification);
        tbShowOnline_.setToggleState(v.showOnlineSongs,     juce::dontSendNotification);
        tbShowOnlineEncore_.setToggleState(v.showOnlineSongsEncore, juce::dontSendNotification);
        tbShowMemory_.setToggleState(v.showMemoryStats,     juce::dontSendNotification);
    }

    //--------------------------------------------------------------------------
    void updateSessionStats(const SettingsPage::SessionStats& s)
    {
        valSongsToday_.setText(juce::String(s.songsPlayedToday), juce::dontSendNotification);
        valActiveMembers_.setText(juce::String(s.activeMembers), juce::dontSendNotification);
        valSingersQueue_.setText(juce::String(s.singersInQueue), juce::dontSendNotification);
        valReqSongs_.setText(juce::String(s.requestedSongs),     juce::dontSendNotification);
    }

    //--------------------------------------------------------------------------
    void updateUserList(const std::vector<SettingsPage::VenueUser>& users)
    {
        userRows_.clear();
        if (userListPanel_) userListPanel_->removeAllChildren();

        for (auto& u : users)
        {
            auto row = std::make_unique<UserRowComponent>(u);
            row->onRoleChanged = [this, email = u.email](const juce::String& role) {
                if (owner_.onChangeUserRole) owner_.onChangeUserRole(email, role);
            };
            row->onDeactivate = [this, email = u.email]() {
                if (owner_.onDeactivateUser) owner_.onDeactivateUser(email);
            };
            row->onRemove = [this, email = u.email]() {
                juce::AlertWindow::showOkCancelBox(
                    juce::MessageBoxIconType::WarningIcon,
                    "Remove User", "Remove " + email + " from this venue?",
                    "Remove", "Cancel", nullptr,
                    juce::ModalCallbackFunction::create([this, email](int r) {
                        if (r == 1 && owner_.onRemoveUser) owner_.onRemoveUser(email);
                    }));
            };
            userListPanel_->addAndMakeVisible(row.get());
            userRows_.push_back(std::move(row));
        }
        resized();
    }

private:
    SettingsPage& owner_;
    bool venueEditMode_ = false;

    // Title / top buttons
    juce::Label      titleLabel_;
    juce::TextButton btnEditVenue_, btnDeleteVenue_;

    // Section 1: Venue info
    juce::Label      secVenueInfo_;
    juce::Label      lblVenueName_, lblAddress_, lblCity_, lblCountry_;
    juce::Label      valVenueName_, valAddress_, valCity_, valCountry_;
    juce::TextEditor edVenueName_,  edAddress_,  edCity_,  edCountry_;
    juce::TextButton btnSaveVenue_, btnCancelVenue_;
    juce::Label      savedLabel_;
    juce::Label      lblLicenseKey_, valLicenseKey_;
    juce::Label      lblVenueIdLbl_, valVenueId_;

    // Section 2: Venue code
    juce::Label      secVenueCode_;
    juce::Label      lblCurrentCode_, valCurrentCode_;
    juce::Label      lblManualCode_;
    juce::TextEditor edManualCode_;
    juce::TextButton btnSetCode_, btnGenCode_;
    juce::Label      lblEmergHeader_, lblCurrentEmerg_, valCurrentEmerg_;
    juce::TextEditor edEmergCode_;
    juce::TextButton btnSetEmerg_, btnGenEmerg_;

    // Section 3: Users
    juce::Label      secUsers_;
    juce::Label      lblInviteHeader_;
    juce::TextEditor edInviteEmail_;
    juce::ComboBox   cbInviteRole_;
    juce::TextButton btnInviteUser_;
    juce::Label      lblCurrentUsersHeader_;
    std::unique_ptr<juce::Component> userListPanel_;
    std::vector<std::unique_ptr<UserRowComponent>> userRows_;

    // Section 4: Logo
    juce::Label      secLogo_;
    juce::Label      lblLogo_, logoPathLabel_;
    juce::TextButton btnBrowseLogo_, btnSaveLogo_, btnDefaultLogo_;
    juce::File       selectedLogoFile_;
    std::unique_ptr<juce::FileChooser> fc_;

    // Section 5: Queue / Display
    juce::Label        secQueue_;
    juce::Label        lblLyricsBg_;
    juce::ComboBox     cbLyricsBg_;
    juce::Label        lblNumSongs_,    lblNumSingers_,   lblNumSkips_;
    juce::ComboBox     cbNumSongs_,     cbNumSingers_,    cbNumSkips_;
    juce::Label        lblRepeat_,      lblAutoApprove_;
    juce::ToggleButton tbRepeat_,       tbAutoApprove_;
    juce::Label        lblShowOnline_,  lblShowOnlineEncore_, lblShowMemory_;
    juce::ToggleButton tbShowOnline_,   tbShowOnlineEncore_,  tbShowMemory_;

    // Section 6: Session
    juce::Label      secSession_;
    juce::Label      lblSongsToday_,    valSongsToday_;
    juce::Label      lblActiveMembers_, valActiveMembers_;
    juce::Label      lblSingersQueue_,  valSingersQueue_;
    juce::Label      lblReqSongs_,      valReqSongs_;
    juce::TextButton btnClearRecent_, btnEndSession_, btnViewArchive_;

    // Computed in resized() and drawn in paint() — one rect per section card.
    std::vector<juce::Rectangle<int>> cardRects_;

    //==========================================================================
    void initSectionLabel(juce::Label& lbl, const juce::String& text)
    {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(juce::FontOptions().withHeight(15.f)).boldened());
        lbl.setColour(juce::Label::backgroundColourId, juce::Colour(0));
        lbl.setColour(juce::Label::textColourId,       juce::Colour(kAccentSoft));
        lbl.setJustificationType(juce::Justification::centredLeft);
        lbl.setBorderSize(juce::BorderSize<int>(0, kPadX, 0, 0));
        addAndMakeVisible(lbl);
    }
    void initFieldLabel(juce::Label& lbl, const juce::String& text)
    {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
        lbl.setColour(juce::Label::textColourId, juce::Colour(kTextSecond));
        lbl.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(lbl);
    }
    void initValueLabel(juce::Label& lbl)
    {
        lbl.setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
        lbl.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
        lbl.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(lbl);
    }
    void initEditor(juce::TextEditor& ed, const juce::String& placeholder)
    {
        ed.setMultiLine(false);
        ed.setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
        ed.setTextToShowWhenEmpty(placeholder, juce::Colour(kTextSecond));
        ed.setColour(juce::TextEditor::backgroundColourId,     juce::Colour(0xff0d1527));
        ed.setColour(juce::TextEditor::textColourId,           juce::Colour(kTextPrimary));
        ed.setColour(juce::TextEditor::outlineColourId,        juce::Colour(kAccent).withAlpha(0.4f));
        ed.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(kAccent));
        addAndMakeVisible(ed);
    }
    void initCombo(juce::ComboBox& cb)
    {
        cb.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d1527));
        cb.setColour(juce::ComboBox::textColourId,       juce::Colour(kTextPrimary));
        cb.setColour(juce::ComboBox::outlineColourId,    juce::Colour(kAccent).withAlpha(0.4f));
        cb.setColour(juce::ComboBox::arrowColourId,      juce::Colour(kTextSecond));
        cb.setColour(juce::ComboBox::buttonColourId,     juce::Colour(kBtnNormal));
        addAndMakeVisible(cb);
    }
    void initToggle(juce::ToggleButton& tb)
    {
        tb.setColour(juce::ToggleButton::textColourId,         juce::Colour(kTextPrimary));
        tb.setColour(juce::ToggleButton::tickColourId,         juce::Colour(kAccent));
        tb.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(kTextSecond));
        addAndMakeVisible(tb);
    }
    void initButton(juce::TextButton& btn, const juce::String& text, uint32_t colour)
    {
        btn.setButtonText(text);
        btn.setColour(juce::TextButton::buttonColourId,  juce::Colour(colour));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(btn);
    }

    //--------------------------------------------------------------------------
    void setVenueEditMode(bool editing)
    {
        venueEditMode_ = editing;
        valVenueName_.setVisible(!editing);
        valAddress_.setVisible(!editing);
        valCity_.setVisible(!editing);
        valCountry_.setVisible(!editing);
        edVenueName_.setVisible(editing);
        edAddress_.setVisible(editing);
        edCity_.setVisible(editing);
        edCountry_.setVisible(editing);
        btnSaveVenue_.setVisible(editing);
        btnCancelVenue_.setVisible(editing);
        btnEditVenue_.setVisible(!editing);
        resized();
    }

    void onSaveVenueInfo()
    {
        auto& v   = owner_.venue_;
        v.name    = edVenueName_.getText().toStdString();
        v.address = edAddress_.getText().toStdString();
        v.city    = edCity_.getText().toStdString();
        v.country = edCountry_.getText().toStdString();
        owner_.notifyChanged();
        setVenueEditMode(false);

        savedLabel_.setText(LocalizationManager::getInstance().getText("settings.saved"),
                            juce::dontSendNotification);
        savedLabel_.setVisible(true);
        juce::Timer::callAfterDelay(2000, [this]() {
            if (savedLabel_.isShowing()) savedLabel_.setVisible(false);
        });
    }

    void onDeleteVenue()
    {
        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon,
            "Delete Venue",
            "This will permanently delete this venue and all its data.\n\nThis action cannot be undone. Are you sure?",
            "Delete Venue", "Cancel", nullptr,
            juce::ModalCallbackFunction::create([this](int result) {
                if (result == 1 && owner_.onDeleteVenue)
                    owner_.onDeleteVenue();
            }));
    }

    void onInviteUser()
    {
        auto email = edInviteEmail_.getText().trim();
        if (email.isEmpty() || !email.contains("@")) {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Invalid Email", "Please enter a valid email address.");
            return;
        }
        juce::StringArray roles = { "Basic User", "Host" };
        auto role = roles[juce::jlimit(0, 1, cbInviteRole_.getSelectedId() - 1)];
        if (owner_.onInviteUser) owner_.onInviteUser(email, role);
        edInviteEmail_.clear();
    }

    void onClearRecent()
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Clear Recently Played",
            "Recently played songs have been cleared.");
    }

    void onEndSession()
    {
        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon,
            "End Session & Archive",
            "This will archive all session data and reset the queue.\n\nAre you sure?",
            "End Session", "Cancel", nullptr,
            juce::ModalCallbackFunction::create([](int result) {
                if (result == 1)
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::InfoIcon,
                        "Session Ended", "Session archived successfully.");
            }));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsContentPanel)
};

//==============================================================================
// SettingsPage
//==============================================================================
SettingsPage::SettingsPage()
{
    loadFromCache();

    panel_ = std::make_unique<SettingsContentPanel>(*this);
    panel_->loadFromVenue(venue_);

    scroll_.setViewedComponent(panel_.get(), false);
    scroll_.setScrollBarsShown(true, false);
    scroll_.setScrollBarThickness(8);
    scroll_.getVerticalScrollBar().setColour(
        juce::ScrollBar::thumbColourId, juce::Colour(kAccent).withAlpha(0.5f));
    addAndMakeVisible(scroll_);
}

SettingsPage::~SettingsPage() = default;

void SettingsPage::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void SettingsPage::resized()
{
    scroll_.setBounds(getLocalBounds());
    if (panel_)
    {
        panel_->setSize(getWidth() - scroll_.getScrollBarThickness(), panel_->getHeight());
        panel_->resized();
    }
}

void SettingsPage::updateAllText()
{
    if (panel_) panel_->updateAllText();
}

void SettingsPage::setVenueData(const VenueItem& venue)
{
    venue_ = venue;
    if (panel_) panel_->loadFromVenue(venue_);
    saveToCache();
}

void SettingsPage::setSessionStats(const SessionStats& stats)
{
    if (panel_) panel_->updateSessionStats(stats);
}

void SettingsPage::setUserList(const std::vector<VenueUser>& users)
{
    if (panel_) panel_->updateUserList(users);
}

void SettingsPage::notifyChanged()
{
    saveToCache();
    if (onSettingsChanged) onSettingsChanged(venue_);
}

//==============================================================================
// Cache
//==============================================================================
juce::File SettingsPage::getCacheFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("EncoreKaraoke/settings.json");
}

void SettingsPage::loadFromCache()
{
    auto f = getCacheFile();
    if (!f.existsAsFile()) return;
    auto loaded = VenueItem::fromJson(f.loadFileAsString());
    if (!loaded.id.empty())
        venue_ = loaded;
}

void SettingsPage::saveToCache()
{
    auto f = getCacheFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText(venue_.toJson());
}
