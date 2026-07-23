/*
  ==============================================================================

    OnboardingWizard.cpp

  ==============================================================================
*/

#include "OnboardingWizard.h"
#include "../LoginTheme.h"
#include "../../Services/FirestoreClient.h"
#include "../../Services/VenueService.h"
#include "../../Services/CompanyService.h"
#include "../../Services/LicenseService.h"
#include "../../Services/InvitationService.h"
#include "../../Services/HostService.h"
#include "../../Services/LibraryScanner.h"
#include "../../Services/SongDatabase.h"
#include "../../Services/UserPreferences.h"
#include "../../Models/Host.h"
#include "../../Models/Company.h"
#include "../../Models/VenueItem.h"
#include "../../Models/UserVenueAssociation.h"
#include "../../Models/AccessRights.h"

namespace
{
    using FC = FirestoreClient;

    // Extra transparent margin reserved around the visible card so its
    // rounded corners, white border, and drop shadow have room to render
    // without being clipped by the (otherwise rectangular) OS window edge.
    constexpr int kShadowMargin = 24;
    constexpr int kCardCornerRadius = 16;

    constexpr int kWindowW = 760 + 2 * kShadowMargin;
    constexpr int kWindowH = 620 + 2 * kShadowMargin;

    void styleEditor(juce::TextEditor& te, const juce::String& placeholder)
    {
        te.setIndents(12, 10);
        te.setBorder(juce::BorderSize<int>(0));
        te.setFont(juce::Font(juce::FontOptions(15.0f)));
        te.setColour(juce::TextEditor::backgroundColourId,     juce::Colour(LoginTheme::kInputFill));
        te.setColour(juce::TextEditor::outlineColourId,        juce::Colour(LoginTheme::kInputBorder));
        te.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(LoginTheme::kInputBorderFocus));
        te.setColour(juce::TextEditor::textColourId,           juce::Colours::white);
        te.setTextToShowWhenEmpty(placeholder, juce::Colour(LoginTheme::kPlaceholder));
    }

    void styleHeading(juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        l.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kBodyText));
        l.setJustificationType(juce::Justification::centredLeft);
    }

    void styleSubheading(juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions(14.0f)));
        l.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kSubtleText));
        l.setJustificationType(juce::Justification::centredLeft);
    }

    void styleFieldLabel(juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions(13.0f)));
        l.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kBodyText));
    }

    void styleStatus(juce::Label& l)
    {
        l.setFont(juce::Font(juce::FontOptions(13.0f)));
        l.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kErrorText));
        l.setJustificationType(juce::Justification::centred);
    }

    juce::String generateShortId(int length)
    {
        static const char* charset = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // no ambiguous chars
        auto& rng = juce::Random::getSystemRandom();
        juce::String out;
        for (int i = 0; i < length; ++i)
            out += juce::String::charToString(charset[rng.nextInt(32)]);
        return out;
    }

    bool looksLikeEmail(const juce::String& s)
    {
        return s.contains("@") && s.contains(".") && ! s.startsWithChar('@') && ! s.endsWithChar('.');
    }
}

//==============================================================================
// A large tappable card used on the account-type step. Draws a simple
// vector icon (no bespoke asset files needed) + title + description.
class TierCard : public juce::Button
{
public:
    enum class Icon { SingleVenue, MultiVenue, Company };

    TierCard(Icon icon, juce::String title, juce::String description)
        : juce::Button({}), icon_(icon), title_(std::move(title)), description_(std::move(description))
    {
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        const auto base = juce::Colour(0xfff9fafb);
        g.setColour(highlighted ? base.darker(0.03f) : base);
        g.fillRoundedRectangle(bounds, 12.0f);
        g.setColour(down ? juce::Colour(0xff4272b8) : juce::Colour(0xffe5e7eb));
        g.drawRoundedRectangle(bounds, 12.0f, highlighted ? 2.0f : 1.5f);

        auto area = bounds.reduced(18.0f);
        auto iconArea = area.removeFromTop(56.0f);
        paintIcon(g, iconArea.withSizeKeepingCentre(48.0f, 48.0f));

        area.removeFromTop(12.0f);
        // This card keeps its own light background (matches the SelectVenue
        // page's "white card on gradient" pattern), so its text stays dark
        // -- unlike the rest of the wizard, which sits directly on the dark
        // page gradient and needs light text.
        g.setColour(juce::Colour(0xff1f2937));
        g.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
        auto titleArea = area.removeFromTop(24.0f);
        g.drawFittedText(title_, titleArea.toNearestInt(), juce::Justification::centred, 1);

        area.removeFromTop(6.0f);
        g.setColour(juce::Colour(0xff6b7280));
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawFittedText(description_, area.toNearestInt(), juce::Justification::centredTop, 4);
    }

private:
    void paintIcon(juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour(juce::Colour(0xff4272b8));
        switch (icon_)
        {
            case Icon::SingleVenue:
            {
                auto box = r.reduced(6.0f);
                g.drawRoundedRectangle(box, 4.0f, 2.5f);
                g.fillRect(box.getCentreX() - 3.0f, box.getBottom() - 14.0f, 6.0f, 14.0f);
                break;
            }
            case Icon::MultiVenue:
            {
                auto b1 = r.withWidth(r.getWidth() * 0.55f).withHeight(r.getHeight() * 0.7f).withY(r.getY() + r.getHeight() * 0.3f);
                auto b2 = r.withX(r.getX() + r.getWidth() * 0.4f).withWidth(r.getWidth() * 0.55f).withHeight(r.getHeight() * 0.85f).withY(r.getY() + r.getHeight() * 0.15f);
                g.drawRoundedRectangle(b1, 3.0f, 2.0f);
                g.drawRoundedRectangle(b2, 3.0f, 2.5f);
                break;
            }
            case Icon::Company:
            {
                const float cx = r.getCentreX(), cy = r.getCentreY();
                g.drawEllipse(r.reduced(4.0f), 2.5f);
                for (int i = 0; i < 3; ++i)
                {
                    const float ang = (float) i * juce::MathConstants<float>::twoPi / 3.0f - juce::MathConstants<float>::halfPi;
                    const float nx = cx + std::cos(ang) * r.getWidth() * 0.36f;
                    const float ny = cy + std::sin(ang) * r.getHeight() * 0.36f;
                    g.fillEllipse(nx - 4.0f, ny - 4.0f, 8.0f, 8.0f);
                }
                break;
            }
        }
    }

    Icon icon_;
    juce::String title_, description_;
};

//==============================================================================
// OnboardingWizard::Content — owns wizard-wide state and swaps a single
// child component per step (each step class below reports completion via a
// callback wired up in showStep()).
class OnboardingWizard::Content : public juce::Component
{
public:
    enum class Step
    {
        CreateAccount, JoinInvitedVenue, AccountType,
        CompanyInfo, VenueInfo, HostInvite, LibraryScan
    };

