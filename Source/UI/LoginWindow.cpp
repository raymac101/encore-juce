/*
  ==============================================================================

    LoginWindow.cpp

    Auth + venue selection UI. Implemented as a single Component (LoginContent)
    that swaps between four sub-views. Keeps the file-count low while still
    matching the page model from the design document.

  ==============================================================================
*/

#include "LoginWindow.h"
#include "LoginTheme.h"
#include "../Localization/LocalizationManager.h"
#include "../Auth/LoginFlowController.h"
#include "../Services/FirestoreClient.h"
#include "../Services/UserPreferences.h"
#include "../Services/VenueService.h"
#include "../Services/VenueSessionService.h"
#include "../Services/ImageCache.h"
#include "../Models/VenueItem.h"
#include "../Models/UserVenueAssociation.h"
#include "../Models/AccessRights.h"
#include "BuildInfo.h"
#include "Onboarding/OnboardingWizard.h"

namespace
{
    // Was a compile-time-only ENCORE_DEV_SKIP_LOGIN macro (baked into every
    // build, including the production installer -- meaning a "Skip Login
    // (Dev)" button that auto-signs in with a real, hardcoded password was
    // shipping to customers). Now a runtime opt-in instead: pass
    // --dev-skip-login on the command line to reveal it. Checked once at
    // LoginContent construction, same juce::JUCEApplicationBase API already
    // used for --scan-plugin=/--scan-output= in PluginHostService.
    bool isDevSkipLoginEnabled()
    {
        for (auto& arg : juce::JUCEApplicationBase::getCommandLineParameterArray())
            if (arg == "--dev-skip-login")
                return true;
        return false;
    }
}

