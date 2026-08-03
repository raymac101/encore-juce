/*
  ==============================================================================

    ProfilePage.h

    Self-service "Edit Profile" screen, reachable from the TopBar user-menu
    dropdown ("Edit Profile" -- see MainComponent's TopBar::onUserButtonClicked
    wiring), not from the sidebar. Ports the old Angular app's profile form
    (src/app/components/profile/profile.component.ts in the legacy project)
    field-for-field: fullName, email, stageName, birthday, country, city,
    gender, avatarUrl (picked from a fixed set of preset icons -- the old app
    never had a photo-upload flow, just 34 preset PNGs).

    Deliberate deviation from the old app: `role` is shown READ-ONLY here,
    not as an editable dropdown. The old Angular form let a host set their
    own `role` field with no extra gating, and this app's Firestore rules
    (`hosts/{hostId} allow update: if isPlatformAdmin() || authUid() == hostId`)
    place no restriction on which fields a self-update may touch -- so an
    editable role selector here would let anyone self-promote to
    EnterpriseAdmin. Role changes belong in the EnterpriseAdmin-only
    Customer Admin tool (Source/UI/CustomerAdminPage.h), never in
    self-service profile editing.

    The actual form lives in the nested ContentPanel, at a fixed natural
    size, inside a juce::Viewport -- shrinking the window (or the app's
    fixed-size min bounds not being small enough on some display) reveals a
    scrollbar instead of squeezing the two-column layout into negative-size
    rectangles, which used to crash on a small enough window.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

class ProfilePage : public juce::Component
{
public:
    ProfilePage();
    ~ProfilePage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void updateAllText();

    /** Repopulates every field from HostService::getInstance().getCurrent().
        Call this right before switching to this page, so it always reflects
        the latest known profile rather than stale data from a previous visit. */
    void loadFromCurrentHost();

    /** Fired after a successful save, on the message thread. The app shell
        uses this to refresh the TopBar's displayed name/avatar immediately. */
    std::function<void()> onProfileSaved;

private:
    class ContentPanel;
    std::unique_ptr<ContentPanel> content_;
    juce::Viewport viewport_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProfilePage)
};
