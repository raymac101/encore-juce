/*
  ==============================================================================

    SettingsPage.cpp
    Created: 22 Apr 2026
    Author:  GitHub Copilot

  ==============================================================================
*/

#include "SettingsPage.h"
#include "MenuTheme.h"
#include "../Services/UserPreferences.h"
#include "../Services/PluginHostService.h"

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
    constexpr uint32_t kCardFill    = 0xff1a2a52;   // card background (alpha applied at paint)
    constexpr uint32_t kCardBorder  = 0xff3a568f;   // card border
    constexpr uint32_t kAccent      = 0xff4272b8;
    constexpr uint32_t kAccentSoft  = 0xff5a8fd8;   // lighter accent for headers
    constexpr uint32_t kBtnNormal   = 0xff2f4b80;
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
        auto& lm = LocalizationManager::getInstance();

        nameLabel_.setText(user.email, juce::dontSendNotification);
        nameLabel_.setFont(juce::Font(juce::FontOptions().withHeight(13.f)).boldened());
        nameLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
        addAndMakeVisible(nameLabel_);

        emailLabel_.setText(user.email, juce::dontSendNotification);
        emailLabel_.setFont(juce::Font(juce::FontOptions().withHeight(12.f)));
        emailLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextSecond));
        addAndMakeVisible(emailLabel_);

        statusBadge_.setText(user.active ? lm.getText("settings.active") : lm.getText("settings.inactive"), juce::dontSendNotification);
        statusBadge_.setFont(juce::Font(juce::FontOptions().withHeight(10.f)).boldened());
        statusBadge_.setColour(juce::Label::textColourId,       juce::Colours::white);
        statusBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(user.active ? kTagActive : kBtnNormal));
        statusBadge_.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(statusBadge_);

        const auto normalizedRole = user.role.trim();
        uint32_t roleCol = kBtnNormal;
        if (normalizedRole == "Admin")
            roleCol = kTagAdmin;
        else if (normalizedRole == "Host")
            roleCol = kTagHost;
        else if (normalizedRole == "Tester")
            roleCol = 0xff7a4fa3;
        else if (normalizedRole == "EnterpriseAdmin")
            roleCol = 0xff9a7a1a;
        juce::String roleDisplay;
        if (normalizedRole == "Basic")
            roleDisplay = lm.getText("settings.role_basic");
        else if (normalizedRole == "Host")
            roleDisplay = lm.getText("settings.role_host");
        else if (normalizedRole == "Admin")
            roleDisplay = lm.getText("settings.role_admin");
        else if (normalizedRole == "Tester")
            roleDisplay = lm.getText("settings.role_tester");
        else if (normalizedRole == "EnterpriseAdmin")
            roleDisplay = lm.getText("settings.role_enterprise_admin");
        else
            roleDisplay = user.role.toUpperCase();
        roleBadge_.setText(roleDisplay, juce::dontSendNotification);
        roleBadge_.setFont(juce::Font(juce::FontOptions().withHeight(10.f)).boldened());
        roleBadge_.setColour(juce::Label::textColourId,       juce::Colours::white);
        roleBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(roleCol));
        roleBadge_.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(roleBadge_);

        roleCombo_.addItem(lm.getText("settings.role_basic"),            1);
        roleCombo_.addItem(lm.getText("settings.role_host"),             2);
        roleCombo_.addItem(lm.getText("settings.role_admin"),            3);
        roleCombo_.addItem(lm.getText("settings.role_tester"),           4);
        roleCombo_.addItem(lm.getText("settings.role_enterprise_admin"), 5);
        if (normalizedRole == "Basic")
            roleCombo_.setSelectedId(1, juce::dontSendNotification);
        else if (normalizedRole == "Host")
            roleCombo_.setSelectedId(2, juce::dontSendNotification);
        else if (normalizedRole == "Admin")
            roleCombo_.setSelectedId(3, juce::dontSendNotification);
        else if (normalizedRole == "Tester")
            roleCombo_.setSelectedId(4, juce::dontSendNotification);
        else if (normalizedRole == "EnterpriseAdmin")
            roleCombo_.setSelectedId(5, juce::dontSendNotification);
        else
            roleCombo_.setSelectedId(1, juce::dontSendNotification);
        roleCombo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d1527));
        roleCombo_.setColour(juce::ComboBox::textColourId,       juce::Colour(kTextPrimary));
        roleCombo_.setColour(juce::ComboBox::outlineColourId,    juce::Colour(kAccent).withAlpha(0.4f));
        roleCombo_.onChange = [this]() {
            if (onRoleChanged) {
                juce::StringArray roles = { "Basic", "Host", "Admin", "Tester", "EnterpriseAdmin" };
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
        makeBtn(btnDeactivate_, lm.getText("settings.btn_deactivate"), kBtnNormal);
        makeBtn(btnRemove_,     lm.getText("settings.btn_remove"),     kBtnDanger);
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

class InvitationRowComponent : public juce::Component
{
public:
    std::function<void()> onRevoke;

    explicit InvitationRowComponent(const SettingsPage::PendingInvitation& invite)
    {
        auto& lm = LocalizationManager::getInstance();

        emailLabel_.setText(invite.email, juce::dontSendNotification);
        emailLabel_.setFont(juce::Font(juce::FontOptions().withHeight(13.f)).boldened());
        emailLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
        addAndMakeVisible(emailLabel_);

        const auto expires = invite.expirationDate.toString(true, true);
        const auto statusText = invite.expired ? lm.getText("settings.expired") : lm.getText("settings.expires_prefix") + expires;
        statusLabel_.setText(statusText, juce::dontSendNotification);
        statusLabel_.setFont(juce::Font(juce::FontOptions().withHeight(12.f)));
        statusLabel_.setColour(juce::Label::textColourId,
                               juce::Colour(invite.expired ? kBtnDanger : kTextSecond));
        addAndMakeVisible(statusLabel_);

        roleBadge_.setText(invite.role.toUpperCase(), juce::dontSendNotification);
        roleBadge_.setFont(juce::Font(juce::FontOptions().withHeight(10.f)).boldened());
        roleBadge_.setColour(juce::Label::textColourId, juce::Colours::white);
        roleBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(kBtnNormal));
        roleBadge_.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(roleBadge_);

        btnRevoke_.setButtonText(lm.getText("settings.btn_revoke"));
        btnRevoke_.setColour(juce::TextButton::buttonColourId, juce::Colour(kBtnDanger));
        btnRevoke_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        btnRevoke_.onClick = [this]() {
            if (onRevoke) onRevoke();
        };
        addAndMakeVisible(btnRevoke_);
    }

    void resized() override
    {
        const int w = getWidth();
        emailLabel_.setBounds(kPadX, 6, 280, 18);
        statusLabel_.setBounds(kPadX, 26, 320, 16);
        roleBadge_.setBounds(kPadX + 328, 22, 120, 22);
        btnRevoke_.setBounds(w - kPadX - 100, 14, 100, 28);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(kTextSecond).withAlpha(0.2f));
        g.drawLine(0, (float) getHeight() - 1, (float) getWidth(), (float) getHeight() - 1);
    }

private:
    juce::Label      emailLabel_, statusLabel_, roleBadge_;
    juce::TextButton btnRevoke_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvitationRowComponent)
};