//==============================================================================
class LoginWindow::LoginContent : public juce::Component
{
public:
    explicit LoginContent(LoginCompleteCallback onComplete)
        : onComplete_(std::move(onComplete))
    {
        setLookAndFeel(&lnf_);
        auto& lm = LocalizationManager::getInstance();

        // Logo (matches `.encore-logo` in the Angular auth screen).
        const auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                .getParentDirectory();
        const auto logoFile = appDir.getChildFile("assets/images/encore-logo-white.png");
        if (logoFile.existsAsFile())
            logoImage_ = juce::ImageFileFormat::loadFrom(logoFile);

        addAndMakeVisible(versionLabel_);
        versionLabel_.setJustificationType(juce::Justification::centred);
        versionLabel_.setColour(juce::Label::textColourId,
                                juce::Colour(LoginTheme::kSubtleText));
        versionLabel_.setFont(juce::Font(juce::FontOptions(14.0f)));
        const juce::String baseVersion = ProjectInfo::versionString;
        const juce::String buildNumber = juce::String(ENCORE_BUILD_NUMBER);
        versionLabel_.setText(lm.getTextWithParams("login.version_label", { baseVersion, buildNumber }),
                              juce::dontSendNotification);

        addAndMakeVisible(headingLabel_);
        addAndMakeVisible(statusLabel_);
        addAndMakeVisible(busySpinner_);
        busySpinner_.setVisible(false);

        headingLabel_.setFont(juce::Font(juce::FontOptions(28.0f)).withStyle(juce::Font::plain));
        headingLabel_.setJustificationType(juce::Justification::centredLeft);
        headingLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
        headingLabel_.setText(lm.getText("login.heading_login"), juce::dontSendNotification);

        statusLabel_.setJustificationType(juce::Justification::centred);
        statusLabel_.setColour(juce::Label::textColourId,
                               juce::Colour(LoginTheme::kSubtleText));
        statusLabel_.setFont(juce::Font(juce::FontOptions(13.0f)));

        // ── Login page widgets ────────────────────────────────────────────────
        addChildComponent(emailEditor_);
        addChildComponent(passwordEditor_);
        addChildComponent(loginButton_);
        addChildComponent(switchModeButton_);
        addChildComponent(googleButton_);
        addChildComponent(appleButton_);
        addChildComponent(getStartedButton_);

        styleEditor(emailEditor_, lm.getText("login.email_placeholder"));
        styleEditor(passwordEditor_, lm.getText("login.password_placeholder"));
        passwordEditor_.setPasswordCharacter((juce::juce_wchar) 0x2022); // bullet

        loginButton_.setButtonText(lm.getText("login.button_login"));
        switchModeButton_.setButtonText(lm.getText("login.button_switch_to_signup"));
        googleButton_.setButtonText(lm.getText("login.button_google"));
        appleButton_.setButtonText(lm.getText("login.button_apple"));
        getStartedButton_.setButtonText(lm.getText("login.button_get_started"));

        // Tag ghost (secondary) buttons so the L&F draws them differently.
        switchModeButton_.getProperties().set("ghost", true);
        googleButton_.getProperties().set("ghost", true);
        appleButton_.getProperties().set("ghost", true);
        refreshButton_.getProperties().set("ghost", true);
        signOutButton_.getProperties().set("ghost", true);
        useDifferentVenueButton_.getProperties().set("ghost", true);
        getStartedButton_.getProperties().set("ghost", true);

        loginButton_.onClick      = [this] { handleEmailSubmit(); };
        switchModeButton_.onClick = [this] { isLoginMode_ = !isLoginMode_; refreshLoginPage(); };
        googleButton_.onClick     = [this] { handleOAuth("google.com"); };
        appleButton_.onClick      = [this] { handleOAuth("apple.com");  };
        getStartedButton_.onClick = [this] { launchOnboardingWizard(OnboardingWizard::StartStep::CreateAccount); };

        if (devSkipLoginEnabled_)
        {
            addChildComponent(skipButton_);
            skipButton_.setButtonText(lm.getText("login.dev_skip_button"));
            skipButton_.getProperties().set("ghost", true);
            skipButton_.onClick = [this]
            {
                // Auto-fill canonical dev credentials, mark the venue we want
                // post-auth, and reuse the normal sign-in path.
                isLoginMode_ = true;
                emailEditor_.setText("raymac@shaw.ca", juce::dontSendNotification);
                passwordEditor_.setText("123456",      juce::dontSendNotification);
                autoPickVenueName_ = "Karaoke Palace";
                handleEmailSubmit();
            };
        }

        // ── Multi-page widgets (created up-front, hidden by default) ─────────
        // SelectVenue: heading + remember-toggle + scrolling venue cards
        addChildComponent(venuesHeadingLabel_);
        venuesHeadingLabel_.setText(lm.getText("login.select_venue.available_venues_heading"), juce::dontSendNotification);
        venuesHeadingLabel_.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
        venuesHeadingLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff1f2937));
        venuesHeadingLabel_.setJustificationType(juce::Justification::centredLeft);

        addChildComponent(rememberVenueToggle_);
        rememberVenueToggle_.setButtonText(lm.getText("login.select_venue.remember_toggle"));
        rememberVenueToggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff374151));
        rememberVenueToggle_.setColour(juce::ToggleButton::tickColourId, juce::Colour(LoginTheme::kAccentBlue));
        rememberVenueToggle_.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff9ca3af));

        addChildComponent(venuesViewport_);
        venuesViewport_.setViewedComponent(&venuesContainer_, false);
        venuesViewport_.setScrollBarsShown(true, false);
        venuesViewport_.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0x66000000));

        // AwaitingInvitation
        addChildComponent(invitationsListBox_);
        invitationsListBox_.setModel(&invitationsListModel_);
        invitationsListBox_.setRowHeight(58);
        invitationsListBox_.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);

        addChildComponent(refreshButton_);
        addChildComponent(signOutButton_);
        addChildComponent(createVenueButton_);
        addChildComponent(requestAccessButton_);
        addChildComponent(useDifferentVenueButton_);
        addChildComponent(messageEditor_);

        styleEditor(messageEditor_, lm.getText("login.request_access.message_placeholder"));
        messageEditor_.setMultiLine(true);
        messageEditor_.setReturnKeyStartsNewLine(true);

        refreshButton_.setButtonText(lm.getText("login.awaiting.button_refresh"));
        signOutButton_.setButtonText(lm.getText("login.shared.button_sign_out"));
        createVenueButton_.setButtonText(lm.getText("login.awaiting.button_create_venue"));
        requestAccessButton_.setButtonText(lm.getText("login.request_access.button_send"));
        useDifferentVenueButton_.setButtonText(lm.getText("login.request_access.button_use_different_venue"));

        refreshButton_.onClick = [this]
        {
            if (page_ == Page::AwaitingInvitation)
                runFlow(); // re-fetch invitations
        };

        signOutButton_.onClick = [this]
        {
            FirestoreClient::getInstance().signOut();
            page_ = Page::Login;
            applyPage();
        };

        createVenueButton_.onClick = [this]
        {
            // Already signed in with a Host doc (this button only shows
            // post-auth) — skip straight to the account-type chooser rather
            // than re-collecting name/stage/avatar.
            launchOnboardingWizard(OnboardingWizard::StartStep::AccountType);
        };

        requestAccessButton_.onClick = [this] { handleRequestAccess(); };

        useDifferentVenueButton_.onClick = [this]
        {
            UserPreferences::getInstance().setVenueId({});
            runFlow();
        };

        venueListModelOnSelected_ = [this](const juce::String& venueId)
        {
            const auto configuredId = flowResult_.configuredVenueId;
            const bool isSwitch = configuredId.isNotEmpty() && configuredId != venueId;

            auto proceed = [this, venueId](bool initialScan)
            {
                setBusy(true, LocalizationManager::getInstance().getText("login.select_venue.status_opening_venue"));

                // selectVenue() checks the license before persisting/opening —
                // must resolve before we ever hand off to onComplete_, or an
                // invalid license could be bypassed.
                LoginFlowController::selectVenue(venueId, [this, venueId, initialScan](bool ok, juce::String licenseMessage)
                {
                    setBusy(false, {});
                    if (ok)
                    {
                        // This is the multi-association picker path -- the
                        // device-confirmation screen is specifically for the
                        // single-association auto-load case (Idea #1), so
                        // only the live-elsewhere check applies here, not the
                        // "you're logging into venue X" confirmation.
                        checkLiveElsewhereThenComplete(venueId, initialScan);
                    }
                    else
                    {
                        showLicenseInvalidAndSignOut(licenseMessage);
                    }
                });
            };

            if (isSwitch)
            {
                auto& lmRef = LocalizationManager::getInstance();
                juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    lmRef.getText("login.select_venue.switch_dialog_title"),
                    lmRef.getText("login.select_venue.switch_dialog_body"),
                    lmRef.getText("login.select_venue.switch_dialog_confirm"),
                    lmRef.getText("login.select_venue.switch_dialog_cancel"),
                    nullptr,
                    juce::ModalCallbackFunction::create(
                        [proceed](int result)
                        {
                            // result == 1 → "Yes", 0 → "Cancel".
                            if (result == 1)
                                proceed(true);
                            // Cancel: stay on the picker, do nothing.
                        }));
                return;
            }

            proceed(false);
        };

        invitationsListModel_.onAccept = [this](int row)
        {
            juce::ignoreUnused(row);
            statusLabel_.setText(
                LocalizationManager::getInstance().getText("login.awaiting.status_accept_not_implemented"),
                juce::dontSendNotification);
        };

        applyPage();
        setSize(720, 820);
    }

    ~LoginContent() override
    {
        setLookAndFeel(nullptr);
    }

    //==============================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Top-to-bottom blue → black gradient.
        juce::ColourGradient bg(juce::Colour(0xff4272b8), bounds.getCentreX(), 0.0f,
                                juce::Colours::black,    bounds.getCentreX(), bounds.getBottom(),
                                false);
        g.setGradientFill(bg);
        g.fillRect(bounds);

        // Logo at the top of the screen.
        if (logoImage_.isValid() && ! logoBounds_.isEmpty())
        {
            g.drawImageWithin(logoImage_,
                              logoBounds_.getX(), logoBounds_.getY(),
                              logoBounds_.getWidth(), logoBounds_.getHeight(),
                              juce::RectanglePlacement::centred
                            | juce::RectanglePlacement::onlyReduceInSize);
        }

        // Translucent / white card behind the form fields.
        if (! cardBounds_.isEmpty())
        {
            const float r = 18.0f;

            // Soft drop-shadow under the card.
            juce::DropShadow cardShadow(juce::Colours::black.withAlpha(0.30f), 32,
                                        juce::Point<int>(0, 8));
            juce::Path p;
            p.addRoundedRectangle(cardBounds_.toFloat(), r);
            cardShadow.drawForPath(g, p);

            const bool sel = page_ == Page::SelectVenue;
            if (sel)
            {
                g.setColour(juce::Colours::white);
                g.fillRoundedRectangle(cardBounds_.toFloat(), r);
            }
            else
            {
                g.setColour(juce::Colour(LoginTheme::kCardFill));
                g.fillRoundedRectangle(cardBounds_.toFloat(), r);

                g.setColour(juce::Colour(LoginTheme::kCardBorder));
                g.drawRoundedRectangle(cardBounds_.toFloat().reduced(0.5f), r, 1.0f);
            }
        }
    }

    void resized() override
    {
        const auto full = getLocalBounds();
        const bool sel = page_ == Page::SelectVenue;

        if (sel)
        {
            // SelectVenue uses a different layout: large heading + subtitle
            // sit on the gradient, with a wide opaque-white card below.
            auto top = full.reduced(40, 24);
            auto headerArea = top.removeFromTop(56);
            headingLabel_.setBounds(headerArea);
            headingLabel_.setJustificationType(juce::Justification::centred);
            headingLabel_.setFont(juce::Font(juce::FontOptions(32.0f, juce::Font::bold)));

            auto subArea = top.removeFromTop(28);
            statusLabel_.setBounds(subArea);
            statusLabel_.setFont(juce::Font(juce::FontOptions(14.0f)));

            // Don't show the small logo on this page (heading is the focus).
            logoBounds_ = {};
            versionLabel_.setBounds(top.removeFromTop(22));

            // White card.
            const int cardW = juce::jmin(820, full.getWidth() - 80);
            const int cardX = full.getCentreX() - cardW / 2;
            const int cardY = subArea.getBottom() + 24;
            const int cardH = full.getHeight() - cardY - 32;
            cardBounds_ = { cardX, cardY, cardW, cardH };

            auto inner = cardBounds_.reduced(36, 32);
            layoutSelectVenuePage(inner);
            return;
        }

        // Login / AwaitingInvitation / RequestAccess: logo at top, version
        // beneath, then a translucent card with the form.
        const int logoH = juce::jmin(128, full.getHeight() / 6);
        const int logoW = juce::jmin(420, full.getWidth() - 80);
        logoBounds_ = juce::Rectangle<int>(0, 0, logoW, logoH)
                          .withCentre({ full.getCentreX(), 32 + logoH / 2 });

        versionLabel_.setBounds(logoBounds_.getX(),
                                logoBounds_.getBottom() + 4,
                                logoBounds_.getWidth(),
                                22);

        const int cardW = juce::jmin(560, full.getWidth() - 64);
        const int cardX = full.getCentreX() - cardW / 2;
        const int cardY = versionLabel_.getBottom() + 18;
        const int cardH = juce::jmax(360, full.getHeight() - cardY - 32);
        cardBounds_ = { cardX, cardY, cardW, cardH };

        auto inner = cardBounds_.reduced(36, 32);

        headingLabel_.setBounds(inner.removeFromTop(40));
        headingLabel_.setJustificationType(juce::Justification::centredLeft);
        headingLabel_.setFont(juce::Font(juce::FontOptions(28.0f)).withStyle(juce::Font::plain));

        // Dev-only shortcut sits beside the "Login" heading rather than
        // stacked at the bottom of the form, where it used to overlap
        // "New to Encore? Get Started" once the form grew past the card's
        // fixed height.
        if (devSkipLoginEnabled_ && page_ == Page::Login)
        {
            auto headingArea = headingLabel_.getBounds();
            skipButton_.setBounds(headingArea.removeFromRight(170).reduced(0, 4));
            headingLabel_.setBounds(headingArea);
        }

        inner.removeFromTop(8);
        statusLabel_.setBounds(inner.removeFromTop(28));
        statusLabel_.setJustificationType(juce::Justification::centred);
        statusLabel_.setFont(juce::Font(juce::FontOptions(13.0f)));
        inner.removeFromTop(6);

        auto spinnerArea = inner.removeFromTop(busySpinner_.isVisible() ? 24 : 0);
        busySpinner_.setBounds(spinnerArea);

        switch (page_)
        {
            case Page::Login:              layoutLoginPage(inner);              break;
            case Page::SelectVenue:        layoutSelectVenuePage(inner);        break;
            case Page::AwaitingInvitation: layoutAwaitingInvitationPage(inner); break;
            case Page::RequestAccess:      layoutRequestAccessPage(inner);      break;
        }
    }

