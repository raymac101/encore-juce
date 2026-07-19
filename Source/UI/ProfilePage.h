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
    void buildAvatarGrid();
    void selectAvatar (int tileIndex);
    void updateSaveButtonState();
    void onSaveClicked();
    void setStatus (const juce::String& message, bool isError = false);

    //==========================================================================
    class AvatarTile : public juce::Component
    {
    public:
        int tileIndex = -1;
        juce::Image image;
        bool selected = false;
        std::function<void (int)> onClicked;

        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent&) override { if (onClicked) onClicked (tileIndex); }
    };

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProfilePage)
};