    Content(StartStep startStep, std::function<void(bool completed)> onFinished)
        : onFinished_(std::move(onFinished))
    {
        // Not opaque: the area outside the rounded card (kShadowMargin) must
        // stay genuinely transparent so the OS-composited window shows the
        // login screen behind it there, per the semi-transparent peer style
        // flag set in OnboardingWizard::launch().
        setOpaque(false);
        setLookAndFeel(&lnf_);
        addAndMakeVisible(cancelButton_);
        cancelButton_.setButtonText("Cancel");
        cancelButton_.getProperties().set("ghost", true);
        // Cancel must NOT be treated as a successful completion -- callers
        // (e.g. LoginWindow's runFlow()) assume "completed" means the wizard
        // already created an account/venue in Firestore. Calling that path
        // on a bare Cancel (nothing created yet) surfaced as a spurious
        // "Error: Not signed in" on the login screen.
        cancelButton_.onClick = [this]
        {
            if (! accountCreatedThisSession_)
            {
                if (onFinished_) onFinished_(false);
                return;
            }

            // The account created earlier in this session must be torn
            // down, or retrying with the same email later fails with
            // "email already exists". Self-delete (via the account's own
            // idToken, still signed in from CreateAccountStep) needs no
            // admin privilege -- a user can always delete their own account.
            cancelButton_.setEnabled(false);
            const auto uidToDelete = uid_;
            juce::Component::SafePointer<Content> safe(this);
            juce::Thread::launch([safe, uidToDelete]()
            {
                auto& fc = FC::getInstance();
                if (uidToDelete.isNotEmpty())
                    fc.deleteDocument("hosts/" + uidToDelete);
                fc.deleteCurrentAccount();
                fc.signOut();

                juce::MessageManager::callAsync([safe]()
                {
                    if (safe == nullptr) return;
                    if (safe->onFinished_) safe->onFinished_(false);
                });
            });
        };

        const auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                .getParentDirectory();
        const auto logoFile = appDir.getChildFile("assets/images/encore-logo-white.png");
        if (logoFile.existsAsFile())
            logoImage_ = juce::ImageFileFormat::loadFrom(logoFile);

        if (startStep == StartStep::AccountType)
        {
            // Already signed in via the normal login form.
            uid_   = FC::getInstance().getUserId();
            email_ = FC::getInstance().getEmail();
            if (HostService::getInstance().hasCurrent())
                host_ = HostService::getInstance().getCurrent();
            host_.email = email_.toStdString();
            showStep(Step::AccountType);
        }
        else
        {
            showStep(Step::CreateAccount);
        }
    }

    ~Content() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        const auto cardBounds = getLocalBounds().reduced(kShadowMargin);
        juce::Path cardPath;
        cardPath.addRoundedRectangle(cardBounds.toFloat(), (float) kCardCornerRadius);

        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.45f), 28, juce::Point<int>(0, 8));
        shadow.drawForPath(g, cardPath);

        auto b = cardBounds.toFloat();
        juce::ColourGradient grad(juce::Colour(LoginTheme::kAccentBlue), b.getCentreX(), b.getY(),
                                   juce::Colours::black,                 b.getCentreX(), b.getBottom(),
                                   false);
        g.setGradientFill(grad);
        g.fillPath(cardPath);

        g.setColour(juce::Colours::white);
        g.strokePath(cardPath, juce::PathStrokeType(4.0f));

        if (logoImage_.isValid() && ! logoBounds_.isEmpty())
            g.drawImageWithin(logoImage_,
                              logoBounds_.getX(), logoBounds_.getY(),
                              logoBounds_.getWidth(), logoBounds_.getHeight(),
                              juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid
                                | juce::RectanglePlacement::onlyReduceInSize);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(kShadowMargin).reduced(24);
        auto top = area.removeFromTop(36);
        // Smaller than the login screen's centrepiece logo, but still
        // clearly readable -- same "top of the screen" spot on every step,
        // opposite Cancel.
        logoBounds_ = top.removeFromLeft(150);
        cancelButton_.setBounds(top.removeFromRight(90));
        area.removeFromTop(8);
        if (stepComponent_ != nullptr)
            stepComponent_->setBounds(area);
    }

    // Each step's factory function below calls this to swap in the next one.
    void showStep(Step step);

