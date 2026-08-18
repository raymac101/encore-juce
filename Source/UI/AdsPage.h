/*
  ==============================================================================

    AdsPage.h

    Venue Ads management page (NavBar "Ads" item, Admin/Tester/
    EnterpriseAdmin only -- see AccessRights.h). Shows a tile grid of every
    ad AdsService knows about for the active venue, with upload (drag-drop
    or Browse), edit (schedule/frequency/duration), delete, and preview.

    Reads/writes go entirely through AdsService, the same service
    LyricDisplayComponent's idle screen uses to decide what to actually
    play -- this page's grid IS the venue's view into that rotation.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/AdMetadata.h"
#include "../Localization/LocalizationManager.h"

//==============================================================================
class AdsPage : public juce::Component
{
public:
    AdsPage();
    ~AdsPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void updateAllText();

    /** Called by MainArea::setVenueContext() on every venue load/switch. */
    void setVenueContext (const juce::String& venueId, const juce::String& venueName);

private:
    //==========================================================================
    // One tile in the grid -- thumbnail, name, status badge, edit/delete
    // buttons. Clicking anywhere else on the tile opens the preview dialog.
    class AdTile : public juce::Component
    {
    public:
        AdTile();

        void setAd (const AdMetadata& ad);
        const AdMetadata& getAd() const noexcept { return ad_; }

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseUp (const juce::MouseEvent& e) override;

        std::function<void (const AdMetadata&)> onPreview;
        std::function<void (const AdMetadata&)> onEdit;
        std::function<void (const AdMetadata&)> onDelete;

    private:
        void refreshThumbnail();
        void refreshVideoThumbnail();

        AdMetadata ad_;
        juce::Image thumbnail_;

        juce::Label      nameLabel_;
        juce::Label      statusBadge_;
        juce::TextButton editButton_ { "Edit" };
        juce::TextButton deleteButton_ { juce::String::charToString ((juce::juce_wchar) 0x2715) }; // x

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdTile)
    };

    //==========================================================================
    // Drag-drop + Browse... upload entry point at the top of the page.
    class UploadZone : public juce::Component,
                       public juce::FileDragAndDropTarget
    {
    public:
        UploadZone();

        void paint (juce::Graphics& g) override;
        void resized() override;

        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void filesDropped (const juce::StringArray& files, int x, int y) override;
        void fileDragEnter (const juce::StringArray& files, int x, int y) override;
        void fileDragExit (const juce::StringArray& files) override;

        void updateAllText();

        std::function<void (const juce::File&)> onFileChosen;

    private:
        static bool isSupportedFile (const juce::File& f);

        juce::Label      promptLabel_;
        juce::TextButton browseButton_ { "Browse..." };
        bool             dragHover_ = false;
        std::unique_ptr<juce::FileChooser> fileChooser_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UploadZone)
    };

    //==========================================================================
    void refreshAds();
    void rebuildTiles();
    void handleFileChosen (const juce::File& file);
    void openEditDialog (AdMetadata defaults, bool isNewUpload, const juce::File& uploadFile);
    void doDelete (const AdMetadata& ad);
    void setStatusMessage (const juce::String& message);
    void setBusy (bool busy);

    juce::String venueId_;
    juce::String venueName_;
    int  loadToken_ = 0;
    bool busy_ = false;

    std::vector<AdMetadata> ads_;

    juce::Label title_;
    juce::Label subtitle_;
    juce::Label status_;
    juce::Label emptyStateLabel_;

    std::unique_ptr<UploadZone> uploadZone_;
    juce::OwnedArray<AdTile> tiles_;

    // Plain Component with a paint callback -- lets decorative panels scroll
    // along with the rest of the content. Same pattern as CompanyAdminPage.
    class ContentHolder : public juce::Component
    {
    public:
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    };

    std::unique_ptr<ContentHolder> contentHolder_;
    juce::Viewport viewport_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdsPage)
};
