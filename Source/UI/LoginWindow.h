/*
  ==============================================================================

    LoginWindow.h

    Pre-main-window auth + venue-selection UI. Drives FirestoreClient and
    LoginFlowController, then invokes onLoginComplete() so Main.cpp can
    construct the real MainWindow.

    Embeds a single content component that swaps between four pages:
      LoginPage           – email/password, Google, Apple buttons
      SelectVenuePage     – list of associated venues
      AwaitingInvitationPage – pending invitations list / blocked screen
      RequestAccessPage   – Scenario 5: stored venueId, no association

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Auth/LoginFlowController.h"

//==============================================================================
class LoginWindow : public juce::DocumentWindow
{
public:
    using LoginCompleteCallback = std::function<void(juce::String selectedVenueId)>;

    LoginWindow(LoginCompleteCallback onComplete);
    ~LoginWindow() override;

    void closeButtonPressed() override;

private:
    class LoginContent;
    std::unique_ptr<LoginContent> contentRef_; // owned via setContentOwned, raw weak ref kept
    LoginCompleteCallback onComplete_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoginWindow)
};
