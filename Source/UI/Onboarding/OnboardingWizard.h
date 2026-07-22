/*
  ==============================================================================

    OnboardingWizard.h

    First-run setup wizard: create an account (or, if already signed in with
    zero venues/invitations, skip straight to the account-type chooser) ->
    detect a pending invitation (join it) or pick an account tier (single
    venue / multi-venue owner / karaoke company) -> create the
    company/venue records with a license -> optionally invite hosts -> run
    the first library scan -> hand back to LoginWindow, which re-resolves
    the normal post-auth flow (now finding the freshly-created association).

    Follows the same bespoke-borderless-DocumentWindow idiom as
    AddSongsDialog/SongEditDialog: a single owning window, an internal
    step enum, self-deletes on completion/cancel.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class OnboardingWizard : public juce::DocumentWindow
{
public:
    enum class StartStep
    {
        CreateAccount,  // Brand-new user, no Firebase account yet.
        AccountType     // Already signed in (via the normal login form),
                        // zero venues/invitations — skip account creation.
    };

    /** Builds and shows the wizard modally over `parentForCentering`.
        `onFinished` fires once the wizard has created everything it needs
        and the user chose to continue — the caller should re-run its own
        post-auth resolution (the wizard does not launch MainComponent
        itself). Also fires (with nothing created) if the user cancels. */
    static void launch(juce::Component* parentForCentering,
                       StartStep startStep,
                       std::function<void()> onFinished);

    void closeButtonPressed() override;

private:
    class Content;

    OnboardingWizard(StartStep startStep, std::function<void()> onFinished);
    ~OnboardingWizard() override;

    std::function<void()> onFinished_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OnboardingWizard)
};