//==============================================================================
// SettingsContentPanel
//==============================================================================
class SettingsContentPanel : public juce::Component,
                              public juce::ChangeListener
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
                    LocalizationManager::getInstance().getText("settings.invalid_code_title"),
                    LocalizationManager::getInstance().getText("settings.invalid_code_body"));
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
                    LocalizationManager::getInstance().getText("settings.invalid_code_title"),
                    LocalizationManager::getInstance().getText("settings.invalid_code_body"));
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
        cbInviteRole_.addItem("Basic",            1);
        cbInviteRole_.addItem("Host",             2);
        cbInviteRole_.addItem("Admin",            3);
        cbInviteRole_.addItem("Tester",           4);
        cbInviteRole_.addItem("Enterprise Admin", 5);
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

        initFieldLabel(lblPendingInvitesHeader_, lm.getText("settings.pending_invitations"));
        lblPendingInvitesHeader_.setFont(juce::Font(juce::FontOptions().withHeight(14.f)).boldened());
        lblPendingInvitesHeader_.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
        inviteListPanel_ = std::make_unique<juce::Component>();
        addAndMakeVisible(*inviteListPanel_);

        initFieldLabel(lblExpiredInvitesHeader_, lm.getText("settings.expired_invitations"));
        lblExpiredInvitesHeader_.setFont(juce::Font(juce::FontOptions().withHeight(14.f)).boldened());
        lblExpiredInvitesHeader_.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
        expiredInviteListPanel_ = std::make_unique<juce::Component>();
        addAndMakeVisible(*expiredInviteListPanel_);

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
                LocalizationManager::getInstance().getText("settings.select_logo_image"),
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
            {
                btnSaveLogo_.setEnabled(false);
                btnDefaultLogo_.setEnabled(false);
                logoPathLabel_.setText(LocalizationManager::getInstance().getText("settings.logo_uploading"), juce::dontSendNotification);
                logoPathLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextSecond));
                owner_.onUploadLogo(selectedLogoFile_);
            }
        };
        btnDefaultLogo_.onClick = [this]() {
            if (owner_.onResetLogo)
            {
                btnSaveLogo_.setEnabled(false);
                btnDefaultLogo_.setEnabled(false);
                owner_.onResetLogo();
            }
        };

        // Lyric screen sizing sliders -- write straight to UserPreferences on
        // every drag tick (no Save button); LyricDisplayComponent reads these
        // directly on its next repaint (it already runs a 30Hz timer), so
        // the lyric window updates live as the slider moves.
        auto& prefs = UserPreferences::getInstance();

        initFieldLabel(lblLyricLogoScale_, lm.getText("settings.lbl_lyric_logo_scale"));
        initSlider(sLyricLogoScale_, 50, 200, 5, "%");
        sLyricLogoScale_.setValue(prefs.getLyricLogoScalePercent(), juce::dontSendNotification);
        sLyricLogoScale_.onValueChange = [this]() {
            UserPreferences::getInstance().setLyricLogoScalePercent((int) sLyricLogoScale_.getValue());
        };

        initFieldLabel(lblLyricBrandTextScale_, lm.getText("settings.lbl_lyric_brand_text_scale"));
        initSlider(sLyricBrandTextScale_, 50, 200, 5, "%");
        sLyricBrandTextScale_.setValue(prefs.getLyricBrandTextScalePercent(), juce::dontSendNotification);
        sLyricBrandTextScale_.onValueChange = [this]() {
            UserPreferences::getInstance().setLyricBrandTextScalePercent((int) sLyricBrandTextScale_.getValue());
        };

        initFieldLabel(lblLyricNowSingingTextScale_, lm.getText("settings.lbl_lyric_now_singing_text_scale"));
        initSlider(sLyricNowSingingTextScale_, 50, 200, 5, "%");
        sLyricNowSingingTextScale_.setValue(prefs.getLyricNowSingingTextScalePercent(), juce::dontSendNotification);
        sLyricNowSingingTextScale_.onValueChange = [this]() {
            UserPreferences::getInstance().setLyricNowSingingTextScalePercent((int) sLyricNowSingingTextScale_.getValue());
        };

        initFieldLabel(lblLyricNowSingingInfoScale_, lm.getText("settings.lbl_lyric_now_singing_info_scale"));
        initSlider(sLyricNowSingingInfoScale_, 50, 200, 5, "%");
        sLyricNowSingingInfoScale_.setValue(prefs.getLyricNowSingingInfoScalePercent(), juce::dontSendNotification);
        sLyricNowSingingInfoScale_.onValueChange = [this]() {
            UserPreferences::getInstance().setLyricNowSingingInfoScalePercent((int) sLyricNowSingingInfoScale_.getValue());
        };

        initFieldLabel(lblLyricUpNextTextScale_, lm.getText("settings.lbl_lyric_up_next_text_scale"));
        initSlider(sLyricUpNextTextScale_, 50, 200, 5, "%");
        sLyricUpNextTextScale_.setValue(prefs.getLyricUpNextTextScalePercent(), juce::dontSendNotification);
        sLyricUpNextTextScale_.onValueChange = [this]() {
            UserPreferences::getInstance().setLyricUpNextTextScalePercent((int) sLyricUpNextTextScale_.getValue());
        };

        initFieldLabel(lblLyricUpNextInfoScale_, lm.getText("settings.lbl_lyric_up_next_info_scale"));
        initSlider(sLyricUpNextInfoScale_, 50, 200, 5, "%");
        sLyricUpNextInfoScale_.setValue(prefs.getLyricUpNextInfoScalePercent(), juce::dontSendNotification);
        sLyricUpNextInfoScale_.onValueChange = [this]() {
            UserPreferences::getInstance().setLyricUpNextInfoScalePercent((int) sLyricUpNextInfoScale_.getValue());
        };

        initFieldLabel(lblLyricBottomBarTextScale_, lm.getText("settings.lbl_lyric_bottom_bar_text_scale"));
        initSlider(sLyricBottomBarTextScale_, 50, 200, 5, "%");
        sLyricBottomBarTextScale_.setValue(prefs.getLyricBottomBarTextScalePercent(), juce::dontSendNotification);
        sLyricBottomBarTextScale_.onValueChange = [this]() {
            UserPreferences::getInstance().setLyricBottomBarTextScalePercent((int) sLyricBottomBarTextScale_.getValue());
        };

        // Moved here from Queue/Display Settings, and converted from a
        // discrete %-choice ComboBox to a live-preview slider.
        initFieldLabel(lblLyricCodeBarHeight_, lm.getText("settings.lbl_lyric_code_bar_height"));
        initSlider(sLyricCodeBarHeight_, 6, 20, 1, "%");
        sLyricCodeBarHeight_.setValue(prefs.getLyricVenueCodeBarHeightPercent(), juce::dontSendNotification);
        sLyricCodeBarHeight_.onValueChange = [this]() {
            UserPreferences::getInstance().setLyricVenueCodeBarHeightPercent((int) sLyricCodeBarHeight_.getValue());
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
        initFieldLabel(lblSilenceThreshold_, lm.getText("settings.lbl_silence_threshold"));
        initFieldLabel(lblLyricAdLead_,      lm.getText("settings.lbl_lyric_ad_lead"));

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

        initCombo(cbSilenceThreshold_);
        cbSilenceThreshold_.addItem("-35 dB", 1);
        cbSilenceThreshold_.addItem("-40 dB", 2);
        cbSilenceThreshold_.addItem("-45 dB", 3);
        cbSilenceThreshold_.addItem("-50 dB", 4);
        cbSilenceThreshold_.addItem("-55 dB", 5);
        cbSilenceThreshold_.addItem("-60 dB", 6);
        cbSilenceThreshold_.addItem("-65 dB", 7);
        cbSilenceThreshold_.addItem("-70 dB", 8);
        {
            const auto db = UserPreferences::getInstance().getTrailingSilenceThresholdDb();
            int id = 4;
            if (db > -37.5f) id = 1;
            else if (db > -42.5f) id = 2;
            else if (db > -47.5f) id = 3;
            else if (db > -52.5f) id = 4;
            else if (db > -57.5f) id = 5;
            else if (db > -62.5f) id = 6;
            else if (db > -67.5f) id = 7;
            else id = 8;
            cbSilenceThreshold_.setSelectedId(id, juce::dontSendNotification);
        }
        cbSilenceThreshold_.onChange = [this]() {
            constexpr float values[] = { -35.0f, -40.0f, -45.0f, -50.0f, -55.0f, -60.0f, -65.0f, -70.0f };
            const int idx = juce::jlimit(0, 7, cbSilenceThreshold_.getSelectedId() - 1);
            UserPreferences::getInstance().setTrailingSilenceThresholdDb(values[idx]);
        };

        initCombo(cbLyricAdLead_);
        for (int s = 3; s <= 15; ++s)
            cbLyricAdLead_.addItem(juce::String(s) + " s", s - 2);
        cbLyricAdLead_.setSelectedId(
            juce::jlimit(1, 13, UserPreferences::getInstance().getLyricAdTransitionLeadSeconds() - 2),
            juce::dontSendNotification);
        cbLyricAdLead_.onChange = [this]() {
            UserPreferences::getInstance().setLyricAdTransitionLeadSeconds(cbLyricAdLead_.getSelectedId() + 2);
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

        // Nightly cleanup time selector. Persisted to UserPreferences so it
        // survives restarts; the actual scheduled task reads the value from
        // there. 24 entries, one per hour-of-day; ComboBox IDs are 1-based so
        // we use (hour + 1).
        initFieldLabel(lblCleanupHour_, lm.getText("settings.lbl_cleanup_time"));
        initCombo(cbCleanupHour_);
        for (int h = 0; h < 24; ++h)
        {
            const int    hour12 = ((h + 11) % 12) + 1;
            const char*  ampm   = (h < 12) ? "AM" : "PM";
            cbCleanupHour_.addItem(juce::String(hour12) + ":00 " + ampm, h + 1);
        }
        cbCleanupHour_.setSelectedId(UserPreferences::getInstance().getNightlyCleanupHour() + 1,
                                     juce::dontSendNotification);
        cbCleanupHour_.onChange = [this]() {
            UserPreferences::getInstance().setNightlyCleanupHour(cbCleanupHour_.getSelectedId() - 1);
        };

        initButton(btnClearRecent_, lm.getText("settings.btn_clear_recent"), kBtnNormal);
        initButton(btnEndSession_,  lm.getText("settings.btn_end_session"),  kBtnDanger);
        initButton(btnViewArchive_, lm.getText("settings.btn_view_archive"), kBtnNormal);
        btnClearRecent_.onClick = [this]() { onClearRecent(); };
        btnEndSession_.onClick  = [this]() { onEndSession(); };
        btnViewArchive_.onClick = [this]() {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                LocalizationManager::getInstance().getText("settings.archive_history_title"),
                LocalizationManager::getInstance().getText("settings.archive_history_body"));
        };

        // ── Section 7: Audio Devices (live mic input) ────────────────────────
        initSectionLabel(secAudioDevices_, lm.getText("settings.sec_audio_devices"));
        initFieldLabel(lblEnableVocalInput_, lm.getText("settings.lbl_enable_vocal_input"));
        initToggle(tbEnableVocalInput_);
        tbEnableVocalInput_.setToggleState(UserPreferences::getInstance().getLiveVocalInputEnabled(), juce::dontSendNotification);
        tbEnableVocalInput_.onStateChange = [this]() {
            UserPreferences::getInstance().setLiveVocalInputEnabled(tbEnableVocalInput_.getToggleState());
        };

        initFieldLabel(lblMic1Channel_, lm.getText("settings.lbl_mic1_channel"));
        initCombo(cbMic1Channel_);
        cbMic1Channel_.onChange = [this]() {
            if (audioEngine_ != nullptr)
                audioEngine_->setMicInputChannel(1, cbMic1Channel_.getSelectedId() - 2);
        };

        initFieldLabel(lblMic2Channel_, lm.getText("settings.lbl_mic2_channel"));
        initCombo(cbMic2Channel_);
        cbMic2Channel_.onChange = [this]() {
            if (audioEngine_ != nullptr)
                audioEngine_->setMicInputChannel(2, cbMic2Channel_.getSelectedId() - 2);
        };

        initFieldLabel(lblMicWarning_, lm.getText("settings.lbl_mic_no_input"));
        lblMicWarning_.setColour(juce::Label::textColourId, juce::Colour(kBtnDanger));
        lblMicWarning_.setVisible(false);

        // ── Section 8: VST3 Plugins ──────────────────────────────────────────
        initSectionLabel(secPlugins_, lm.getText("settings.sec_plugins"));
        initButton(btnRescanPlugins_, lm.getText("settings.btn_rescan_plugins"), kBtnNormal);
        btnRescanPlugins_.onClick = [this]() { startPluginScan(); };
        initValueLabel(lblPluginScanStatus_);
        refreshPluginScanStatusLabel();

        // Live scan log — shows each plugin as it's actually found (not
        // just attempted) so a scan of a large plugin folder doesn't look
        // like the app has frozen.
        pluginScanLog_.setMultiLine(true, false);
        pluginScanLog_.setReadOnly(true);
        pluginScanLog_.setScrollbarsShown(true);
        pluginScanLog_.setCaretVisible(false);
        pluginScanLog_.setFont(juce::Font(juce::FontOptions().withHeight(12.f)));
        pluginScanLog_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0d1527));
        pluginScanLog_.setColour(juce::TextEditor::textColourId, juce::Colour(kTextSecond));
        pluginScanLog_.setColour(juce::TextEditor::outlineColourId, juce::Colour(kAccent).withAlpha(0.4f));
        addAndMakeVisible(pluginScanLog_);
    }

    void startPluginScan()
    {
        btnRescanPlugins_.setEnabled(false);
        lblPluginScanStatus_.setText(
            LocalizationManager::getInstance().getText("settings.plugin_scan_in_progress"),
            juce::dontSendNotification);
        pluginScanLog_.clear();

        juce::Component::SafePointer<SettingsContentPanel> safe(this);
        PluginHostService::getInstance().scanForPlugins(
            [safe](float progress01, juce::String name, bool found)
            {
                if (safe == nullptr)
                    return;

                // Update the status line every tick (proves the scan is
                // still moving even while nothing new has been found yet)...
                safe->lblPluginScanStatus_.setText(
                    LocalizationManager::getInstance().getText("settings.plugin_scan_in_progress")
                        + " (" + juce::String((int) (progress01 * 100.0f)) + "%)",
                    juce::dontSendNotification);

                // ...and append a line the moment a real plugin is found.
                if (found)
                {
                    safe->pluginScanLog_.moveCaretToEnd();
                    safe->pluginScanLog_.insertTextAtCaret(name + juce::newLine);
                }
            },
            [safe](int numFound)
            {
                if (safe == nullptr)
                    return;
                safe->btnRescanPlugins_.setEnabled(true);
                safe->refreshPluginScanStatusLabel(numFound);
            });
    }

    void refreshPluginScanStatusLabel(int lastFoundCount = -1)
    {
        auto& lm = LocalizationManager::getInstance();
        const auto lastScan = PluginHostService::getInstance().getLastScanTime();

        if (lastFoundCount >= 0)
        {
            lblPluginScanStatus_.setText(
                lm.getText("settings.plugin_scan_found_prefix") + juce::String(lastFoundCount),
                juce::dontSendNotification);
        }
        else if (lastScan != juce::Time())
        {
            lblPluginScanStatus_.setText(
                lm.getText("settings.plugin_scan_last_prefix") + lastScan.toString(true, true),
                juce::dontSendNotification);
        }
        else
        {
            lblPluginScanStatus_.setText(
                lm.getText("settings.plugin_scan_never"), juce::dontSendNotification);
        }
    }

    //--------------------------------------------------------------------------
    ~SettingsContentPanel() override
    {
        if (audioEngine_ != nullptr)
            audioEngine_->getDeviceManager().removeChangeListener(this);
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
        auto sliderRow = [&](juce::Label& lbl, juce::Slider& s) {
            lbl.setBounds(kPadX, y, kLabelW, kRowH);
            s.setBounds(kCtrlX, y, w - kCtrlX - kPadX, kRowH);
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
        y += kFieldGap;
        lblPendingInvitesHeader_.setBounds(kPadX, y, w - kPadX * 2, 26); y += 30;
        if (inviteListPanel_)
        {
            const int inviteRowH = 56;
            int numRows = (int) inviteRows_.size();
            inviteListPanel_->setBounds(0, y, w, inviteRowH * juce::jmax(1, numRows));
            for (int i = 0; i < numRows; ++i)
                inviteRows_[(size_t) i]->setBounds(0, i * inviteRowH, w, inviteRowH);
            y += inviteListPanel_->getHeight();
        }
        y += kFieldGap;
        lblExpiredInvitesHeader_.setBounds(kPadX, y, w - kPadX * 2, 26); y += 30;
        if (expiredInviteListPanel_)
        {
            const int inviteRowH = 56;
            int numRows = (int) expiredInviteRows_.size();
            expiredInviteListPanel_->setBounds(0, y, w, inviteRowH * juce::jmax(1, numRows));
            for (int i = 0; i < numRows; ++i)
                expiredInviteRows_[(size_t) i]->setBounds(0, i * inviteRowH, w, inviteRowH);
            y += expiredInviteListPanel_->getHeight();
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
        y += kRowH + kFieldGap;
        sliderRow(lblLyricLogoScale_,          sLyricLogoScale_);
        sliderRow(lblLyricBrandTextScale_,     sLyricBrandTextScale_);
        sliderRow(lblLyricNowSingingTextScale_, sLyricNowSingingTextScale_);
        sliderRow(lblLyricNowSingingInfoScale_, sLyricNowSingingInfoScale_);
        sliderRow(lblLyricUpNextTextScale_,     sLyricUpNextTextScale_);
        sliderRow(lblLyricUpNextInfoScale_,     sLyricUpNextInfoScale_);
        sliderRow(lblLyricBottomBarTextScale_,  sLyricBottomBarTextScale_);
        sliderRow(lblLyricCodeBarHeight_,       sLyricCodeBarHeight_);
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
        comboRow(lblSilenceThreshold_,  cbSilenceThreshold_);
        comboRow(lblLyricAdLead_,       cbLyricAdLead_);
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
        comboRow(lblCleanupHour_, cbCleanupHour_);
        btnClearRecent_.setBounds(kPadX,       y, 200, kRowH);
        btnEndSession_.setBounds(kPadX + 208,  y, 200, kRowH);
        btnViewArchive_.setBounds(kPadX + 416, y, 200, kRowH);
        y += kRowH;
        cardEnd(cs);
        y += kSectionGap;

        // Section 7: Audio Devices
        cs = cardStart();
        secAudioDevices_.setBounds(0, y, w, kSectionH); y += kSectionH + kFieldGap;
        toggleRow(lblEnableVocalInput_, tbEnableVocalInput_);
        if (deviceSelector_ != nullptr)
        {
            const int deviceSelectorHeight = 360;
            deviceSelector_->setBounds(kPadX, y, w - kPadX * 2, deviceSelectorHeight);
            y += deviceSelectorHeight + kFieldGap;
        }
        comboRow(lblMic1Channel_, cbMic1Channel_);
        comboRow(lblMic2Channel_, cbMic2Channel_);
        lblMicWarning_.setBounds(kPadX, y, w - kPadX * 2, kRowH); y += kRowH + kFieldGap;
        cardEnd(cs);
        y += kSectionGap;

        // Section 8: VST3 Plugins
        cs = cardStart();
        secPlugins_.setBounds(0, y, w, kSectionH); y += kSectionH + kFieldGap;
        btnRescanPlugins_.setBounds(kPadX, y, 160, kRowH);
        lblPluginScanStatus_.setBounds(kPadX + 168, y, w - kPadX * 2 - 168, kRowH);
        y += kRowH + kFieldGap;
        const int scanLogHeight = 110;
        pluginScanLog_.setBounds(kPadX, y, w - kPadX * 2, scanLogHeight);
        y += scanLogHeight;
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
        lblPendingInvitesHeader_.setText("Pending Invitations", juce::dontSendNotification);
        lblExpiredInvitesHeader_.setText("Expired Invitations", juce::dontSendNotification);

        secLogo_.setText(lm.getText("settings.sec_logo"),                  juce::dontSendNotification);
        lblLogo_.setText(lm.getText("settings.lbl_logo"),                  juce::dontSendNotification);
        btnBrowseLogo_.setButtonText(lm.getText("settings.btn_browse"));
        btnSaveLogo_.setButtonText(lm.getText("settings.btn_save_logo"));
        btnDefaultLogo_.setButtonText(lm.getText("settings.btn_default_logo"));
        lblLyricLogoScale_.setText(lm.getText("settings.lbl_lyric_logo_scale"), juce::dontSendNotification);
        lblLyricBrandTextScale_.setText(lm.getText("settings.lbl_lyric_brand_text_scale"), juce::dontSendNotification);
        lblLyricNowSingingTextScale_.setText(lm.getText("settings.lbl_lyric_now_singing_text_scale"), juce::dontSendNotification);
        lblLyricNowSingingInfoScale_.setText(lm.getText("settings.lbl_lyric_now_singing_info_scale"), juce::dontSendNotification);
        lblLyricUpNextTextScale_.setText(lm.getText("settings.lbl_lyric_up_next_text_scale"), juce::dontSendNotification);
        lblLyricUpNextInfoScale_.setText(lm.getText("settings.lbl_lyric_up_next_info_scale"), juce::dontSendNotification);
        lblLyricBottomBarTextScale_.setText(lm.getText("settings.lbl_lyric_bottom_bar_text_scale"), juce::dontSendNotification);
        lblLyricCodeBarHeight_.setText(lm.getText("settings.lbl_lyric_code_bar_height"), juce::dontSendNotification);

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
        lblSilenceThreshold_.setText(lm.getText("settings.lbl_silence_threshold"), juce::dontSendNotification);
        lblLyricAdLead_.setText(lm.getText("settings.lbl_lyric_ad_lead"), juce::dontSendNotification);

        secSession_.setText(lm.getText("settings.sec_session"),              juce::dontSendNotification);
        lblSongsToday_.setText(lm.getText("settings.lbl_songs_today"),       juce::dontSendNotification);
        lblActiveMembers_.setText(lm.getText("settings.lbl_active_members"), juce::dontSendNotification);
        lblSingersQueue_.setText(lm.getText("settings.lbl_singers_queue"),   juce::dontSendNotification);
        lblReqSongs_.setText(lm.getText("settings.lbl_req_songs"),           juce::dontSendNotification);
        lblCleanupHour_.setText(lm.getText("settings.lbl_cleanup_time"),     juce::dontSendNotification);
        btnClearRecent_.setButtonText(lm.getText("settings.btn_clear_recent"));
        btnEndSession_.setButtonText(lm.getText("settings.btn_end_session"));
        btnViewArchive_.setButtonText(lm.getText("settings.btn_view_archive"));

        secAudioDevices_.setText(lm.getText("settings.sec_audio_devices"), juce::dontSendNotification);
        lblEnableVocalInput_.setText(lm.getText("settings.lbl_enable_vocal_input"), juce::dontSendNotification);
        lblMic1Channel_.setText(lm.getText("settings.lbl_mic1_channel"), juce::dontSendNotification);
        lblMic2Channel_.setText(lm.getText("settings.lbl_mic2_channel"), juce::dontSendNotification);
        lblMicWarning_.setText(lm.getText("settings.lbl_mic_no_input"), juce::dontSendNotification);

        secPlugins_.setText(lm.getText("settings.sec_plugins"), juce::dontSendNotification);
        btnRescanPlugins_.setButtonText(lm.getText("settings.btn_rescan_plugins"));
        refreshPluginScanStatusLabel();

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

    void updateInvitationList(const std::vector<SettingsPage::PendingInvitation>& invites)
    {
        inviteRows_.clear();
        expiredInviteRows_.clear();
        if (inviteListPanel_) inviteListPanel_->removeAllChildren();
        if (expiredInviteListPanel_) expiredInviteListPanel_->removeAllChildren();

        for (auto& inv : invites)
        {
            auto row = std::make_unique<InvitationRowComponent>(inv);
            row->onRevoke = [this, email = inv.email]() {
                juce::AlertWindow::showOkCancelBox(
                    juce::MessageBoxIconType::WarningIcon,
                    "Revoke Invitation", "Revoke pending invitation for " + email + "?",
                    "Revoke", "Cancel", nullptr,
                    juce::ModalCallbackFunction::create([this, email](int r) {
                        if (r == 1 && owner_.onRevokeInvitation) owner_.onRevokeInvitation(email);
                    }));
            };
            if (inv.expired)
            {
                if (expiredInviteListPanel_) expiredInviteListPanel_->addAndMakeVisible(row.get());
                expiredInviteRows_.push_back(std::move(row));
            }
            else
            {
                if (inviteListPanel_) inviteListPanel_->addAndMakeVisible(row.get());
                inviteRows_.push_back(std::move(row));
            }
        }
        resized();
    }

    //--------------------------------------------------------------------------
    void setAudioEngine(AudioEngine* engine)
    {
        audioEngine_ = engine;
        if (audioEngine_ != nullptr && deviceSelector_ == nullptr)
        {
            deviceSelector_ = std::make_unique<juce::AudioDeviceSelectorComponent>(
                audioEngine_->getDeviceManager(),
                0, 2,    // min/max input channels
                2, 2,    // min/max output channels
                false,   // showMidiInputOptions
                false,   // showMidiOutputSelector
                false,   // showChannelsAsStereoPairs — want individual channel numbering for the Mic 1/2 pickers below
                true);   // hideAdvancedOptionsWithButton — keep it compact by default
            addAndMakeVisible(*deviceSelector_);
            audioEngine_->getDeviceManager().addChangeListener(this);
        }
        refreshAudioDeviceSection();
        resized();
    }

    //--------------------------------------------------------------------------
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        refreshAudioDeviceSection();
    }

    //--------------------------------------------------------------------------
    void setLogoStatus(const juce::String& text, bool isError)
    {
        btnSaveLogo_.setEnabled(true);
        btnDefaultLogo_.setEnabled(true);
        logoPathLabel_.setText(text, juce::dontSendNotification);
        logoPathLabel_.setColour(juce::Label::textColourId,
                                  isError ? juce::Colour(0xfff87171) : juce::Colour(kTextPrimary));
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
    juce::Label      lblPendingInvitesHeader_;
    std::unique_ptr<juce::Component> inviteListPanel_;
    std::vector<std::unique_ptr<InvitationRowComponent>> inviteRows_;
    juce::Label      lblExpiredInvitesHeader_;
    std::unique_ptr<juce::Component> expiredInviteListPanel_;
    std::vector<std::unique_ptr<InvitationRowComponent>> expiredInviteRows_;

    // Section 4: Logo
    juce::Label      secLogo_;
    juce::Label      lblLogo_, logoPathLabel_;
    juce::TextButton btnBrowseLogo_, btnSaveLogo_, btnDefaultLogo_;
    juce::File       selectedLogoFile_;
    std::unique_ptr<juce::FileChooser> fc_;

    // Lyric screen sizing sliders (live preview on the lyric display,
    // no Save button -- same UserPreferences-direct pattern as the silence
    // threshold / lyric ad lead controls below).
    juce::Label  lblLyricLogoScale_, lblLyricBrandTextScale_,
                 lblLyricNowSingingTextScale_, lblLyricNowSingingInfoScale_,
                 lblLyricUpNextTextScale_, lblLyricUpNextInfoScale_,
                 lblLyricBottomBarTextScale_, lblLyricCodeBarHeight_;
    juce::Slider sLyricLogoScale_, sLyricBrandTextScale_,
                 sLyricNowSingingTextScale_, sLyricNowSingingInfoScale_,
                 sLyricUpNextTextScale_, sLyricUpNextInfoScale_,
                 sLyricBottomBarTextScale_, sLyricCodeBarHeight_;

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
    juce::Label        lblSilenceThreshold_, lblLyricAdLead_;
    juce::ComboBox     cbSilenceThreshold_, cbLyricAdLead_;

    // Section 6: Session
    juce::Label      secSession_;
    juce::Label      lblSongsToday_,    valSongsToday_;
    juce::Label      lblActiveMembers_, valActiveMembers_;
    juce::Label      lblSingersQueue_,  valSingersQueue_;
    juce::Label      lblReqSongs_,      valReqSongs_;
    juce::Label      lblCleanupHour_;
    juce::ComboBox   cbCleanupHour_;
    juce::TextButton btnClearRecent_, btnEndSession_, btnViewArchive_;

    // Section 7: Audio Devices
    juce::Label      secAudioDevices_;
    juce::Label      lblEnableVocalInput_;
    juce::ToggleButton tbEnableVocalInput_;
    AudioEngine*     audioEngine_ = nullptr;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector_;
    juce::Label      lblMic1Channel_, lblMic2Channel_;
    juce::ComboBox   cbMic1Channel_, cbMic2Channel_;
    juce::Label      lblMicWarning_;

    // Section 8: VST3 Plugins
    juce::Label      secPlugins_;
    juce::TextButton btnRescanPlugins_;
    juce::Label      lblPluginScanStatus_;
    juce::TextEditor pluginScanLog_;

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
    void initSlider(juce::Slider& s, int minValue, int maxValue, int step, const juce::String& suffix)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setRange((double) minValue, (double) maxValue, (double) step);
        s.setTextValueSuffix(suffix);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, kRowH);
        s.setColour(juce::Slider::backgroundColourId,      juce::Colour(0xff0d1527));
        s.setColour(juce::Slider::trackColourId,           juce::Colour(kAccent));
        s.setColour(juce::Slider::thumbColourId,            juce::Colour(kAccent));
        s.setColour(juce::Slider::textBoxTextColourId,      juce::Colour(kTextPrimary));
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0d1527));
        s.setColour(juce::Slider::textBoxOutlineColourId,   juce::Colour(kAccent).withAlpha(0.4f));
        addAndMakeVisible(s);
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
        juce::StringArray roles = { "Basic", "Host", "Admin", "Tester", "EnterpriseAdmin" };
        auto role = roles[juce::jlimit(0, (int) roles.size() - 1, cbInviteRole_.getSelectedId() - 1)];
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
            "This will archive all session data and clear the queue, "
            "requested songs, audit and members audit collections.\n\n"
            "Are you sure?",
            "End Session", "Cancel", nullptr,
            juce::ModalCallbackFunction::create([this](int result) {
                if (result != 1) return;
                if (! owner_.onEndSession)
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "End Session",
                        "Archive handler not wired up.");
                    return;
                }
                owner_.onEndSession([](bool ok) {
                    juce::AlertWindow::showMessageBoxAsync(
                        ok ? juce::MessageBoxIconType::InfoIcon
                           : juce::MessageBoxIconType::WarningIcon,
                        "Session Ended",
                        ok ? "Session archived successfully."
                           : "Session archive failed. See log for details.");
                });
            }));
    }

    void refreshAudioDeviceSection()
    {
        if (audioEngine_ == nullptr)
            return;

        const auto channelNames = audioEngine_->getAvailableInputChannelNames();

        auto populate = [&](juce::ComboBox& combo, int micIndex)
        {
            combo.clear(juce::dontSendNotification);
            combo.addItem(LocalizationManager::getInstance().getText("settings.mic_channel_none"), 1);
            for (int i = 0; i < channelNames.size(); ++i)
                combo.addItem(channelNames[i], i + 2);
            const int mapped = audioEngine_->getMicInputChannel(micIndex);
            combo.setSelectedId(mapped + 2, juce::dontSendNotification);
        };
        populate(cbMic1Channel_, 1);
        populate(cbMic2Channel_, 2);

        const bool hasInput = ! channelNames.isEmpty();
        lblMicWarning_.setVisible(! hasInput);
        cbMic1Channel_.setEnabled(hasInput);
        cbMic2Channel_.setEnabled(hasInput);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsContentPanel)
};

//==============================================================================
// SettingsPage
//==============================================================================
SettingsPage::SettingsPage()
{
    setOpaque(true);

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
    MenuTheme::drawPageBackground(g, getLocalBounds());
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

void SettingsPage::setPendingInvitations(const std::vector<PendingInvitation>& invitations)
{
    if (panel_) panel_->updateInvitationList(invitations);
}

void SettingsPage::setAudioEngine(AudioEngine* engine)
{
    if (panel_) panel_->setAudioEngine(engine);
}

void SettingsPage::setLogoStatus(const juce::String& text, bool isError)
{
    if (panel_) panel_->setLogoStatus(text, isError);
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