private:
    enum class Page { Login, SelectVenue, AwaitingInvitation, RequestAccess };

    //==============================================================================
    void styleEditor(juce::TextEditor& te, const juce::String& placeholder)
    {
        te.setIndents(18, 14);
        te.setBorder(juce::BorderSize<int>(0));
        te.setFont(juce::Font(juce::FontOptions(16.0f)));
        te.setColour(juce::TextEditor::backgroundColourId,    juce::Colour(LoginTheme::kInputFill));
        te.setColour(juce::TextEditor::outlineColourId,       juce::Colour(LoginTheme::kInputBorder));
        te.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(LoginTheme::kInputBorderFocus));
        te.setColour(juce::TextEditor::textColourId,          juce::Colours::white);
        te.setTextToShowWhenEmpty(placeholder, juce::Colour(LoginTheme::kPlaceholder));
    }

    //==============================================================================
    // Layout helpers
    void layoutLoginPage(juce::Rectangle<int> area)
    {
        // Big breathable inputs to match the original.
        area.removeFromTop(16);
        emailEditor_.setBounds(area.removeFromTop(54));
        area.removeFromTop(16);
        passwordEditor_.setBounds(area.removeFromTop(54));

        area.removeFromTop(22);

        // Centred Login button.
        loginButton_.setBounds(area.removeFromTop(48).withSizeKeepingCentre(180, 48));

        area.removeFromTop(10);

        // Centred Switch button (ghost).
        switchModeButton_.setBounds(area.removeFromTop(44).withSizeKeepingCentre(220, 44));

        // OAuth buttons — always visible.
        area.removeFromTop(20);

        // Faint "or" divider.
        // (Drawn by the buttons themselves; just a small spacer.)
        googleButton_.setBounds(area.removeFromTop(44).withSizeKeepingCentre(300, 44));
        area.removeFromTop(10);
        appleButton_.setBounds(area.removeFromTop(44).withSizeKeepingCentre(300, 44));

        area.removeFromTop(16);
        getStartedButton_.setBounds(area.removeFromTop(40).withSizeKeepingCentre(280, 40));
    }

    void layoutSelectVenuePage(juce::Rectangle<int> area)
    {
        venuesHeadingLabel_.setBounds(area.removeFromTop(32));
        area.removeFromTop(12);
        rememberVenueToggle_.setBounds(area.removeFromTop(28));
        area.removeFromTop(12);

        auto bottom = area.removeFromBottom(48);
        // Sign Out (ghost) bottom-right, on the white card → restyle to dark text.
        signOutButton_.setBounds(bottom.removeFromRight(140).reduced(0, 6));

        venuesViewport_.setBounds(area);

        // Lay out the inner cards stacked vertically.
        const int viewW = venuesViewport_.getMaximumVisibleWidth();
        const int cardH = 110;
        const int gap = 12;
        int y = 0;
        for (auto& c : venueCards_)
        {
            c->setBounds(0, y, viewW, cardH);
            y += cardH + gap;
        }
        venuesContainer_.setBounds(0, 0, viewW, juce::jmax(area.getHeight(), y));
    }

    void layoutAwaitingInvitationPage(juce::Rectangle<int> area)
    {
        auto bottom = area.removeFromBottom(48);
        signOutButton_.setBounds(bottom.removeFromRight(140));
        bottom.removeFromRight(8);
        if (createVenueButton_.isVisible())
            createVenueButton_.setBounds(bottom.removeFromRight(190));
        bottom.removeFromRight(8);
        refreshButton_.setBounds(bottom.removeFromRight(120));

        invitationsListBox_.setBounds(area.reduced(0, 8));
    }

    void layoutRequestAccessPage(juce::Rectangle<int> area)
    {
        auto bottom = area.removeFromBottom(48);
        signOutButton_.setBounds(bottom.removeFromRight(140));
        bottom.removeFromRight(8);
        useDifferentVenueButton_.setBounds(bottom.removeFromRight(200));
        bottom.removeFromRight(8);
        requestAccessButton_.setBounds(bottom.removeFromRight(200));

        area.removeFromTop(8);
        messageEditor_.setBounds(area.reduced(0, 8));
    }

    //==============================================================================
    void applyPage()
    {
        auto& lm = LocalizationManager::getInstance();
        const bool login  = page_ == Page::Login;
        const bool sel    = page_ == Page::SelectVenue;
        const bool await_ = page_ == Page::AwaitingInvitation;
        const bool req    = page_ == Page::RequestAccess;

        emailEditor_.setVisible(login);
        passwordEditor_.setVisible(login);
        loginButton_.setVisible(login);
        switchModeButton_.setVisible(login);
        googleButton_.setVisible(login);
        appleButton_.setVisible(login);
        getStartedButton_.setVisible(login);
        if (devSkipLoginEnabled_)
            skipButton_.setVisible(login);

        venuesHeadingLabel_.setVisible(sel);
        rememberVenueToggle_.setVisible(sel);
        venuesViewport_.setVisible(sel);
        invitationsListBox_.setVisible(await_);
        refreshButton_.setVisible(await_);
        createVenueButton_.setVisible(await_ && (flowResult_.canCreateVenue || flowResult_.offerSelfServeSetup));
        createVenueButton_.setButtonText(flowResult_.offerSelfServeSetup ? lm.getText("login.awaiting.button_get_started_alt")
                                                                          : lm.getText("login.awaiting.button_create_venue"));
        signOutButton_.setVisible(sel || await_ || req);
        messageEditor_.setVisible(req);
        requestAccessButton_.setVisible(req);
        useDifferentVenueButton_.setVisible(req);

        // The version label and the small status label live above/below the
        // logo on every page.
        versionLabel_.setVisible(login || sel || await_ || req);

        switch (page_)
        {
            case Page::Login:
                refreshLoginPage();
                headingLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
                statusLabel_.setColour(juce::Label::textColourId,
                                       juce::Colour(LoginTheme::kSubtleText));
                break;

            case Page::SelectVenue:
                headingLabel_.setText(lm.getText("login.select_venue.heading"), juce::dontSendNotification);
                headingLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
                statusLabel_.setText(
                    lm.getText("login.select_venue.subtitle"),
                    juce::dontSendNotification);
                statusLabel_.setColour(juce::Label::textColourId,
                                       juce::Colour(LoginTheme::kSubtleText));
                rebuildVenueCards();

                // Pre-tick "remember" if the configured venue is in the list.
                rememberVenueToggle_.setToggleState(
                    flowResult_.configuredVenueId.isNotEmpty(),
                    juce::dontSendNotification);
                break;

            case Page::AwaitingInvitation:
                headingLabel_.setText(lm.getText("login.awaiting.heading"), juce::dontSendNotification);
                headingLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
                statusLabel_.setText(
                    flowResult_.invitations.empty()
                        ? lm.getText("login.awaiting.status_no_invitations")
                        : lm.getText("login.awaiting.status_has_invitations"),
                    juce::dontSendNotification);
                invitationsListBox_.updateContent();
                break;

            case Page::RequestAccess:
                headingLabel_.setText(lm.getText("login.request_access.heading"), juce::dontSendNotification);
                headingLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
                statusLabel_.setText(
                    lm.getText("login.request_access.subtitle"),
                    juce::dontSendNotification);
                break;
        }

        invitationsListModel_.invitations = &flowResult_.invitations;

        resized();
        repaint();
    }

    //==============================================================================
    void rebuildVenueCards()
    {
        venueCards_.clear();
        for (const auto& a : flowResult_.associations)
        {
            const bool configured = a.venueId == flowResult_.configuredVenueId;
            auto card = std::make_unique<VenueCardComponent>(a, configured);
            const auto venueId = a.venueId;
            card->onSelect = [this, venueId] { if (venueListModelOnSelected_) venueListModelOnSelected_(venueId); };
            venuesContainer_.addAndMakeVisible(card.get());
            venueCards_.push_back(std::move(card));
        }
    }

    void refreshLoginPage()
    {
        auto& lm = LocalizationManager::getInstance();
        loginButton_.setButtonText(isLoginMode_ ? lm.getText("login.button_login") : lm.getText("login.button_signup"));
        switchModeButton_.setButtonText(isLoginMode_ ? lm.getText("login.button_switch_to_signup") : lm.getText("login.button_switch_to_login"));
        headingLabel_.setText(isLoginMode_ ? lm.getText("login.heading_login") : lm.getText("login.heading_signup"),
                              juce::dontSendNotification);
    }

    void setBusy(bool busy, const juce::String& message)
    {
        busySpinner_.setVisible(busy);
        statusLabel_.setText(message, juce::dontSendNotification);

        loginButton_.setEnabled(!busy);
        switchModeButton_.setEnabled(!busy);
        googleButton_.setEnabled(!busy);
        appleButton_.setEnabled(!busy);
        getStartedButton_.setEnabled(!busy);
        refreshButton_.setEnabled(!busy);
        signOutButton_.setEnabled(!busy);
        createVenueButton_.setEnabled(!busy);
        requestAccessButton_.setEnabled(!busy);
        useDifferentVenueButton_.setEnabled(!busy);

        resized();
    }

    //==============================================================================
    // Sign-in handlers (run on a background thread to avoid blocking the UI)
    void handleEmailSubmit()
    {
        const auto email    = emailEditor_.getText().trim();
        const auto password = passwordEditor_.getText();

        auto& lm = LocalizationManager::getInstance();
        if (email.isEmpty() || password.length() < 6)
        {
            statusLabel_.setText(lm.getText("login.error_invalid_email_password"),
                                 juce::dontSendNotification);
            return;
        }

        const bool signUp = ! isLoginMode_;
        setBusy(true, signUp ? lm.getText("login.status_creating_account") : lm.getText("login.status_signing_in"));

        juce::Component::SafePointer<LoginContent> safe(this);
        juce::Thread::launch([safe, email, password, signUp]
        {
            auto& fc = FirestoreClient::getInstance();
            auto result = signUp ? fc.signUpWithEmailPassword(email, password)
                                 : fc.signInWithEmailPassword(email, password);

            juce::MessageManager::callAsync([safe, result]()
            {
                if (safe == nullptr)
                    return;

                if (! result.ok)
                {
                    safe->setBusy(false, {});
                    safe->statusLabel_.setText(safe->formatAuthError(result.errorMessage),
                                         juce::dontSendNotification);
                    return;
                }
                safe->runFlow();
            });
        });
    }

    void handleOAuth(const juce::String& providerId)
    {
        setBusy(true, LocalizationManager::getInstance().getText("login.status_opening_browser"));
        juce::Component::SafePointer<LoginContent> safe(this);
        juce::Thread::launch([safe, providerId]
        {
            auto& fc = FirestoreClient::getInstance();
            auto result = fc.signInWithOAuthProvider(providerId);

            juce::MessageManager::callAsync([safe, result]()
            {
                if (safe == nullptr)
                    return;

                safe->setBusy(false, {});
                if (! result.ok)
                {
                    safe->statusLabel_.setText(result.errorMessage, juce::dontSendNotification);
                    return;
                }
                safe->runFlow();
            });
        });
    }

    juce::String formatAuthError(const juce::String& code)
    {
        // Identity Toolkit returns codes like "EMAIL_NOT_FOUND",
        // "INVALID_PASSWORD", "EMAIL_EXISTS", "WEAK_PASSWORD : Password should be at least 6 characters".
        auto& lm = LocalizationManager::getInstance();
        if (code.contains("NO_CONNECTION"))       return lm.getText("login.error_no_connection");
        if (code.contains("EMAIL_NOT_FOUND"))     return lm.getText("login.error_user_not_found");
        if (code.contains("INVALID_PASSWORD"))    return lm.getText("login.error_password_incorrect");
        if (code.contains("INVALID_LOGIN_CREDENTIALS")) return lm.getText("login.error_invalid_credentials");
        if (code.contains("EMAIL_EXISTS"))        return lm.getText("login.error_email_exists");
        if (code.contains("WEAK_PASSWORD"))       return lm.getText("login.error_weak_password");
        if (code.contains("USER_DISABLED"))       return lm.getText("login.error_user_disabled");
        if (code.contains("INVALID_EMAIL"))       return lm.getText("login.error_invalid_email");
        return code.isEmpty() ? lm.getText("login.error_signin_failed_generic") : code;
    }

    //==============================================================================
    // Scenario flow
    void runFlow()
    {
        setBusy(true, LocalizationManager::getInstance().getText("login.shared.status_loading_profile"));
        LoginFlowController::runPostAuthFlow(
            [this](LoginFlowController::Result result)
            {
                flowResult_ = std::move(result);
                setBusy(false, {});

                using O = LoginFlowController::Outcome;

                // Dev skip path: if a target venue name was set by the Skip
                // button, find it in the associations list (or accept the
                // already-resolved single venue) and short-circuit the picker.
                // autoPickVenueName_ can only be non-empty if devSkipLoginEnabled_
                // was true and the button was actually clicked, so no separate
                // runtime guard is needed here.
                if (autoPickVenueName_.isNotEmpty())
                {
                    juce::String pickId;
                    if (flowResult_.outcome == O::VenueLoaded)
                    {
                        pickId = flowResult_.venueId;
                    }
                    else if (flowResult_.outcome == O::PickVenue)
                    {
                        for (const auto& a : flowResult_.associations)
                        {
                            if (a.venueName.equalsIgnoreCase(autoPickVenueName_))
                            {
                                pickId = a.venueId;
                                break;
                            }
                        }
                    }

                    autoPickVenueName_.clear();

                    if (pickId.isNotEmpty())
                    {
                        setBusy(true, LocalizationManager::getInstance().getText("login.shared.status_loading_venue"));
                        LoginFlowController::selectVenue(pickId, [this, pickId](bool ok, juce::String licenseMessage)
                        {
                            setBusy(false, {});
                            if (ok)
                            {
                                if (onComplete_) onComplete_(pickId, false);
                            }
                            else
                            {
                                showLicenseInvalidAndSignOut(licenseMessage);
                            }
                        });
                        return;
                    }
                    // Fall through to the normal picker if we couldn't find it.
                }

                switch (flowResult_.outcome)
                {
                    case O::VenueLoaded:
                        proceedToVenueAfterChecks(flowResult_.venueId, flowResult_.venueName, false);
                        return;
                    case O::PickVenue:
                        page_ = Page::SelectVenue;
                        break;
                    case O::AwaitInvitation:
                        page_ = Page::AwaitingInvitation;
                        break;
                    case O::RequestAccess:
                        page_ = Page::RequestAccess;
                        break;
                    case O::VenueLicenseInvalid:
                        showLicenseInvalidAndSignOut(flowResult_.licenseMessage);
                        return;
                }
                applyPage();
            },
            [this](juce::String error)
            {
                setBusy(false, {});
                statusLabel_.setText(LocalizationManager::getInstance().getTextWithParams("login.shared.error_prefix", { error }),
                                     juce::dontSendNotification);
            });
    }

    // Blocks entry to a venue whose license is invalid/expired — no bypass,
    // signs the account out and returns to the login page so nothing (not
    // even a cached local venueId) can quietly slip past the check.
    void showLicenseInvalidAndSignOut(const juce::String& message)
    {
        setBusy(false, {});
        auto& lm = LocalizationManager::getInstance();
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            lm.getText("login.alert.venue_unavailable_title"),
            message.isNotEmpty() ? message
                                 : lm.getText("login.alert.venue_license_invalid_body"));

        FirestoreClient::getInstance().signOut();
        page_ = Page::Login;
        applyPage();
    }

    // Single hand-off point for BOTH places a venue can actually be opened
    // (the single-association auto-load in runFlow(), and the multi-venue
    // picker's venueListModelOnSelected_) -- runs the two collision-
    // prevention checks in sequence, then calls onComplete_ exactly like
    // both call sites used to do directly. Deliberately NOT wired into the
    // ENCORE_DEV_SKIP_LOGIN shortcut above, which exists specifically to
    // bypass friction during development.
    //
    // 1) New-device confirmation ("you're logging into venue {name}"),
    //    shown once per venue per PC (UserPreferences tracks which venues
    //    this install has already confirmed) -- Idea #1 from the plan.
    // 2) Live-elsewhere warning, checked every time (not remembered, since
    //    "is someone else live right now" is time-sensitive) -- Idea #2.
    void proceedToVenueAfterChecks(const juce::String& venueId, const juce::String& venueName, bool initialScan)
    {
        auto& prefs = UserPreferences::getInstance();
        if (! prefs.hasConfirmedVenueOnThisDevice(venueId))
        {
            auto& lm = LocalizationManager::getInstance();
            juce::AlertWindow::showOkCancelBox(
                juce::AlertWindow::InfoIcon,
                lm.getTextWithParams("login.confirm_venue.title", { venueName.isNotEmpty() ? venueName : venueId }),
                lm.getText("login.confirm_venue.body"),
                lm.getText("login.confirm_venue.btn_continue"),
                lm.getText("login.confirm_venue.btn_create_new"),
                nullptr,
                juce::ModalCallbackFunction::create([this, venueId, initialScan](int result)
                {
                    if (result == 1)
                    {
                        UserPreferences::getInstance().markVenueConfirmedOnThisDevice(venueId);
                        checkLiveElsewhereThenComplete(venueId, initialScan);
                    }
                    else
                    {
                        // "Create a New Venue" -- leaves the pending venue
                        // open/unconfirmed; the wizard's own completion re-runs
                        // runFlow(), which naturally re-resolves into whichever
                        // venue is now correct (see its existing comment above).
                        launchOnboardingWizard(OnboardingWizard::StartStep::AccountType);
                    }
                }));
            return;
        }

        checkLiveElsewhereThenComplete(venueId, initialScan);
    }

    void checkLiveElsewhereThenComplete(const juce::String& venueId, bool initialScan)
    {
        VenueSessionService::getInstance().checkForOtherActiveSessions(venueId,
            [this, venueId, initialScan](bool otherActive, juce::String otherDeviceLabel, juce::String /*error*/)
            {
                if (! otherActive)
                {
                    if (onComplete_) onComplete_(venueId, initialScan);
                    return;
                }

                auto& lm = LocalizationManager::getInstance();
                const auto label = otherDeviceLabel.isNotEmpty()
                    ? otherDeviceLabel
                    : lm.getText("login.live_elsewhere.unknown_device");

                juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    lm.getText("login.live_elsewhere.title"),
                    lm.getTextWithParams("login.live_elsewhere.body", { label }),
                    lm.getText("login.live_elsewhere.btn_open_anyway"),
                    lm.getText("login.live_elsewhere.btn_cancel"),
                    nullptr,
                    juce::ModalCallbackFunction::create([this, venueId, initialScan](int result)
                    {
                        if (result == 1 && onComplete_)
                            onComplete_(venueId, initialScan);
                        // Cancel: stay on the current page, do nothing.
                    }));
            });
    }

    // Launches the first-run onboarding wizard. `CreateAccount` is the
    // pre-auth entry (brand-new user, no Firebase account yet — the wizard's
    // own first step performs the sign-up); `AccountType` is the post-auth
    // entry (already signed in via the form above, zero venues/invitations —
    // skips straight to picking single-venue / multi-venue / company tier).
    // On completion the wizard has already created everything it needs in
    // Firestore, so re-running runFlow() naturally resolves straight into
    // the new venue via the normal single-association VenueLoaded path.
    void launchOnboardingWizard(OnboardingWizard::StartStep startStep)
    {
        OnboardingWizard::launch(this, startStep, [this]()
        {
            runFlow();
        });
    }

    void handleRequestAccess()
    {
        if (flowResult_.venueId.isEmpty())
            return;

        setBusy(true, LocalizationManager::getInstance().getText("login.request_access.status_sending"));

        // Look up venue name (best effort) for the request payload.
        juce::Component::SafePointer<LoginContent> safe(this);
        juce::Thread::launch([safe, venueId = flowResult_.venueId, msg = messageEditor_.getText()]()
        {
            auto doc = FirestoreClient::getInstance().getDocument("venues/" + venueId);
            const auto venueName = FirestoreClient::readString(doc, "name");

            LoginFlowController::requestVenueAccess(venueId, venueName, msg,
                [safe](bool ok, juce::String error)
                {
                    if (safe == nullptr)
                        return;

                    safe->setBusy(false, {});
                    auto& lmRef = LocalizationManager::getInstance();
                    safe->statusLabel_.setText(
                        ok ? lmRef.getText("login.request_access.status_sent")
                           : lmRef.getTextWithParams("login.request_access.status_failed", { error }),
                        juce::dontSendNotification);

                    safe->requestAccessButton_.setEnabled(! ok);
                });
        });
    }

    //==============================================================================
    // VenueCardComponent — single venue row in the picker (matches the
    // mock-up: light-blue tinted background for the "configured" venue,
    // logo, name + address + role + last-visited, green SELECT button.)
    class VenueCardComponent : public juce::Component
    {
    public:
        VenueCardComponent(const UserVenueAssociation& a, bool configured)
            : association_(a), isConfigured_(configured)
        {
            addAndMakeVisible(selectButton_);
            selectButton_.setButtonText(LocalizationManager::getInstance().getText("login.select_venue.card_select_button"));
            // Custom green button — bypass the LoginLookAndFeel gradient.
            selectButton_.getProperties().set("greenSelect", true);
            selectButton_.onClick = [this] { if (onSelect) onSelect(); };

            // Async: fetch full venue doc so we can show the street address
            // and logo (the lookup table only carries name + city).
            VenueService::getInstance().loadVenue(a.venueId,
                [safeThis = juce::Component::SafePointer<VenueCardComponent>(this)]
                (bool ok, VenueItem v, juce::String /*err*/)
                {
                    if (! ok || safeThis == nullptr) return;
                    safeThis->setVenueDetails(v);
                });
        }

        void setVenueDetails(const VenueItem& v)
        {
            address_ = juce::String(v.address);
            // Pull the cached city if we don't already have one.
            if (association_.venueName.isEmpty())
                association_.venueName = juce::String(v.name);
            const juce::String url(v.logoUrl);
            if (url.isNotEmpty())
            {
                juce::Component::SafePointer<VenueCardComponent> safeThis(this);
                auto img = ArtworkCache::getInstance().getOrFetch(url,
                    [safeThis, url]
                    {
                        if (safeThis == nullptr) return;
                        auto cached = ArtworkCache::getInstance().getOrFetch(url, nullptr);
                        if (cached.isValid())
                        {
                            safeThis->logo_ = cached;
                            safeThis->repaint();
                        }
                    });
                if (img.isValid())
                    logo_ = img;
            }
            repaint();
        }

        std::function<void()> onSelect;

        void paint(juce::Graphics& g) override
        {
            auto& lm = LocalizationManager::getInstance();
            const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
            const float r = 12.0f;

            // Card background: light blue if configured on this PC, otherwise
            // very pale grey/white.
            const auto fill = isConfigured_ ? juce::Colour(0xffe6f0fb)
                                            : juce::Colour(0xfff8fafc);
            g.setColour(fill);
            g.fillRoundedRectangle(bounds, r);

            // 1px border.
            g.setColour(juce::Colour(0xffd0d7e2));
            g.drawRoundedRectangle(bounds, r, 1.0f);

            const int pad = 16;
            const int logoSize = 56;

            // Logo / placeholder.
            const juce::Rectangle<int> logoArea(pad, pad, logoSize, logoSize);
            if (logo_.isValid())
            {
                g.drawImageWithin(logo_, logoArea.getX(), logoArea.getY(),
                                  logoArea.getWidth(), logoArea.getHeight(),
                                  juce::RectanglePlacement::centred
                                | juce::RectanglePlacement::onlyReduceInSize);
            }
            else
            {
                g.setColour(juce::Colour(0xffe2e8f0));
                g.fillRoundedRectangle(logoArea.toFloat(), 6.0f);
                g.setColour(juce::Colour(0xff94a3b8));
                g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
                g.drawText(association_.venueName.substring(0, 1).toUpperCase(),
                           logoArea, juce::Justification::centred);
            }

            // Text column.
            const int textX = pad + logoSize + 16;
            const int textW = getWidth() - textX - 140; // leave room for SELECT
            int y = pad - 2;

            g.setColour(juce::Colour(0xff1f2937));
            g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
            g.drawText(association_.venueName, textX, y, textW, 22,
                       juce::Justification::centredLeft, true);
            y += 22;

            g.setColour(juce::Colour(0xff4b5563));
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            if (address_.isNotEmpty())
            {
                g.drawText(address_, textX, y, textW, 16,
                           juce::Justification::centredLeft, true);
                y += 16;
            }

            // City – we don't carry it on UserVenueAssociation directly;
            // VenueService::getCurrent() would have it. Skip for now and
            // rely on the address line being enough.

            g.drawText(lm.getTextWithParams("login.select_venue.card_role_label",
                           { juce::String(AccessRightsUtil::userRoleToString(association_.role)) }),
                       textX, y, textW, 16, juce::Justification::centredLeft);
            y += 16;

            const juce::String last = association_.lastAccessDate.toMilliseconds() > 0
                                         ? association_.lastAccessDate.formatted("%b %d, %Y")
                                         : lm.getText("login.select_venue.card_invalid_date");
            g.drawText(lm.getTextWithParams("login.select_venue.card_last_visited", { last }), textX, y, textW, 16,
                       juce::Justification::centredLeft);
            y += 18;

            // "Configured on this PC" pill.
            if (isConfigured_)
            {
                const juce::String pillText = lm.getText("login.select_venue.card_configured_pill");
                auto font = juce::Font(juce::FontOptions(11.0f, juce::Font::bold));
                const int pillW = (int) juce::GlyphArrangement::getStringWidth(font, pillText) + 20;
                juce::Rectangle<float> pill((float) textX, (float) y, (float) pillW, 22.0f);
                g.setColour(juce::Colour(0xffd1fae5));
                g.fillRoundedRectangle(pill, 11.0f);
                g.setColour(juce::Colour(0xff047857));
                g.setFont(font);
                g.drawText(pillText, pill.toNearestInt(), juce::Justification::centred);
            }
        }

        void resized() override
        {
            const int w = 110, h = 36;
            selectButton_.setBounds(getWidth() - w - 16, (getHeight() - h) / 2, w, h);
        }

    private:
        UserVenueAssociation association_;
        bool isConfigured_;
        juce::Image logo_;
        juce::String address_;
        juce::TextButton selectButton_;
    };

    //==============================================================================
    // ListBox model for invitations (kept; AssociationListModel was removed).
    struct InvitationsListModel : public juce::ListBoxModel
    {
        std::vector<VenueInvitation>* invitations = nullptr;
        std::function<void(int)> onAccept;

        int getNumRows() override { return invitations ? (int) invitations->size() : 0; }

        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool isSelected) override
        {
            if (invitations == nullptr || row < 0 || row >= (int) invitations->size()) return;
            const auto& i = (*invitations)[(size_t) row];
            g.fillAll(isSelected ? juce::Colour(0x33ffffff) : juce::Colour(0x14ffffff));
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
            g.drawText(i.venueName, 12, 6, w - 100, 22, juce::Justification::centredLeft);
            g.setColour(juce::Colour(LoginTheme::kSubtleText));
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.drawText(LocalizationManager::getInstance().getTextWithParams("login.awaiting.invite_row_subtitle",
                           { i.invitedByName, juce::String(AccessRightsUtil::userRoleToString(i.role)) }),
                       12, 28, w - 100, 18, juce::Justification::centredLeft);

            // "Accept" hint button
            g.setColour(juce::Colour(0xffc69b3b));
            g.fillRoundedRectangle((float)(w - 90), 14.0f, 78.0f, 28.0f, 4.0f);
            g.setColour(juce::Colours::black);
            g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
            g.drawText(LocalizationManager::getInstance().getText("login.awaiting.invite_row_accept_button"),
                       w - 90, 14, 78, 28, juce::Justification::centred);
        }

        void listBoxItemClicked(int row, const juce::MouseEvent& e) override
        {
            // Only fire onAccept when the click hits the right-side pill.
            if (onAccept && e.x > 0)
                onAccept(row);
        }
    };

    //==============================================================================
    // State
    Page page_ = Page::Login;
    bool isLoginMode_ = true;
    LoginFlowController::Result flowResult_;
    LoginCompleteCallback onComplete_;

    LoginLookAndFeel lnf_;
    juce::Image logoImage_;
    juce::Rectangle<int> logoBounds_;
    juce::Rectangle<int> cardBounds_;

    // Common
    juce::Label versionLabel_;
    juce::Label headingLabel_;
    juce::Label statusLabel_;
    juce::Component busySpinner_; // simple placeholder; could be a real spinner

    // Login page
    juce::TextEditor emailEditor_;
    juce::TextEditor passwordEditor_;
    juce::TextButton loginButton_;
    juce::TextButton switchModeButton_;
    juce::TextButton getStartedButton_;
    juce::TextButton googleButton_;
    juce::TextButton appleButton_;

    // Dev-only "Skip Login" shortcut -- hidden unless launched with
    // --dev-skip-login (see isDevSkipLoginEnabled() above). Always declared
    // (cheap) so the rest of the class doesn't need conditional compilation.
    const bool       devSkipLoginEnabled_ = isDevSkipLoginEnabled();
    juce::TextButton skipButton_;
    // Set when the dev "Skip" button auto-fills credentials so the post-auth
    // flow knows to bypass the venue picker by name. Cleared after the pick.
    juce::String     autoPickVenueName_;

    // SelectVenue page (custom cards instead of ListBox)
    class VenueCardComponent;
    juce::Label             venuesHeadingLabel_;
    juce::ToggleButton      rememberVenueToggle_;
    juce::Viewport          venuesViewport_;
    juce::Component         venuesContainer_;
    std::vector<std::unique_ptr<VenueCardComponent>> venueCards_;
    std::function<void(const juce::String&)> venueListModelOnSelected_;

    // AwaitingInvitation / RequestAccess
    InvitationsListModel  invitationsListModel_;
    juce::ListBox          invitationsListBox_;

    juce::TextButton refreshButton_;
    juce::TextButton signOutButton_;
    juce::TextButton createVenueButton_;
    juce::TextButton requestAccessButton_;
    juce::TextButton useDifferentVenueButton_;
    juce::TextEditor messageEditor_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoginContent)
};

//==============================================================================
LoginWindow::LoginWindow(LoginCompleteCallback onComplete)
    : DocumentWindow(LocalizationManager::getInstance().getText("login.window_title"),
                     juce::Desktop::getInstance().getDefaultLookAndFeel()
                         .findColour(juce::ResizableWindow::backgroundColourId),
                     DocumentWindow::closeButton)
    , onComplete_(std::move(onComplete))
{
    setUsingNativeTitleBar(true);
    setResizable(false, false);

    auto* c = new LoginContent([this](juce::String venueId, bool requestInitialScan)
    {
        auto cb = std::move(onComplete_);
        onComplete_ = nullptr;
        if (cb) cb(venueId, requestInitialScan);
    });
    setContentOwned(c, true);
    centreWithSize(720, 820);
    setVisible(true);
}

LoginWindow::~LoginWindow() = default;

void LoginWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}