private:
    //==============================================================================
    // Step: Create Account
    class CreateAccountStep : public juce::Component
    {
    public:
        CreateAccountStep()
        {
            styleHeading(heading_, "Create Your Account");
            styleSubheading(sub_, "Let's get your karaoke setup started.");
            addAndMakeVisible(heading_); addAndMakeVisible(sub_);

            setupField(firstNameLabel_, firstNameEd_, "First Name", "Jamie");
            setupField(lastNameLabel_,  lastNameEd_,  "Last Name",  "Rivera");
            setupField(stageNameLabel_, stageNameEd_, "Stage Name (shown to singers)", "DJ Jamie");
            setupField(emailLabel_,     emailEd_,     "Email", "you@example.com");
            setupField(passLabel_,      passEd_,      "Password", "At least 6 characters");
            passEd_.setPasswordCharacter((juce::juce_wchar) 0x2022);
            setupField(confirmLabel_,   confirmEd_,   "Confirm Password", "Re-enter password");
            confirmEd_.setPasswordCharacter((juce::juce_wchar) 0x2022);

            styleFieldLabel(avatarLabel_, "Choose an Avatar");
            addAndMakeVisible(avatarLabel_);
            addAndMakeVisible(avatarViewport_);
            avatarViewport_.setViewedComponent(&avatarGridContainer_, false);
            avatarViewport_.setScrollBarsShown(true, false);
            buildAvatarGrid();

            addAndMakeVisible(statusLabel_);
            styleStatus(statusLabel_);

            addAndMakeVisible(submitButton_);
            submitButton_.setButtonText("Create Account");
            submitButton_.onClick = [this] { submit(); };
        }

        void resized() override
        {
            auto area = getLocalBounds();
            heading_.setBounds(area.removeFromTop(30));
            sub_.setBounds(area.removeFromTop(22));
            area.removeFromTop(12);

            auto row = [&](juce::Rectangle<int> full) -> std::array<juce::Rectangle<int>, 2>
            {
                auto left = full.removeFromLeft(full.getWidth() / 2 - 8);
                full.removeFromLeft(16);
                return { left, full };
            };

            auto r1 = area.removeFromTop(58);
            auto [firstArea, lastArea] = row(r1);
            layoutField(firstNameLabel_, firstNameEd_, firstArea);
            layoutField(lastNameLabel_,  lastNameEd_,  lastArea);
            area.removeFromTop(10);

            auto r2 = area.removeFromTop(58);
            auto [stageArea, emailArea] = row(r2);
            layoutField(stageNameLabel_, stageNameEd_, stageArea);
            layoutField(emailLabel_,     emailEd_,     emailArea);
            area.removeFromTop(10);

            auto r3 = area.removeFromTop(58);
            auto [passArea, confirmArea] = row(r3);
            layoutField(passLabel_,    passEd_,    passArea);
            layoutField(confirmLabel_, confirmEd_, confirmArea);
            area.removeFromTop(14);

            avatarLabel_.setBounds(area.removeFromTop(20));

            // Fixed at exactly kAvatarRowsVisible rows -- there are more
            // avatar icons than fit in the wizard's fixed-size window, so
            // the grid scrolls internally instead of overflowing into
            // whatever's laid out below it (this used to overlap the
            // Create Account button once enough avatar files existed).
            const int viewportW = area.getWidth();
            const int scrollbarW = avatarViewport_.getScrollBarThickness();
            const int cell = juce::jmax(1, (viewportW - scrollbarW) / kAvatarCols);
            auto gridArea = area.removeFromTop(kAvatarRowsVisible * cell + 4);
            avatarViewport_.setBounds(gridArea);
            layoutAvatarGrid(viewportW - scrollbarW);

            area.removeFromTop(10);
            statusLabel_.setBounds(area.removeFromTop(20));
            submitButton_.setBounds(area.removeFromTop(44).withSizeKeepingCentre(220, 44));
        }

    private:
        void setupField(juce::Label& label, juce::TextEditor& editor,
                        const juce::String& labelText, const juce::String& placeholder)
        {
            styleFieldLabel(label, labelText);
            styleEditor(editor, placeholder);
            addAndMakeVisible(label);
            addAndMakeVisible(editor);
        }

        void layoutField(juce::Label& label, juce::TextEditor& editor, juce::Rectangle<int> area)
        {
            label.setBounds(area.removeFromTop(16));
            editor.setBounds(area);
        }

        void buildAvatarGrid()
        {
            const auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                    .getParentDirectory();
            const auto iconDir = appDir.getChildFile("assets/icon");
            if (! iconDir.isDirectory())
                return;

            auto files = iconDir.findChildFiles(juce::File::findFiles, false, "*.png");
            files.sort();

            int groupId = 1;
            for (const auto& f : files)
            {
                auto img = juce::ImageFileFormat::loadFrom(f);
                if (! img.isValid())
                    continue;

                auto btn = std::make_unique<juce::ImageButton>();
                btn->setImages(false, true, true,
                              img, 1.0f, {},
                              img, 1.0f, juce::Colours::white.withAlpha(0.15f),
                              img, 1.0f, juce::Colours::white.withAlpha(0.30f));
                btn->setClickingTogglesState(true);
                btn->setRadioGroupId(groupId);
                const auto relPath = juce::String("assets/icon/") + f.getFileName();
                btn->getProperties().set("avatarPath", relPath);
                avatarGridContainer_.addAndMakeVisible(*btn);
                avatarButtons_.push_back(std::move(btn));
            }

            if (! avatarButtons_.empty())
                avatarButtons_.front()->setToggleState(true, juce::dontSendNotification);
        }

        // Lays out every avatar into avatarGridContainer_ (all rows, not
        // just the visible ones) and grows the container to match, so
        // avatarViewport_ can scroll to whatever doesn't fit in its fixed
        // kAvatarRowsVisible-tall viewing area.
        void layoutAvatarGrid(int containerWidth)
        {
            const int cell = juce::jmax(1, containerWidth / kAvatarCols);
            int col = 0, row = 0;
            for (auto& btn : avatarButtons_)
            {
                btn->setBounds(col * cell, row * cell, cell - 6, cell - 6);
                ++col;
                if (col >= kAvatarCols)
                {
                    col = 0;
                    ++row;
                }
            }
            const int totalRows = avatarButtons_.empty() ? 1
                : (int) ((avatarButtons_.size() + kAvatarCols - 1) / (size_t) kAvatarCols);
            avatarGridContainer_.setSize(containerWidth, totalRows * cell);
        }

        juce::String selectedAvatarPath() const
        {
            for (auto& btn : avatarButtons_)
                if (btn->getToggleState())
                    return btn->getProperties()["avatarPath"].toString();
            return {};
        }

        void submit()
        {
            const auto first    = firstNameEd_.getText().trim();
            const auto last     = lastNameEd_.getText().trim();
            const auto stage    = stageNameEd_.getText().trim();
            const auto email    = emailEd_.getText().trim();
            const auto password = passEd_.getText();
            const auto confirm  = confirmEd_.getText();

            if (first.isEmpty() || last.isEmpty() || email.isEmpty())
            {
                statusLabel_.setText("First name, last name and email are required.", juce::dontSendNotification);
                return;
            }
            if (! looksLikeEmail(email))
            {
                statusLabel_.setText("Enter a valid email address.", juce::dontSendNotification);
                return;
            }
            if (password.length() < 6)
            {
                statusLabel_.setText("Password must be at least 6 characters.", juce::dontSendNotification);
                return;
            }
            if (password != confirm)
            {
                statusLabel_.setText("Passwords do not match.", juce::dontSendNotification);
                return;
            }

            submitButton_.setEnabled(false);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kSubtleText));
            statusLabel_.setText("Creating your account...", juce::dontSendNotification);

            const auto stageName = stage.isNotEmpty() ? stage : (first + " " + last);
            const auto avatarPath = selectedAvatarPath();

            juce::Component::SafePointer<CreateAccountStep> safe(this);
            juce::Thread::launch([safe, first, last, stageName, email, password, avatarPath]()
            {
                auto& fc = FC::getInstance();
                auto authResult = fc.signUpWithEmailPassword(email, password);

                if (! authResult.ok)
                {
                    const auto msg = authResult.errorMessage.isNotEmpty() ? authResult.errorMessage
                                                                          : authResult.errorCode;
                    juce::MessageManager::callAsync([safe, msg]()
                    {
                        if (safe == nullptr) return;
                        safe->submitButton_.setEnabled(true);
                        safe->statusLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kErrorText));
                        safe->statusLabel_.setText(msg.isNotEmpty() ? msg : "Could not create account.",
                                                   juce::dontSendNotification);
                    });
                    return;
                }

                const auto uid = fc.getUserId();
                const bool isFirstHostEver = fc.listCollection("hosts", 1).isEmpty();
                const UserRole role = isFirstHostEver ? UserRole::EnterpriseAdmin : UserRole::Host;

                const auto now = juce::Time::getCurrentTime();
                juce::Time expiry(now.toMilliseconds() + (juce::int64) 365 * 24 * 60 * 60 * 1000);

                auto fields = FC::makeFields({
                    { "userId",     FC::stringValue(uid) },
                    { "email",      FC::stringValue(email) },
                    { "companyId",  FC::stringValue("") },
                    { "profileId",  FC::stringValue("") },
                    { "avatarUrl",  FC::stringValue(avatarPath) },
                    { "stageName",  FC::stringValue(stageName) },
                    { "firstName",  FC::stringValue(first) },
                    { "lastName",   FC::stringValue(last) },
                    { "fullName",   FC::stringValue(first + " " + last) },
                    { "birthday",   FC::stringValue("") },
                    { "country",    FC::stringValue("") },
                    { "city",       FC::stringValue("") },
                    { "gender",     FC::stringValue("") },
                    { "signUpDate", FC::stringValue(now.toISO8601(true)) },
                    { "lastLogin",  FC::stringValue(now.toISO8601(true)) },
                    { "role",       FC::stringValue(juce::String(AccessRightsUtil::userRoleToString(role))) },
                    { "accessExpirationDate", FC::timestampValue(expiry) }
                });
                FC::getInstance().createDocument("hosts", fields, uid);

                Host h;
                h.userId = uid.toStdString(); h.email = email.toStdString();
                h.firstName = first.toStdString(); h.lastName = last.toStdString();
                h.stageName = stageName.toStdString(); h.fullName = (first + " " + last).toStdString();
                h.avatarUrl = avatarPath.toStdString(); h.role = role;

                auto invitations = InvitationService::queryPendingInvitationsSync(email);

                juce::MessageManager::callAsync([safe, uid, email, h, invitations]() mutable
                {
                    if (safe == nullptr) return;
                    HostService::getInstance().setCurrent(h);
                    if (safe->onAccountCreated)
                        safe->onAccountCreated(uid, email, h, invitations);
                });
            });
        }

    public:
        std::function<void(juce::String uid, juce::String email, Host host,
                           std::vector<VenueInvitation> invitations)> onAccountCreated;

    private:
        static constexpr int kAvatarCols = 9;
        static constexpr int kAvatarRowsVisible = 2;

        juce::Label heading_, sub_;
        juce::Label firstNameLabel_, lastNameLabel_, stageNameLabel_, emailLabel_, passLabel_, confirmLabel_;
        juce::TextEditor firstNameEd_, lastNameEd_, stageNameEd_, emailEd_, passEd_, confirmEd_;
        juce::Label avatarLabel_, statusLabel_;
        juce::TextButton submitButton_;
        juce::Viewport avatarViewport_;
        juce::Component avatarGridContainer_;
        std::vector<std::unique_ptr<juce::ImageButton>> avatarButtons_;
    };

    //==============================================================================
    // Step: You were invited — join instead of creating a new venue/company.
    class JoinInvitedVenueStep : public juce::Component
    {
    public:
        JoinInvitedVenueStep(VenueInvitation invitation, juce::String uid,
                             std::function<void(bool joined)> onDone)
            : invitation_(std::move(invitation)), uid_(std::move(uid)), onDone_(std::move(onDone))
        {
            styleHeading(heading_, "You Were Invited!");
            styleSubheading(sub_, juce::String(invitation_.invitedByName.isNotEmpty()
                                              ? invitation_.invitedByName : invitation_.invitedByEmail)
                                     + " invited you to join a venue on Encore.");
            addAndMakeVisible(heading_); addAndMakeVisible(sub_);

            addAndMakeVisible(detailLabel_);
            detailLabel_.setText("Venue: " + invitation_.venueName
                                 + "\nRole: " + juce::String(AccessRightsUtil::userRoleToString(invitation_.role)),
                                 juce::dontSendNotification);
            detailLabel_.setFont(juce::Font(juce::FontOptions(15.0f)));
            detailLabel_.setJustificationType(juce::Justification::topLeft);
            detailLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kBodyText));

            addAndMakeVisible(statusLabel_);
            styleStatus(statusLabel_);

            addAndMakeVisible(acceptButton_);
            acceptButton_.setButtonText("Accept & Join This Venue");
            acceptButton_.onClick = [this] { accept(); };

            addAndMakeVisible(declineButton_);
            declineButton_.setButtonText("No Thanks, Set Up My Own");
            declineButton_.getProperties().set("ghost", true);
            declineButton_.onClick = [this] { if (onDone_) onDone_(false); };
        }

        void resized() override
        {
            auto area = getLocalBounds();
            heading_.setBounds(area.removeFromTop(30));
            sub_.setBounds(area.removeFromTop(40));
            area.removeFromTop(16);
            detailLabel_.setBounds(area.removeFromTop(80));
            area.removeFromTop(16);
            statusLabel_.setBounds(area.removeFromTop(20));
            acceptButton_.setBounds(area.removeFromTop(44).withSizeKeepingCentre(280, 44));
            area.removeFromTop(10);
            declineButton_.setBounds(area.removeFromTop(36).withSizeKeepingCentre(240, 36));
        }

    private:
        void accept()
        {
            acceptButton_.setEnabled(false);
            declineButton_.setEnabled(false);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kSubtleText));
            statusLabel_.setText("Joining venue...", juce::dontSendNotification);

            juce::Component::SafePointer<JoinInvitedVenueStep> safe(this);
            InvitationService::getInstance().acceptInvitation(invitation_, uid_,
                [safe](bool ok, juce::String error)
                {
                    if (safe == nullptr) return;
                    if (ok)
                    {
                        if (safe->onDone_) safe->onDone_(true);
                    }
                    else
                    {
                        safe->acceptButton_.setEnabled(true);
                        safe->declineButton_.setEnabled(true);
                        safe->statusLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kErrorText));
                        safe->statusLabel_.setText(error.isNotEmpty() ? error : "Could not join venue.",
                                                   juce::dontSendNotification);
                    }
                });
        }

        VenueInvitation invitation_;
        juce::String uid_;
        std::function<void(bool)> onDone_;
        juce::Label heading_, sub_, detailLabel_, statusLabel_;
        juce::TextButton acceptButton_, declineButton_;
    };

    //==============================================================================
    // Step: Account type chooser
    class AccountTypeStep : public juce::Component
    {
    public:
        AccountTypeStep(std::function<void(int tier)> onChosen)
            : onChosen_(std::move(onChosen)),
              card1_(TierCard::Icon::SingleVenue, "Single Venue",
                     "I run karaoke at one location. Simple setup, one venue, one login."),
              card2_(TierCard::Icon::MultiVenue, "Multiple Venues",
                     "I own several venues and want to bring on other hosts."),
              card3_(TierCard::Icon::Company, "Karaoke Company",
                     "We run karaoke across many venues with many hosts and computers.")
        {
            styleHeading(heading_, "How Will You Use Encore?");
            styleSubheading(sub_, "Pick the option that best matches your setup.");
            addAndMakeVisible(heading_); addAndMakeVisible(sub_);

            for (auto* card : { &card1_, &card2_, &card3_ })
                addAndMakeVisible(card);

            card1_.onClick = [this] { if (onChosen_) onChosen_(1); };
            card2_.onClick = [this] { if (onChosen_) onChosen_(2); };
            card3_.onClick = [this] { if (onChosen_) onChosen_(3); };
        }

        void resized() override
        {
            auto area = getLocalBounds();
            heading_.setBounds(area.removeFromTop(34));
            sub_.setBounds(area.removeFromTop(22));
            area.removeFromTop(24);

            const int gap = 20;
            const int w = (area.getWidth() - gap * 2) / 3;
            card1_.setBounds(area.removeFromLeft(w));
            area.removeFromLeft(gap);
            card2_.setBounds(area.removeFromLeft(w));
            area.removeFromLeft(gap);
            card3_.setBounds(area);
        }

    private:
        std::function<void(int)> onChosen_;
        juce::Label heading_, sub_;
        TierCard card1_, card2_, card3_;
    };

    //==============================================================================
    // Step: Company info (tiers 2 & 3)
    class CompanyInfoStep : public juce::Component
    {
    public:
        CompanyInfoStep(std::function<void(Company)> onDone) : onDone_(std::move(onDone))
        {
            styleHeading(heading_, "Tell Us About Your Company");
            styleSubheading(sub_, "This groups your venues together so you can manage hosts across all of them.");
            addAndMakeVisible(heading_); addAndMakeVisible(sub_);

            setupField(nameLabel_, nameEd_, "Company Name", "Sunset Karaoke Group");
            setupField(addressLabel_, addressEd_, "Address", "123 Main St");
            setupField(cityLabel_, cityEd_, "City", "Austin");
            setupField(countryLabel_, countryEd_, "Country", "USA");

            addAndMakeVisible(statusLabel_);
            styleStatus(statusLabel_);
            addAndMakeVisible(nextButton_);
            nextButton_.setButtonText("Next: Add Your First Venue");
            nextButton_.onClick = [this] { submit(); };
        }

        void resized() override
        {
            auto area = getLocalBounds();
            heading_.setBounds(area.removeFromTop(30));
            sub_.setBounds(area.removeFromTop(40));
            area.removeFromTop(16);

            layoutField(nameLabel_, nameEd_, area.removeFromTop(58));
            area.removeFromTop(10);
            layoutField(addressLabel_, addressEd_, area.removeFromTop(58));
            area.removeFromTop(10);

            auto row = area.removeFromTop(58);
            auto cityArea = row.removeFromLeft(row.getWidth() / 2 - 8);
            row.removeFromLeft(16);
            layoutField(cityLabel_, cityEd_, cityArea);
            layoutField(countryLabel_, countryEd_, row);

            area.removeFromTop(16);
            statusLabel_.setBounds(area.removeFromTop(20));
            nextButton_.setBounds(area.removeFromTop(44).withSizeKeepingCentre(260, 44));
        }

    private:
        void setupField(juce::Label& label, juce::TextEditor& editor,
                        const juce::String& text, const juce::String& placeholder)
        {
            styleFieldLabel(label, text);
            styleEditor(editor, placeholder);
            addAndMakeVisible(label); addAndMakeVisible(editor);
        }
        void layoutField(juce::Label& label, juce::TextEditor& editor, juce::Rectangle<int> area)
        {
            label.setBounds(area.removeFromTop(16));
            editor.setBounds(area);
        }

        void submit()
        {
            const auto name = nameEd_.getText().trim();
            if (name.isEmpty())
            {
                statusLabel_.setText("Company name is required.", juce::dontSendNotification);
                return;
            }

            nextButton_.setEnabled(false);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kSubtleText));
            statusLabel_.setText("Creating company...", juce::dontSendNotification);

            Company c;
            c.name = name.toStdString();
            c.address = addressEd_.getText().trim().toStdString();
            c.city = cityEd_.getText().trim().toStdString();
            c.country = countryEd_.getText().trim().toStdString();
            c.ownerUserId = FC::getInstance().getUserId().toStdString();

            juce::Component::SafePointer<CompanyInfoStep> safe(this);
            CompanyService::getInstance().createCompany(c,
                [safe, c](bool ok, juce::String companyId, juce::String error)
                {
                    if (safe == nullptr) return;
                    if (! ok)
                    {
                        safe->nextButton_.setEnabled(true);
                        safe->statusLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kErrorText));
                        safe->statusLabel_.setText(error.isNotEmpty() ? error : "Could not create company.",
                                                   juce::dontSendNotification);
                        return;
                    }

                    CompanyService::getInstance().addCompanyMember(companyId, FC::getInstance().getUserId(),
                                                                    "company_admin", nullptr);

                    Company created = c;
                    created.id = companyId.toStdString();
                    if (safe->onDone_) safe->onDone_(created);
                });
        }

        std::function<void(Company)> onDone_;
        juce::Label heading_, sub_, nameLabel_, addressLabel_, cityLabel_, countryLabel_, statusLabel_;
        juce::TextEditor nameEd_, addressEd_, cityEd_, countryEd_;
        juce::TextButton nextButton_;
    };

    //==============================================================================
    // Step: Venue info. Tier 1/2 create exactly one venue; tier 3 supports
    // adding multiple before continuing.
    class VenueInfoStep : public juce::Component
    {
    public:
        VenueInfoStep(int tier, juce::String companyId, std::vector<VenueItem>* createdVenues,
                     std::function<void()> onDone)
            : tier_(tier), companyId_(std::move(companyId)), createdVenues_(createdVenues), onDone_(std::move(onDone))
        {
            styleHeading(heading_, tier_ == 3 ? "Add Your Venues" : "Tell Us About Your Venue");
            styleSubheading(sub_, "Provide information about your venue so your customers can find you.");
            addAndMakeVisible(heading_); addAndMakeVisible(sub_);

            setupField(nameLabel_, nameEd_, "Venue Name", "The Blue Note Lounge");
            setupField(addressLabel_, addressEd_, "Address", "456 Elm St");
            setupField(cityLabel_, cityEd_, "City", "Austin");
            setupField(countryLabel_, countryEd_, "Country", "USA");

            addAndMakeVisible(createdListLabel_);
            createdListLabel_.setFont(juce::Font(juce::FontOptions(13.0f)));
            createdListLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kSuccessText));
            createdListLabel_.setJustificationType(juce::Justification::topLeft);
            refreshCreatedList();

            addAndMakeVisible(statusLabel_);
            styleStatus(statusLabel_);

            addAndMakeVisible(addButton_);
            addButton_.setButtonText(tier_ == 3 ? "Add This Venue" : "Create Venue & Continue");
            addButton_.onClick = [this] { submit(); };

            if (tier_ == 3)
            {
                addAndMakeVisible(continueButton_);
                continueButton_.setButtonText("Continue");
                continueButton_.getProperties().set("ghost", true);
                continueButton_.setEnabled(false);
                continueButton_.onClick = [this] { if (onDone_) onDone_(); };
            }
        }

        void resized() override
        {
            auto area = getLocalBounds();
            heading_.setBounds(area.removeFromTop(30));
            sub_.setBounds(area.removeFromTop(36));
            area.removeFromTop(12);

            if (tier_ == 3)
            {
                createdListLabel_.setBounds(area.removeFromTop(50));
                area.removeFromTop(8);
            }

            layoutField(nameLabel_, nameEd_, area.removeFromTop(58));
            area.removeFromTop(10);
            layoutField(addressLabel_, addressEd_, area.removeFromTop(58));
            area.removeFromTop(10);

            auto row = area.removeFromTop(58);
            auto cityArea = row.removeFromLeft(row.getWidth() / 2 - 8);
            row.removeFromLeft(16);
            layoutField(cityLabel_, cityEd_, cityArea);
            layoutField(countryLabel_, countryEd_, row);

            area.removeFromTop(16);
            statusLabel_.setBounds(area.removeFromTop(20));

            if (tier_ == 3)
            {
                auto btnRow = area.removeFromTop(44);
                addButton_.setBounds(btnRow.removeFromLeft(btnRow.getWidth() / 2 - 8));
                btnRow.removeFromLeft(16);
                continueButton_.setBounds(btnRow);
            }
            else
            {
                addButton_.setBounds(area.removeFromTop(44).withSizeKeepingCentre(260, 44));
            }
        }

    private:
        void setupField(juce::Label& label, juce::TextEditor& editor,
                        const juce::String& text, const juce::String& placeholder)
        {
            styleFieldLabel(label, text);
            styleEditor(editor, placeholder);
            addAndMakeVisible(label); addAndMakeVisible(editor);
        }
        void layoutField(juce::Label& label, juce::TextEditor& editor, juce::Rectangle<int> area)
        {
            label.setBounds(area.removeFromTop(16));
            editor.setBounds(area);
        }

        void refreshCreatedList()
        {
            if (tier_ != 3 || createdVenues_ == nullptr) return;
            juce::String text = createdVenues_->empty() ? juce::String("No venues added yet.")
                                                          : juce::String("Added so far: ");
            for (size_t i = 0; i < createdVenues_->size(); ++i)
            {
                if (i > 0) text += ", ";
                text += juce::String((*createdVenues_)[i].name);
            }
            createdListLabel_.setText(text, juce::dontSendNotification);
        }

        void submit()
        {
            const auto name = nameEd_.getText().trim();
            if (name.isEmpty())
            {
                statusLabel_.setText("Venue name is required.", juce::dontSendNotification);
                return;
            }

            addButton_.setEnabled(false);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kSubtleText));
            statusLabel_.setText("Creating venue...", juce::dontSendNotification);

            VenueItem v;
            v.name = name.toStdString();
            v.address = addressEd_.getText().trim().toStdString();
            v.city = cityEd_.getText().trim().toStdString();
            v.country = countryEd_.getText().trim().toStdString();
            v.companyId = companyId_.toStdString();

            const auto uid = FC::getInstance().getUserId();

            juce::Component::SafePointer<VenueInfoStep> safe(this);
            VenueService::getInstance().addVenue(v,
                [safe, v, uid](bool ok, juce::String venueId, juce::String error)
                {
                    if (safe == nullptr) return;
                    if (! ok || venueId.isEmpty())
                    {
                        safe->addButton_.setEnabled(true);
                        safe->statusLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kErrorText));
                        safe->statusLabel_.setText(error.isNotEmpty() ? error : "Could not create venue.",
                                                   juce::dontSendNotification);
                        return;
                    }

                    const auto venueName = juce::String(v.name);
                    LicenseService::getInstance().createLicenseForVenue(venueId, venueName, nullptr);

                    // Self-grant the creator's association — role "Admin",
                    // doc id `{uid}_{venueId}` (matches
                    // LoginFlowController::queryAssociations' convention).
                    const auto now = juce::Time::getCurrentTime();
                    auto assocFields = FC::makeFields({
                        { "userId",     FC::stringValue(uid) },
                        { "venueId",    FC::stringValue(venueId) },
                        { "venueName",  FC::stringValue(venueName) },
                        { "role",       FC::stringValue(juce::String(AccessRightsUtil::userRoleToString(UserRole::Admin))) },
                        { "status",     FC::stringValue("active") },
                        { "joinedDate", FC::timestampValue(now) },
                        { "lastActive", FC::timestampValue(now) }
                    });
                    juce::Thread::launch([uid, venueId, assocFields]()
                    {
                        FC::getInstance().createDocument("user-venue-lookup", assocFields, uid + "_" + venueId);
                    });

                    VenueItem created = v;
                    created.id = venueId.toStdString();
                    if (safe->createdVenues_ != nullptr)
                        safe->createdVenues_->push_back(created);

                    if (safe->tier_ == 3)
                    {
                        safe->addButton_.setEnabled(true);
                        safe->continueButton_.setEnabled(true);
                        safe->statusLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kSuccessText));
                        safe->statusLabel_.setText("Venue added! Add another or continue.", juce::dontSendNotification);
                        safe->nameEd_.clear(); safe->addressEd_.clear();
                        safe->cityEd_.clear(); safe->countryEd_.clear();
                        safe->refreshCreatedList();
                        safe->resized();
                    }
                    else
                    {
                        if (safe->onDone_) safe->onDone_();
                    }
                });
        }

        int tier_;
        juce::String companyId_;
        std::vector<VenueItem>* createdVenues_;
        std::function<void()> onDone_;
        juce::Label heading_, sub_, nameLabel_, addressLabel_, cityLabel_, countryLabel_, statusLabel_, createdListLabel_;
        juce::TextEditor nameEd_, addressEd_, cityEd_, countryEd_;
        juce::TextButton addButton_, continueButton_;
    };

    //==============================================================================
    // Step: Host invite (tiers 2 & 3, skippable)
    class HostInviteStep : public juce::Component
    {
    public:
        HostInviteStep(std::vector<VenueItem> venues, std::function<void()> onDone)
            : venues_(std::move(venues)), onDone_(std::move(onDone))
        {
            styleHeading(heading_, "Invite Your Hosts");
            styleSubheading(sub_, "Optional — you can always invite hosts later from Settings.");
            addAndMakeVisible(heading_); addAndMakeVisible(sub_);

            styleFieldLabel(emailLabel_, "Host Email");
            styleEditor(emailEd_, "host@example.com");
            addAndMakeVisible(emailLabel_); addAndMakeVisible(emailEd_);

            addAndMakeVisible(venueBox_);
            for (int i = 0; i < (int) venues_.size(); ++i)
                venueBox_.addItem(juce::String(venues_[(size_t) i].name), i + 1);
            if (! venues_.empty()) venueBox_.setSelectedId(1);

            addAndMakeVisible(addRowButton_);
            addRowButton_.setButtonText("Add Invite");
            addRowButton_.onClick = [this] { addPendingInvite(); };

            addAndMakeVisible(pendingListLabel_);
            pendingListLabel_.setFont(juce::Font(juce::FontOptions(13.0f)));
            pendingListLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kBodyText));
            pendingListLabel_.setJustificationType(juce::Justification::topLeft);

            addAndMakeVisible(statusLabel_);
            styleStatus(statusLabel_);

            addAndMakeVisible(sendButton_);
            sendButton_.setButtonText("Send Invites & Continue");
            sendButton_.onClick = [this] { sendAll(); };

            addAndMakeVisible(skipButton_);
            skipButton_.setButtonText("Skip for Now");
            skipButton_.getProperties().set("ghost", true);
            skipButton_.onClick = [this] { if (onDone_) onDone_(); };
        }

        void resized() override
        {
            auto area = getLocalBounds();
            heading_.setBounds(area.removeFromTop(30));
            sub_.setBounds(area.removeFromTop(28));
            area.removeFromTop(12);

            auto row = area.removeFromTop(58);
            auto emailArea = row.removeFromLeft(row.getWidth() * 2 / 3 - 8);
            row.removeFromLeft(16);
            emailLabel_.setBounds(emailArea.removeFromTop(16));
            emailEd_.setBounds(emailArea);
            venueBox_.setBounds(row.removeFromBottom(38));

            area.removeFromTop(8);
            addRowButton_.setBounds(area.removeFromTop(32).withSizeKeepingCentre(160, 32));
            area.removeFromTop(10);
            pendingListLabel_.setBounds(area.removeFromTop(90));

            area.removeFromTop(10);
            statusLabel_.setBounds(area.removeFromTop(20));
            sendButton_.setBounds(area.removeFromTop(44).withSizeKeepingCentre(260, 44));
            area.removeFromTop(8);
            skipButton_.setBounds(area.removeFromTop(32).withSizeKeepingCentre(160, 32));
        }

    private:
        struct PendingInvite { juce::String email; juce::String venueId; juce::String venueName; };

        void addPendingInvite()
        {
            const auto email = emailEd_.getText().trim();
            if (! looksLikeEmail(email) || venues_.empty())
                return;

            const int idx = venueBox_.getSelectedId() - 1;
            if (idx < 0 || idx >= (int) venues_.size())
                return;

            PendingInvite p;
            p.email = email;
            p.venueId = juce::String(venues_[(size_t) idx].id);
            p.venueName = juce::String(venues_[(size_t) idx].name);
            pending_.push_back(p);
            emailEd_.clear();

            juce::String text;
            for (auto& inv : pending_)
                text += inv.email + " → " + inv.venueName + "\n";
            pendingListLabel_.setText(text, juce::dontSendNotification);
        }

        void sendAll()
        {
            if (pending_.empty())
            {
                if (onDone_) onDone_();
                return;
            }

            sendButton_.setEnabled(false);
            skipButton_.setEnabled(false);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kSubtleText));
            statusLabel_.setText("Sending invites...", juce::dontSendNotification);

            const auto inviterEmail = FC::getInstance().getEmail();
            const auto inviterName  = HostService::getInstance().hasCurrent()
                                         ? juce::String(HostService::getInstance().getCurrent().stageName)
                                         : inviterEmail;
            auto toSend = pending_;

            juce::Component::SafePointer<HostInviteStep> safe(this);
            juce::Thread::launch([safe, toSend, inviterEmail, inviterName]()
            {
                const auto now = juce::Time::getCurrentTime();
                juce::Time expiry(now.toMilliseconds() + (juce::int64) 30 * 24 * 60 * 60 * 1000);

                for (auto& inv : toSend)
                {
                    auto fields = FC::makeFields({
                        { "venueId",          FC::stringValue(inv.venueId) },
                        { "venueName",        FC::stringValue(inv.venueName) },
                        { "invitedUserEmail", FC::stringValue(inv.email.toLowerCase()) },
                        { "invitedByEmail",   FC::stringValue(inviterEmail) },
                        { "invitedByName",    FC::stringValue(inviterName) },
                        { "role",             FC::stringValue(juce::String(AccessRightsUtil::userRoleToString(UserRole::Host))) },
                        { "invitationDate",   FC::timestampValue(now) },
                        { "expirationDate",   FC::timestampValue(expiry) },
                        { "isAccepted",       FC::booleanValue(false) },
                        { "isExpired",        FC::booleanValue(false) }
                    });
                    FC::getInstance().createDocument("venueInvitations", fields);
                }

                juce::MessageManager::callAsync([safe]()
                {
                    if (safe == nullptr) return;
                    if (safe->onDone_) safe->onDone_();
                });
            });
        }

        std::vector<VenueItem> venues_;
        std::vector<PendingInvite> pending_;
        std::function<void()> onDone_;
        juce::Label heading_, sub_, emailLabel_, pendingListLabel_, statusLabel_;
        juce::TextEditor emailEd_;
        juce::ComboBox venueBox_;
        juce::TextButton addRowButton_, sendButton_, skipButton_;
    };

    //==============================================================================
    // Step: Initial library scan
    class LibraryScanStep : public juce::Component, private juce::Timer
    {
    public:
        LibraryScanStep(std::function<void()> onDone) : onDone_(std::move(onDone))
        {
            styleHeading(heading_, "Build Your Songbook");
            styleSubheading(sub_, "Point Encore at your karaoke files (CDG/ZIP/MP4/M4A) so we can scan and build your songbook.");
            addAndMakeVisible(heading_); addAndMakeVisible(sub_);

            addAndMakeVisible(statusLabel_);
            statusLabel_.setFont(juce::Font(juce::FontOptions(14.0f)));
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(LoginTheme::kBodyText));
            statusLabel_.setJustificationType(juce::Justification::centred);
            statusLabel_.setText("No folder scanned yet.", juce::dontSendNotification);

            addAndMakeVisible(progressBar_);

            addAndMakeVisible(scanButton_);
            scanButton_.setButtonText("Choose Music Folder & Scan");
            scanButton_.onClick = [this] { chooseFolderAndScan(); };

            addAndMakeVisible(continueButton_);
            continueButton_.setButtonText("Continue to Login");
            continueButton_.onClick = [this] { if (onDone_) onDone_(); };

            if (songDb_.open())
                scanner_.setSongDatabase(&songDb_);

            scanner_.onProgress = [this](int cur, int total, juce::String name)
            {
                juce::MessageManager::callAsync([this, cur, total, name]()
                {
                    progressValue_ = total > 0 ? (double) cur / (double) total : -1.0;
                    statusLabel_.setText("Scanning: " + name, juce::dontSendNotification);
                });
            };
            scanner_.onComplete = [this](std::vector<CdgSong> songs, LibraryScanner::ScanStats)
            {
                juce::MessageManager::callAsync([this, count = (int) songs.size()]()
                {
                    stopTimer();
                    progressValue_ = 1.0;
                    scanButton_.setEnabled(true);
                    statusLabel_.setText("Done! Found " + juce::String(count) + " songs.",
                                        juce::dontSendNotification);
                });
            };
            scanner_.onError = [this](juce::String err)
            {
                juce::MessageManager::callAsync([this, err]()
                {
                    stopTimer();
                    scanButton_.setEnabled(true);
                    statusLabel_.setText("Scan error: " + err, juce::dontSendNotification);
                });
            };
        }

        ~LibraryScanStep() override
        {
            stopTimer();
            scanner_.setSongDatabase(nullptr);
        }

        void resized() override
        {
            auto area = getLocalBounds();
            heading_.setBounds(area.removeFromTop(30));
            sub_.setBounds(area.removeFromTop(40));
            area.removeFromTop(30);

            scanButton_.setBounds(area.removeFromTop(44).withSizeKeepingCentre(280, 44));
            area.removeFromTop(20);
            progressBar_.setBounds(area.removeFromTop(20).withSizeKeepingCentre(400, 20));
            area.removeFromTop(12);
            statusLabel_.setBounds(area.removeFromTop(24));

            area.removeFromTop(30);
            continueButton_.setBounds(area.removeFromTop(44).withSizeKeepingCentre(240, 44));
        }

    private:
        void timerCallback() override { /* keeps progressBar_ animating via progressValue_ */ }

        void chooseFolderAndScan()
        {
            if (scanner_.isScanning.load())
                return;

            const auto startDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
            fileChooser_ = std::make_shared<juce::FileChooser>("Select your karaoke music folder", startDir);
            fileChooser_->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                [this](const juce::FileChooser& fc)
                {
                    auto result = fc.getResult();
                    if (! result.isDirectory())
                        return;

                    UserPreferences::getInstance().setLibraryPath(result.getFullPathName());
                    scanButton_.setEnabled(false);
                    progressValue_ = -1.0;
                    statusLabel_.setText("Starting scan...", juce::dontSendNotification);
                    scanner_.startInitialScan(result);
                    startTimerHz(10);
                });
        }

        std::function<void()> onDone_;
        juce::Label heading_, sub_, statusLabel_;
        juce::TextButton scanButton_, continueButton_;
        double progressValue_ = 0.0;
        juce::ProgressBar progressBar_ { progressValue_ };
        std::shared_ptr<juce::FileChooser> fileChooser_;
        SongDatabase songDb_;
        LibraryScanner scanner_;
    };

    //==============================================================================
    LoginLookAndFeel lnf_;
    juce::Image logoImage_;
    juce::Rectangle<int> logoBounds_;
    std::function<void(bool completed)> onFinished_;
    juce::TextButton cancelButton_;
    std::unique_ptr<juce::Component> stepComponent_;

    // Wizard-wide state, accumulated as steps complete.
    bool accountCreatedThisSession_ = false;
    juce::String uid_, email_;
    Host host_;
    juce::String companyId_;
    int accountTier_ = 1;
    std::vector<VenueItem> createdVenues_;
    VenueInvitation pendingInvitationForNextStep_;

    JUCE_DECLARE_NON_COPYABLE(Content)
};

