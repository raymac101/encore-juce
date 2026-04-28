/*
  ==============================================================================

    LoginWindow.cpp

    Auth + venue selection UI. Implemented as a single Component (LoginContent)
    that swaps between four sub-views. Keeps the file-count low while still
    matching the page model from the design document.

  ==============================================================================
*/

#include "LoginWindow.h"
#include "../Auth/LoginFlowController.h"
#include "../Services/FirestoreClient.h"
#include "../Services/UserPreferences.h"
#include "../Services/VenueService.h"
#include "../Services/ImageCache.h"
#include "../Models/VenueItem.h"
#include "../Models/UserVenueAssociation.h"
#include "../Models/AccessRights.h"

//==============================================================================
// Brand-matched look-and-feel for the login screen. Mirrors the Angular
// auth.component.scss: translucent inputs with white border, primary button
// in the blue gradient, secondary button as a ghost outline.
namespace LoginTheme
{
    static constexpr uint32_t kAccentBlue       = 0xff4272b8;
    static constexpr uint32_t kAccentBlueLight  = 0xff5a8fd8;
    static constexpr uint32_t kInputFill        = 0x1affffff; // rgba(255,255,255,0.10)
    static constexpr uint32_t kInputFillFocus   = 0x26ffffff; // rgba(255,255,255,0.15)
    static constexpr uint32_t kInputBorder      = 0x33ffffff; // rgba(255,255,255,0.20)
    static constexpr uint32_t kInputBorderFocus = 0x66ffffff; // rgba(255,255,255,0.40)
    static constexpr uint32_t kCardFill         = 0x1affffff;
    static constexpr uint32_t kCardBorder       = 0x33ffffff;
    static constexpr uint32_t kPlaceholder      = 0x99ffffff; // rgba(255,255,255,0.60)
    static constexpr uint32_t kBodyText         = 0xffffffff;
    static constexpr uint32_t kSubtleText       = 0xe6ffffff; // rgba(255,255,255,0.90)
    static constexpr uint32_t kDividerText      = 0x80ffffff;
}

class LoginLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LoginLookAndFeel()
    {
        setColour(juce::TextEditor::backgroundColourId,    juce::Colour(LoginTheme::kInputFill));
        setColour(juce::TextEditor::textColourId,          juce::Colour(LoginTheme::kBodyText));
        setColour(juce::TextEditor::highlightColourId,     juce::Colour(LoginTheme::kAccentBlue).withAlpha(0.5f));
        setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextEditor::outlineColourId,       juce::Colour(LoginTheme::kInputBorder));
        setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(LoginTheme::kInputBorderFocus));
        setColour(juce::CaretComponent::caretColourId,     juce::Colours::white);

        setColour(juce::Label::textColourId,               juce::Colour(LoginTheme::kBodyText));

        setColour(juce::TextButton::buttonColourId,        juce::Colour(LoginTheme::kAccentBlue));
        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(LoginTheme::kAccentBlueLight));
        setColour(juce::TextButton::textColourOffId,       juce::Colours::white);
        setColour(juce::TextButton::textColourOnId,        juce::Colours::white);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int /*height*/) override
    {
        return juce::Font(juce::FontOptions(14.0f)).withStyle(juce::Font::plain);
    }

    juce::Font getLabelFont(juce::Label& l) override
    {
        return l.getFont();
    }

    // Translucent rounded-rect inputs to match `.form_input`.
    void fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                  juce::TextEditor& e) override
    {
        const float r = 10.0f;
        g.setColour(e.findColour(juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle(0.0f, 0.0f, (float) width, (float) height, r);
    }

    void drawTextEditorOutline(juce::Graphics& g, int width, int height,
                               juce::TextEditor& e) override
    {
        const float r = 10.0f;
        const auto colour = e.hasKeyboardFocus(true)
                                ? e.findColour(juce::TextEditor::focusedOutlineColourId)
                                : e.findColour(juce::TextEditor::outlineColourId);
        g.setColour(colour);
        g.drawRoundedRectangle(1.0f, 1.0f,
                               (float) width  - 2.0f,
                               (float) height - 2.0f,
                               r, 2.0f);
    }

    // Pill-shaped buttons. Primary buttons (button-primary) get the blue
    // gradient; ghost buttons (button-secondary) are translucent white with a
    // subtle border.
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& /*backgroundColour*/,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const float r = juce::jmin(12.0f, bounds.getHeight() * 0.5f);
        const bool ghost = button.getProperties().getWithDefault("ghost", false);
        const bool greenSelect = button.getProperties().getWithDefault("greenSelect", false);
        const bool enabled = button.isEnabled();

        if (! enabled)
        {
            g.setColour(juce::Colour(0x33ffffff));
            g.fillRoundedRectangle(bounds, r);
            return;
        }

        if (greenSelect)
        {
            // Solid green pill (matches the SELECT button in the mock-up).
            const auto base = juce::Colour(0xff10b981);
            g.setColour(shouldDrawButtonAsHighlighted ? base.brighter(0.10f) : base);
            g.fillRoundedRectangle(bounds, r);
            if (shouldDrawButtonAsDown)
            {
                g.setColour(juce::Colours::black.withAlpha(0.10f));
                g.fillRoundedRectangle(bounds, r);
            }
            return;
        }

        if (ghost)
        {
            const float a = shouldDrawButtonAsHighlighted ? 0.20f : 0.10f;
            g.setColour(juce::Colours::white.withAlpha(a));
            g.fillRoundedRectangle(bounds, r);

            g.setColour(juce::Colour(LoginTheme::kInputBorder));
            g.drawRoundedRectangle(bounds, r, 1.5f);
            return;
        }

        // Primary: vertical blue gradient.
        const auto top    = juce::Colour(LoginTheme::kAccentBlueLight);
        const auto bottom = juce::Colour(LoginTheme::kAccentBlue);

        juce::ColourGradient grad(
            shouldDrawButtonAsHighlighted ? top.brighter(0.05f) : top,
            bounds.getCentreX(), bounds.getY(),
            shouldDrawButtonAsHighlighted ? bottom.brighter(0.05f) : bottom,
            bounds.getCentreX(), bounds.getBottom(),
            false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, r);

        if (shouldDrawButtonAsDown)
        {
            g.setColour(juce::Colours::black.withAlpha(0.10f));
            g.fillRoundedRectangle(bounds, r);
        }
    }

    juce::Font getTextButtonFontForButton(juce::TextButton& b)
    {
        const bool greenSelect = b.getProperties().getWithDefault("greenSelect", false);
        return juce::Font(juce::FontOptions(greenSelect ? 13.0f : 14.0f))
                  .withStyle(greenSelect ? juce::Font::bold : juce::Font::plain);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*shouldDrawButtonAsHighlighted*/,
                        bool /*shouldDrawButtonAsDown*/) override
    {
        g.setFont(getTextButtonFontForButton(button));
        g.setColour(juce::Colours::white);
        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds().reduced(2),
                         juce::Justification::centred, 1);
    }
};

//==============================================================================
class LoginWindow::LoginContent : public juce::Component
{
public:
    explicit LoginContent(LoginCompleteCallback onComplete)
        : onComplete_(std::move(onComplete))
    {
        setLookAndFeel(&lnf_);

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
        if (auto* app = juce::JUCEApplication::getInstance())
            versionLabel_.setText("Version " + app->getApplicationVersion(),
                                  juce::dontSendNotification);

        addAndMakeVisible(headingLabel_);
        addAndMakeVisible(statusLabel_);
        addAndMakeVisible(busySpinner_);
        busySpinner_.setVisible(false);

        headingLabel_.setFont(juce::Font(juce::FontOptions(28.0f)).withStyle(juce::Font::plain));
        headingLabel_.setJustificationType(juce::Justification::centredLeft);
        headingLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
        headingLabel_.setText("Login", juce::dontSendNotification);

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

        styleEditor(emailEditor_, "E-Mail");
        styleEditor(passwordEditor_, "Password");
        passwordEditor_.setPasswordCharacter((juce::juce_wchar) 0x2022); // bullet

        loginButton_.setButtonText("Login");
        switchModeButton_.setButtonText("Switch to Sign Up");
        googleButton_.setButtonText("Sign in with Google");
        appleButton_.setButtonText("Sign in with Apple");

        // Tag ghost (secondary) buttons so the L&F draws them differently.
        switchModeButton_.getProperties().set("ghost", true);
        googleButton_.getProperties().set("ghost", true);
        appleButton_.getProperties().set("ghost", true);
        refreshButton_.getProperties().set("ghost", true);
        signOutButton_.getProperties().set("ghost", true);
        useDifferentVenueButton_.getProperties().set("ghost", true);

        loginButton_.onClick      = [this] { handleEmailSubmit(); };
        switchModeButton_.onClick = [this] { isLoginMode_ = !isLoginMode_; refreshLoginPage(); };
        googleButton_.onClick     = [this] { handleOAuth("google.com"); };
        appleButton_.onClick      = [this] { handleOAuth("apple.com");  };

        // ── Multi-page widgets (created up-front, hidden by default) ─────────
        // SelectVenue: heading + remember-toggle + scrolling venue cards
        addChildComponent(venuesHeadingLabel_);
        venuesHeadingLabel_.setText("Your Available Venues", juce::dontSendNotification);
        venuesHeadingLabel_.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
        venuesHeadingLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff1f2937));
        venuesHeadingLabel_.setJustificationType(juce::Justification::centredLeft);

        addChildComponent(rememberVenueToggle_);
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

        styleEditor(messageEditor_, "Optional message to the venue admin (e.g. 'I'm the new KJ for Friday nights')");
        messageEditor_.setMultiLine(true);
        messageEditor_.setReturnKeyStartsNewLine(true);

        refreshButton_.setButtonText("Refresh");
        signOutButton_.setButtonText("Sign Out");
        createVenueButton_.setButtonText("Create New Venue");
        requestAccessButton_.setButtonText("Send Access Request");
        useDifferentVenueButton_.setButtonText("Use a Different Venue");

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
            if (onComplete_) onComplete_({}, false);
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
                setBusy(true, "Loading venue...");
                // Persist as the new "configured on this PC" venue if the
                // host opted in, OR if they're explicitly switching venues
                // (the new one then becomes the configured one).
                if (rememberVenueToggle_.getToggleState() || initialScan)
                    UserPreferences::getInstance().setVenueId(venueId);

                LoginFlowController::selectVenue(venueId, [this, venueId, initialScan]
                {
                    setBusy(false, {});
                    if (onComplete_) onComplete_(venueId, initialScan);
                });
            };

            if (isSwitch)
            {
                juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    "Switch venue?",
                    "You are switching from the venue configured to this PC. "
                    "To do this you need to re-scan the music and generate a "
                    "new songbook. Do you want to continue?",
                    "Yes",
                    "Cancel",
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
                "Accepting invitations from JUCE isn't wired up yet - coming next.",
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
            versionLabel_.setBounds(0, 0, 0, 0);

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

        venuesHeadingLabel_.setVisible(sel);
        rememberVenueToggle_.setVisible(sel);
        venuesViewport_.setVisible(sel);
        invitationsListBox_.setVisible(await_);
        refreshButton_.setVisible(await_);
        createVenueButton_.setVisible(await_ && flowResult_.canCreateVenue);
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
                headingLabel_.setText("Select Your Venue", juce::dontSendNotification);
                headingLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
                statusLabel_.setText(
                    "You have access to multiple venues. Please select the one you'd like to use.",
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
                headingLabel_.setText("Welcome to Encore", juce::dontSendNotification);
                headingLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
                statusLabel_.setText(
                    flowResult_.invitations.empty()
                        ? "No invitations yet. Ask a venue admin to invite you, then click Refresh."
                        : "You have pending invitations.",
                    juce::dontSendNotification);
                invitationsListBox_.updateContent();
                break;

            case Page::RequestAccess:
                headingLabel_.setText("Request Access", juce::dontSendNotification);
                headingLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
                statusLabel_.setText(
                    "This computer is set for a venue you don't belong to yet.",
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
        loginButton_.setButtonText(isLoginMode_ ? "Login" : "Sign Up");
        switchModeButton_.setButtonText(isLoginMode_ ? "Switch to Sign Up" : "Switch to Login");
        headingLabel_.setText(isLoginMode_ ? "Login" : "Sign Up",
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

        if (email.isEmpty() || password.length() < 6)
        {
            statusLabel_.setText("Enter a valid email and a password (≥ 6 chars).",
                                 juce::dontSendNotification);
            return;
        }

        const bool signUp = ! isLoginMode_;
        setBusy(true, signUp ? "Creating your account..." : "Signing in...");

        juce::Thread::launch([this, email, password, signUp]
        {
            auto& fc = FirestoreClient::getInstance();
            auto result = signUp ? fc.signUpWithEmailPassword(email, password)
                                 : fc.signInWithEmailPassword(email, password);

            juce::MessageManager::callAsync([this, result]()
            {
                if (! result.ok)
                {
                    setBusy(false, {});
                    statusLabel_.setText(formatAuthError(result.errorMessage),
                                         juce::dontSendNotification);
                    return;
                }
                runFlow();
            });
        });
    }

    void handleOAuth(const juce::String& providerId)
    {
        setBusy(true, "Opening browser...");
        juce::Thread::launch([this, providerId]
        {
            auto& fc = FirestoreClient::getInstance();
            auto result = fc.signInWithOAuthProvider(providerId);

            juce::MessageManager::callAsync([this, result]()
            {
                setBusy(false, {});
                if (! result.ok)
                {
                    statusLabel_.setText(result.errorMessage, juce::dontSendNotification);
                    return;
                }
                runFlow();
            });
        });
    }

    juce::String formatAuthError(const juce::String& code)
    {
        // Identity Toolkit returns codes like "EMAIL_NOT_FOUND",
        // "INVALID_PASSWORD", "EMAIL_EXISTS", "WEAK_PASSWORD : Password should be at least 6 characters".
        if (code.contains("EMAIL_NOT_FOUND"))     return "User not found, please sign up.";
        if (code.contains("INVALID_PASSWORD"))    return "Password is incorrect.";
        if (code.contains("INVALID_LOGIN_CREDENTIALS")) return "Email or password is incorrect.";
        if (code.contains("EMAIL_EXISTS"))        return "This email is already registered.";
        if (code.contains("WEAK_PASSWORD"))       return "Password is too weak (min 6 chars).";
        if (code.contains("USER_DISABLED"))       return "This account has been disabled.";
        if (code.contains("INVALID_EMAIL"))       return "Not a valid email address.";
        return code.isEmpty() ? juce::String("Sign-in failed. Please try again.") : code;
    }

    //==============================================================================
    // Scenario flow
    void runFlow()
    {
        setBusy(true, "Loading your profile...");
        LoginFlowController::runPostAuthFlow(
            [this](LoginFlowController::Result result)
            {
                flowResult_ = std::move(result);
                setBusy(false, {});

                using O = LoginFlowController::Outcome;
                switch (flowResult_.outcome)
                {
                    case O::VenueLoaded:
                        if (onComplete_) onComplete_(flowResult_.venueId, false);
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
                }
                applyPage();
            },
            [this](juce::String error)
            {
                setBusy(false, {});
                statusLabel_.setText("Error: " + error, juce::dontSendNotification);
            });
    }

    void handleRequestAccess()
    {
        if (flowResult_.venueId.isEmpty())
            return;

        setBusy(true, "Sending request...");

        // Look up venue name (best effort) for the request payload.
        juce::Thread::launch([this, venueId = flowResult_.venueId, msg = messageEditor_.getText()]()
        {
            auto doc = FirestoreClient::getInstance().getDocument("venues/" + venueId);
            const auto venueName = FirestoreClient::readString(doc, "name");

            LoginFlowController::requestVenueAccess(venueId, venueName, msg,
                [this](bool ok, juce::String error)
                {
                    setBusy(false, {});
                    statusLabel_.setText(
                        ok ? "Request sent. The venue admin has been notified."
                           : juce::String("Failed: ") + error,
                        juce::dontSendNotification);

                    requestAccessButton_.setEnabled(! ok);
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
            selectButton_.setButtonText("SELECT");
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

            g.drawText("Role: " + juce::String(AccessRightsUtil::userRoleToString(association_.role)),
                       textX, y, textW, 16, juce::Justification::centredLeft);
            y += 16;

            const juce::String last = association_.lastAccessDate.toMilliseconds() > 0
                                         ? association_.lastAccessDate.formatted("%b %d, %Y")
                                         : "Invalid Date";
            g.drawText("Last visited: " + last, textX, y, textW, 16,
                       juce::Justification::centredLeft);
            y += 18;

            // "Configured on this PC" pill.
            if (isConfigured_)
            {
                const juce::String pillText = "Configured on this PC";
                auto font = juce::Font(juce::FontOptions(11.0f, juce::Font::bold));
                const int pillW = (int) font.getStringWidthFloat(pillText) + 20;
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
            g.drawText("Invited by " + i.invitedByName + " as "
                         + juce::String(AccessRightsUtil::userRoleToString(i.role)),
                       12, 28, w - 100, 18, juce::Justification::centredLeft);

            // "Accept" hint button
            g.setColour(juce::Colour(0xffc69b3b));
            g.fillRoundedRectangle((float)(w - 90), 14.0f, 78.0f, 28.0f, 4.0f);
            g.setColour(juce::Colours::black);
            g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
            g.drawText("Accept", w - 90, 14, 78, 28, juce::Justification::centred);
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
    juce::TextButton googleButton_;
    juce::TextButton appleButton_;

    // SelectVenue page (custom cards instead of ListBox)
    class VenueCardComponent;
    juce::Label             venuesHeadingLabel_;
    juce::ToggleButton      rememberVenueToggle_ { "Remember this venue on this PC" };
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
    : DocumentWindow("Encore - Sign In",
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