//==============================================================================
void OnboardingWizard::Content::showStep(Step step)
{
    switch (step)
    {
        case Step::CreateAccount:
        {
            auto s = std::make_unique<CreateAccountStep>();
            s->onAccountCreated = [this](juce::String uid, juce::String email, Host host,
                                         std::vector<VenueInvitation> invitations)
            {
                uid_ = uid; email_ = email; host_ = host;
                // From here on, Cancel must undo this account rather than
                // just closing the wizard -- otherwise the user can't
                // retry with the same email (accounts:signUp rejects an
                // already-registered address).
                accountCreatedThisSession_ = true;
                if (! invitations.empty())
                {
                    pendingInvitationForNextStep_ = invitations.front();
                    showStep(Step::JoinInvitedVenue);
                }
                else
                {
                    showStep(Step::AccountType);
                }
            };
            stepComponent_ = std::move(s);
            break;
        }
        case Step::JoinInvitedVenue:
        {
            auto s = std::make_unique<JoinInvitedVenueStep>(pendingInvitationForNextStep_, uid_,
                [this](bool joined)
                {
                    if (joined)
                    {
                        if (onFinished_) onFinished_(true);
                    }
                    else
                    {
                        showStep(Step::AccountType);
                    }
                });
            stepComponent_ = std::move(s);
            break;
        }
        case Step::AccountType:
        {
            auto s = std::make_unique<AccountTypeStep>([this](int tier)
            {
                accountTier_ = tier;
                if (tier == 1)
                    showStep(Step::VenueInfo);
                else
                    showStep(Step::CompanyInfo);
            });
            stepComponent_ = std::move(s);
            break;
        }
        case Step::CompanyInfo:
        {
            auto s = std::make_unique<CompanyInfoStep>([this](Company c)
            {
                companyId_ = juce::String(c.id);
                showStep(Step::VenueInfo);
            });
            stepComponent_ = std::move(s);
            break;
        }
        case Step::VenueInfo:
        {
            auto s = std::make_unique<VenueInfoStep>(accountTier_, companyId_, &createdVenues_, [this]()
            {
                if (accountTier_ == 1)
                    showStep(Step::LibraryScan);
                else
                    showStep(Step::HostInvite);
            });
            stepComponent_ = std::move(s);
            break;
        }
        case Step::HostInvite:
        {
            auto s = std::make_unique<HostInviteStep>(createdVenues_, [this]()
            {
                showStep(Step::LibraryScan);
            });
            stepComponent_ = std::move(s);
            break;
        }
        case Step::LibraryScan:
        {
            auto s = std::make_unique<LibraryScanStep>([this]()
            {
                if (onFinished_) onFinished_(true);
            });
            stepComponent_ = std::move(s);
            break;
        }
    }

    addAndMakeVisible(stepComponent_.get());
    resized();
}

//==============================================================================
OnboardingWizard::OnboardingWizard(StartStep startStep, std::function<void()> onFinished)
    : juce::DocumentWindow("Welcome to Encore", juce::Colours::black, 0),
      onFinished_(std::move(onFinished))
{
    setUsingNativeTitleBar(false);
    setTitleBarHeight(0);
    setResizable(false, false);
    setAlwaysOnTop(true);

    setContentOwned(new Content(startStep, [this](bool completed)
    {
        if (completed && onFinished_) onFinished_();
        exitModalState(0);
    }), false);
    setSize(kWindowW, kWindowH);
}

OnboardingWizard::~OnboardingWizard() = default;

void OnboardingWizard::closeButtonPressed()
{
    // Native close (currently unreachable -- setTitleBarHeight(0) hides the
    // title bar entirely) is a cancel, not a completion; never fires the
    // caller's onFinished_.
    exitModalState(0);
}

void OnboardingWizard::launch(juce::Component* parentForCentering,
                              StartStep startStep,
                              std::function<void()> onFinished)
{
    auto* wizard = new OnboardingWizard(startStep, std::move(onFinished));
    // Semi-transparent peer so the margin around the rounded card (drawn in
    // Content::paint()) is genuinely see-through rather than an opaque
    // rectangle -- otherwise the card's rounded corners would show square
    // against a solid backing colour instead of the login screen behind it.
    wizard->setOpaque(false);
    wizard->addToDesktop(juce::ComponentPeer::windowIsSemiTransparent);
    if (parentForCentering != nullptr)
        wizard->centreAroundComponent(parentForCentering, kWindowW, kWindowH);
    else
        wizard->centreWithSize(kWindowW, kWindowH);
    wizard->setVisible(true);
    // exitModalState() (called from the wizard's own child buttons, e.g. Cancel)
    // must not delete the wizard synchronously -- that would free the Button
    // that's still executing its own onClick on the call stack. JUCE runs this
    // ModalCallbackFunction asynchronously after exitModalState(), so the delete
    // is deferred until the click handler has fully unwound.
    wizard->enterModalState(true, juce::ModalCallbackFunction::create([wizard](int) { delete wizard; }), false);
}
