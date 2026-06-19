/*
  ============================================================================== 

    CompanyAdminPage.cpp
    selectedLogoFile_ = juce::File();
  ==============================================================================
*/

#include "CompanyAdminPage.h"
#include "../Services/FirestoreClient.h"
#include "../Firebase/FirebaseConfig.h"

namespace
{
juce::String mimeTypeForFile (const juce::File& file)
{
        const auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".png")  return "image/png";
        if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
        if (ext == ".gif")  return "image/gif";
        if (ext == ".webp") return "image/webp";
        return "application/octet-stream";
}
}

namespace
{
const juce::Colour kBg     { 0xff0f1722 };
const juce::Colour kPanel  { 0xff16213e };
const juce::Colour kBorder { 0xff2c3e57 };
const juce::Colour kAccent { 0xff30daff };
const juce::Colour kText   { 0xffffffff };
const juce::Colour kMuted  { 0xffc7d2e0 };
}

CompanyAdminPage::CompanyAdminPage()
{
    auto& lm = LocalizationManager::getInstance();

    title_.setFont (juce::Font (juce::FontOptions().withHeight (30.0f)).boldened());
    title_.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (title_);

    subtitle_.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)));
    subtitle_.setColour (juce::Label::textColourId, kMuted);
    subtitle_.setText (lm.getText ("page.company_admin.subtitle"), juce::dontSendNotification);
    addAndMakeVisible (subtitle_);

    status_.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    status_.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (status_);

    companyInfoTitle_.setFont (juce::Font (juce::FontOptions().withHeight (16.0f)).boldened());
    companyInfoTitle_.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (companyInfoTitle_);

    companyIdEditor_.setTextToShowWhenEmpty (lm.getText ("company_admin.company_id_hint"), kMuted);
    companyIdEditor_.setColour (juce::TextEditor::backgroundColourId, kPanel);
    companyIdEditor_.setColour (juce::TextEditor::textColourId, kText);
    companyIdEditor_.setColour (juce::TextEditor::outlineColourId, kBorder);
    companyIdEditor_.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
    addAndMakeVisible (companyIdEditor_);

    companyIdLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    companyIdLabel_.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (companyIdLabel_);

    companyNameLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)).boldened());
    companyNameLabel_.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (companyNameLabel_);

    companyStatusLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)).boldened());
    companyStatusLabel_.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (companyStatusLabel_);

    logoLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)).boldened());
    logoLabel_.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (logoLabel_);

    logoPathLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    logoPathLabel_.setColour (juce::Label::textColourId, kMuted);
    logoPathLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoPathLabel_);

    companyNameEditor_.setTextToShowWhenEmpty (lm.getText ("company_admin.company_name_hint"), kMuted);
    companyNameEditor_.setColour (juce::TextEditor::backgroundColourId, kPanel);
    companyNameEditor_.setColour (juce::TextEditor::textColourId, kText);
    companyNameEditor_.setColour (juce::TextEditor::outlineColourId, kBorder);
    companyNameEditor_.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
    addAndMakeVisible (companyNameEditor_);

    companyStatusBox_.addItem (lm.getText ("company_admin.status_active"), 1);
    companyStatusBox_.addItem (lm.getText ("company_admin.status_suspended"), 2);
    companyStatusBox_.setColour (juce::ComboBox::backgroundColourId, kPanel);
    companyStatusBox_.setColour (juce::ComboBox::textColourId, kText);
    companyStatusBox_.setColour (juce::ComboBox::outlineColourId, kBorder);
    addAndMakeVisible (companyStatusBox_);

    membersTitle_.setFont (juce::Font (juce::FontOptions().withHeight (16.0f)).boldened());
    membersTitle_.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (membersTitle_);

    memberUserIdLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)).boldened());
    memberUserIdLabel_.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (memberUserIdLabel_);

    memberRoleLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)).boldened());
    memberRoleLabel_.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (memberRoleLabel_);

    memberUserIdEditor_.setTextToShowWhenEmpty (lm.getText ("company_admin.member_user_id_hint"), kMuted);
    memberUserIdEditor_.setColour (juce::TextEditor::backgroundColourId, kPanel);
    memberUserIdEditor_.setColour (juce::TextEditor::textColourId, kText);
    memberUserIdEditor_.setColour (juce::TextEditor::outlineColourId, kBorder);
    memberUserIdEditor_.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
    addAndMakeVisible (memberUserIdEditor_);

    memberRoleBox_.addItem ("company_admin", 1);
    memberRoleBox_.addItem ("host", 2);
    memberRoleBox_.addItem ("viewer", 3);
    memberRoleBox_.setSelectedId (1, juce::dontSendNotification);
    memberRoleBox_.setColour (juce::ComboBox::backgroundColourId, kPanel);
    memberRoleBox_.setColour (juce::ComboBox::textColourId, kText);
    memberRoleBox_.setColour (juce::ComboBox::outlineColourId, kBorder);
    addAndMakeVisible (memberRoleBox_);

    membersListLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    membersListLabel_.setColour (juce::Label::textColourId, kText);
    membersListLabel_.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (membersListLabel_);

    for (auto* button : { &applyCompanyIdButton_, &browseLogoButton_, &clearLogoButton_, &saveCompanyButton_,
                          &saveMemberButton_, &refreshMembersButton_,
                          &refreshButton_, &inviteButton_, &registerButton_, &uploadButton_ })
    {
        button->setColour (juce::TextButton::buttonColourId, kPanel);
        button->setColour (juce::TextButton::textColourOffId, kText);
        button->setColour (juce::TextButton::textColourOnId, kText);
        addAndMakeVisible (*button);
    }

    applyCompanyIdButton_.onClick = [this]() { applyCompanyIdFromEditor(); };

    browseLogoButton_.onClick = [this]()
    {
        fileChooser_ = std::make_unique<juce::FileChooser> (
            LocalizationManager::getInstance().getText ("company_admin.select_logo"),
            juce::File::getSpecialLocation (juce::File::userHomeDirectory),
            "*.png;*.jpg;*.jpeg;*.gif;*.webp");

        fileChooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (! file.existsAsFile())
                    return;

                selectedLogoFile_ = file;
                companyLogoFileName_ = file.getFileName();
                logoPathLabel_.setText (companyLogoFileName_, juce::dontSendNotification);
                updateLogoPreviewFromFile (file);
            });
    };

    clearLogoButton_.onClick = [this]() { clearLogo(); };
    saveCompanyButton_.onClick = [this]() { saveCompanyInfo(); };
        selectedLogoFile_ = juce::File();
    refreshMembersButton_.onClick = [this]() { loadMembers(); };

    configureCard (venueCard_,    lm.getText ("company_admin.venues"),    "0");
    configureCard (hostCard_,     lm.getText ("company_admin.hosts"),     "0");
    configureCard (deviceCard_,   lm.getText ("company_admin.devices"),   "0");
    configureCard (packageCard_,  lm.getText ("company_admin.packages"),  "0");
    configureCard (campaignCard_, lm.getText ("company_admin.campaigns"), "0");

    setCompanyContext ({}, {});
}

void CompanyAdminPage::updateLogoPreviewFromFile (const juce::File& file)
{
    logoPreview_ = {};
    if (file.existsAsFile())
        logoPreview_ = juce::ImageFileFormat::loadFrom (file);
    repaint();
}

void CompanyAdminPage::configureCard (StatCard& card, const juce::String& title, const juce::String& initialValue)
{
    card.title.setText (title, juce::dontSendNotification);
    card.title.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)).boldened());
    card.title.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (card.title);

    card.value.setText (initialValue, juce::dontSendNotification);
    card.value.setFont (juce::Font (juce::FontOptions().withHeight (28.0f)).boldened());
    card.value.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (card.value);
}

void CompanyAdminPage::setCompanyContext (const juce::String& companyId, const juce::String& companyRole)
{
    companyId_ = companyId;
    companyRole_ = companyRole;

    auto& lm = LocalizationManager::getInstance();
    title_.setText (lm.getText ("page.company_admin"), juce::dontSendNotification);

    juce::String context = companyId_.isNotEmpty() ? companyId_ : lm.getText ("company_admin.no_company");
    if (companyRole_.isNotEmpty())
        context << "  |  Role: " << companyRole_;
    subtitle_.setText (context, juce::dontSendNotification);

    companyIdEditor_.setText (companyId_, juce::dontSendNotification);

    loadCompanyInfo();
    loadMembers();
    repaint();
}

void CompanyAdminPage::applyCompanyIdFromEditor()
{
    const auto nextCompanyId = companyIdEditor_.getText().trim();
    if (nextCompanyId.isEmpty())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.company_id_required"));
        return;
    }

    companyId_ = nextCompanyId;
    companyDocExists_ = false;
    if (onCompanyIdChanged)
        onCompanyIdChanged (companyId_);

    juce::String context = companyId_;
    if (companyRole_.isNotEmpty())
        context << "  |  Role: " << companyRole_;
    subtitle_.setText (context, juce::dontSendNotification);

    loadCompanyInfo();
    loadMembers();
}

void CompanyAdminPage::loadCompanyInfo()
{
    companyDocExists_ = false;
    companyLogoUrl_.clear();
    companyLogoStoragePath_.clear();
    selectedLogoFile_ = juce::File();
    logoPreview_ = {};

    if (companyId_.isEmpty())
        return;

    companyInfoTitle_.setText (LocalizationManager::getInstance().getText ("company_admin.edit_info"), juce::dontSendNotification);
    companyIdLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.company_id") + ": " + companyId_, juce::dontSendNotification);
    status_.setText (LocalizationManager::getInstance().getText ("company_admin.loading_info"), juce::dontSendNotification);

    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    const auto companyId = companyId_;
    juce::Thread::launch ([safe, companyId]()
    {
        auto& fc = FirestoreClient::getInstance();
        int httpStatus = 0;
        auto doc = fc.getDocument ("companies/" + companyId, &httpStatus);

        juce::String name;
        juce::String status;
        juce::String logoUrl;
        juce::String logoStoragePath;
        juce::String logoFileName;
        const bool exists = (httpStatus == 200 && doc.isObject() && doc.hasProperty ("fields"));

        if (exists)
        {
            name = FirestoreClient::readString (doc, "name");
            status = FirestoreClient::readString (doc, "status");
            logoUrl = FirestoreClient::readString (doc, "logoUrl");
            logoStoragePath = FirestoreClient::readString (doc, "logoStoragePath");
            logoFileName = FirestoreClient::readString (doc, "logoFileName");
        }

        juce::MessageManager::callAsync ([safe, name, status, logoUrl, logoStoragePath, logoFileName, exists]()
        {
            if (safe == nullptr)
                return;

            safe->companyDocExists_ = exists;
            safe->companyNameEditor_.setText (name, juce::dontSendNotification);
            safe->companyStatusBox_.setSelectedId (status.equalsIgnoreCase ("suspended") ? 2 : 1, juce::dontSendNotification);
            safe->companyLogoUrl_ = logoUrl;
            safe->companyLogoStoragePath_ = logoStoragePath;
            safe->companyLogoFileName_ = logoFileName;
            const auto logoText = logoFileName.isNotEmpty() ? logoFileName
                : logoUrl.isNotEmpty() ? logoUrl
                : LocalizationManager::getInstance().getText ("company_admin.no_logo");
            safe->logoPathLabel_.setText (logoText, juce::dontSendNotification);
            safe->logoPreview_ = {};
            safe->repaint();
            safe->status_.setText (exists ? LocalizationManager::getInstance().getText ("company_admin.info_loaded")
                                          : LocalizationManager::getInstance().getText ("company_admin.create_info"), juce::dontSendNotification);
        });
    });
}

void CompanyAdminPage::clearLogo()
{
    companyLogoUrl_.clear();
    companyLogoStoragePath_.clear();
    companyLogoFileName_.clear();
    selectedLogoFile_ = juce::File();
    logoPreview_ = {};
    logoPathLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.no_logo"), juce::dontSendNotification);
    repaint();
}

bool CompanyAdminPage::uploadLogoToStorage (const juce::String& companyId,
                                            const juce::File& logoFile,
                                            juce::String& outLogoUrl,
                                            juce::String& outStoragePath,
                                            juce::String& outError) const
{
    juce::MemoryBlock data;
    if (! logoFile.existsAsFile() || ! logoFile.loadFileAsData (data))
    {
        outError = "Could not read logo file.";
        return false;
    }

    const auto ext = logoFile.getFileExtension().toLowerCase();
    const auto objectName = "companies/" + companyId + "/artwork/logo-"
                            + juce::String (juce::Time::currentTimeMillis()) + ext;
    const auto bucket = FirebaseConfig::storageBucket;
    auto url = juce::URL ("https://firebasestorage.googleapis.com/v0/b/" + bucket
                          + "/o?uploadType=media&name=" + juce::URL::addEscapeChars (objectName, true));
    url = url.withPOSTData (data);

    int status = 0;
    const auto headers = "Authorization: Bearer " + FirestoreClient::getInstance().getIdToken() + "\r\n"
                         + "Content-Type: " + mimeTypeForFile (logoFile) + "\r\n"
                         + "Accept: application/json";

    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs (20000)
        .withExtraHeaders (headers)
        .withHttpRequestCmd ("POST")
        .withStatusCode (&status);

    auto stream = std::unique_ptr<juce::InputStream> (url.createInputStream (opts));
    if (stream == nullptr)
    {
        outError = "Storage upload connection failed.";
        return false;
    }

    const auto response = stream->readEntireStreamAsString();
    if (status < 200 || status >= 300)
    {
        outError = "Storage upload failed (HTTP " + juce::String (status) + ")";
        return false;
    }

    juce::var parsed;
    if (juce::JSON::parse (response, parsed).failed() || ! parsed.isObject())
    {
        outError = "Storage upload response parse failed.";
        return false;
    }

    outStoragePath = parsed.getProperty ("name", objectName).toString();
    const auto token = parsed.getProperty ("downloadTokens", "").toString();
    outLogoUrl = "https://firebasestorage.googleapis.com/v0/b/" + bucket + "/o/"
                 + juce::URL::addEscapeChars (outStoragePath, true) + "?alt=media";
    if (token.isNotEmpty())
        outLogoUrl << "&token=" << juce::URL::addEscapeChars (token, true);

    return true;
}

void CompanyAdminPage::saveCompanyInfo()
{
    const auto companyId = companyIdEditor_.getText().trim();
    if (companyId.isEmpty())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.company_id_required"));
        return;
    }

    const auto companyName = companyNameEditor_.getText().trim();
    if (companyName.isEmpty())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.name_required"));
        return;
    }

    const auto statusValue = companyStatusBox_.getSelectedId() == 2 ? juce::String ("suspended") : juce::String ("active");

    setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.saving_info"));
    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    juce::Thread::launch ([safe, companyId, companyName, statusValue]()
    {
        if (safe == nullptr)
            return;

        auto& fc = FirestoreClient::getInstance();
        juce::String logoUrl = safe->companyLogoUrl_;
        juce::String logoStoragePath = safe->companyLogoStoragePath_;
        juce::String logoFileName = safe->companyLogoFileName_;

        if (safe->selectedLogoFile_.existsAsFile())
        {
            juce::String uploadError;
            if (! safe->uploadLogoToStorage (companyId, safe->selectedLogoFile_, logoUrl, logoStoragePath, uploadError))
            {
                juce::MessageManager::callAsync ([safe, uploadError]()
                {
                    if (safe != nullptr)
                        safe->setStatusMessage (uploadError);
                });
                return;
            }
        }

        auto fields = FirestoreClient::makeFields ({
            { "name",            FirestoreClient::stringValue (companyName) },
            { "status",          FirestoreClient::stringValue (statusValue) },
            { "ownerUserId",     FirestoreClient::stringValue (FirestoreClient::getInstance().getUserId()) },
            { "updatedAt",       FirestoreClient::timestampValue (juce::Time::getCurrentTime()) },
            { "logoUrl",         logoUrl.isNotEmpty() ? FirestoreClient::stringValue (logoUrl) : FirestoreClient::nullValue() },
            { "logoStoragePath", logoStoragePath.isNotEmpty() ? FirestoreClient::stringValue (logoStoragePath) : FirestoreClient::nullValue() },
            { "logoFileName",    logoFileName.isNotEmpty() ? FirestoreClient::stringValue (logoFileName) : FirestoreClient::nullValue() }
        });

        bool ok = false;
        if (safe != nullptr && safe->companyDocExists_)
            ok = fc.patchDocument ("companies/" + companyId, fields);
        else
            ok = fc.createDocument ("companies", fields, companyId).isObject();

        juce::MessageManager::callAsync ([safe, ok, companyId, logoUrl, logoStoragePath]()
        {
            if (safe == nullptr)
                return;

            if (ok)
            {
                safe->companyDocExists_ = true;
                safe->companyId_ = companyId;
                safe->companyIdEditor_.setText (companyId, juce::dontSendNotification);
                safe->companyLogoUrl_ = logoUrl;
                safe->companyLogoStoragePath_ = logoStoragePath;
                safe->selectedLogoFile_ = juce::File();
                if (safe->onCompanyIdChanged)
                    safe->onCompanyIdChanged (companyId);
                safe->setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.saved"));
                safe->loadMembers();
            }
            else
            {
                safe->setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.save_failed"));
            }
        });
    });
}

void CompanyAdminPage::loadMembers()
{
    if (companyId_.isEmpty())
    {
        membersListLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.member_none"), juce::dontSendNotification);
        return;
    }

    membersListLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.members_loading"), juce::dontSendNotification);
    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    const auto companyId = companyId_;
    juce::Thread::launch ([safe, companyId]()
    {
        auto docs = FirestoreClient::getInstance().listCollection ("companies/" + companyId + "/members", 500);
        juce::StringArray lines;
        for (auto& d : docs)
        {
            const auto memberId = d.getProperty ("name", "").toString().fromLastOccurrenceOf ("/", false, false);
            const auto userId = FirestoreClient::readString (d, "userId");
            const auto role = FirestoreClient::readString (d, "role");
            const auto status = FirestoreClient::readString (d, "status");
            lines.add (memberId + "  |  " + (userId.isNotEmpty() ? userId : memberId)
                       + "  |  " + role + "  |  " + status);
        }

        juce::MessageManager::callAsync ([safe, lines]()
        {
            if (safe == nullptr)
                return;

            if (lines.isEmpty())
                safe->membersListLabel_.setText (LocalizationManager::getInstance().getText ("company_admin.member_none"), juce::dontSendNotification);
            else
                safe->membersListLabel_.setText (lines.joinIntoString ("\n"), juce::dontSendNotification);
        });
    });
}

void CompanyAdminPage::saveMemberMapping()
{
    if (companyId_.isEmpty())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.company_id_required"));
        return;
    }

    const auto memberUserId = memberUserIdEditor_.getText().trim();
    if (memberUserId.isEmpty())
    {
        setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.member_user_id_required"));
        return;
    }

    const auto role = memberRoleBox_.getText().trim();
    juce::Component::SafePointer<CompanyAdminPage> safe (this);
    const auto companyId = companyId_;
    setStatusMessage (LocalizationManager::getInstance().getText ("company_admin.member_saving"));

    juce::Thread::launch ([safe, companyId, memberUserId, role]()
    {
        auto fields = FirestoreClient::makeFields ({
            { "userId",      FirestoreClient::stringValue (memberUserId) },
            { "role",        FirestoreClient::stringValue (role) },
            { "status",      FirestoreClient::stringValue ("active") },
            { "updatedAt",   FirestoreClient::timestampValue (juce::Time::getCurrentTime()) },
            { "updatedBy",   FirestoreClient::stringValue (FirestoreClient::getInstance().getUserId()) }
        });

        const bool ok = FirestoreClient::getInstance().createDocument (
            "companies/" + companyId + "/members", fields, memberUserId).isObject();

        juce::MessageManager::callAsync ([safe, ok]()
        {
            if (safe == nullptr)
                return;
            safe->setStatusMessage (ok ? LocalizationManager::getInstance().getText ("company_admin.member_saved")
                                        : LocalizationManager::getInstance().getText ("company_admin.member_save_failed"));
            if (ok)
            {
                safe->memberUserIdEditor_.clear();
                safe->loadMembers();
            }
        });
    });
}

void CompanyAdminPage::setSummary (const Summary& summary)
{
    summary_ = summary;
    venueCard_.value.setText (juce::String (summary_.venues), juce::dontSendNotification);
    hostCard_.value.setText (juce::String (summary_.hosts), juce::dontSendNotification);
    deviceCard_.value.setText (juce::String (summary_.devices), juce::dontSendNotification);
    packageCard_.value.setText (juce::String (summary_.songPackages), juce::dontSendNotification);
    campaignCard_.value.setText (juce::String (summary_.campaigns), juce::dontSendNotification);
}

void CompanyAdminPage::setStatusMessage (const juce::String& message)
{
    statusMessage_ = message;
    status_.setText (statusMessage_, juce::dontSendNotification);
}

void CompanyAdminPage::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    title_.setText (lm.getText ("page.company_admin"), juce::dontSendNotification);
    subtitle_.setText ((companyId_.isNotEmpty() ? companyId_ : lm.getText ("company_admin.no_company"))
                       + (companyRole_.isNotEmpty() ? "  |  Role: " + companyRole_ : ""),
                       juce::dontSendNotification);
    status_.setText (statusMessage_, juce::dontSendNotification);
    companyInfoTitle_.setText (lm.getText ("company_admin.edit_info"), juce::dontSendNotification);
    companyIdLabel_.setText (lm.getText ("company_admin.company_id") + ": " + companyId_, juce::dontSendNotification);
    companyIdEditor_.setTextToShowWhenEmpty (lm.getText ("company_admin.company_id_hint"), kMuted);
    applyCompanyIdButton_.setButtonText (lm.getText ("company_admin.use_company_id"));
    companyNameLabel_.setText (lm.getText ("company_admin.company_name"), juce::dontSendNotification);
    companyStatusLabel_.setText (lm.getText ("company_admin.company_status"), juce::dontSendNotification);
    logoLabel_.setText (lm.getText ("company_admin.logo"), juce::dontSendNotification);
    if (logoPathLabel_.getText().isEmpty())
        logoPathLabel_.setText (lm.getText ("company_admin.no_logo"), juce::dontSendNotification);
    venueCard_.title.setText (lm.getText ("company_admin.venues"), juce::dontSendNotification);
    hostCard_.title.setText (lm.getText ("company_admin.hosts"), juce::dontSendNotification);
    deviceCard_.title.setText (lm.getText ("company_admin.devices"), juce::dontSendNotification);
    packageCard_.title.setText (lm.getText ("company_admin.packages"), juce::dontSendNotification);
    campaignCard_.title.setText (lm.getText ("company_admin.campaigns"), juce::dontSendNotification);
    browseLogoButton_.setButtonText (lm.getText ("company_admin.select_logo"));
    clearLogoButton_.setButtonText (lm.getText ("company_admin.clear_logo"));
    saveCompanyButton_.setButtonText (lm.getText ("company_admin.save_info"));
    membersTitle_.setText (lm.getText ("company_admin.members_title"), juce::dontSendNotification);
    memberUserIdLabel_.setText (lm.getText ("company_admin.member_user_id"), juce::dontSendNotification);
    memberRoleLabel_.setText (lm.getText ("company_admin.member_role"), juce::dontSendNotification);
    memberUserIdEditor_.setTextToShowWhenEmpty (lm.getText ("company_admin.member_user_id_hint"), kMuted);
    saveMemberButton_.setButtonText (lm.getText ("company_admin.member_save"));
    refreshMembersButton_.setButtonText (lm.getText ("company_admin.member_refresh"));
    refreshButton_.setButtonText (lm.getText ("company_admin.refresh"));
    inviteButton_.setButtonText (lm.getText ("company_admin.invite_host"));
    registerButton_.setButtonText (lm.getText ("company_admin.register_device"));
    uploadButton_.setButtonText (lm.getText ("company_admin.upload_songs"));
}

void CompanyAdminPage::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    auto bounds = getLocalBounds().reduced (22);
    g.setColour (kBorder);
    g.drawRoundedRectangle (bounds.toFloat(), 20.0f, 1.5f);

    auto header = bounds.removeFromTop (110);
    g.setColour (kPanel);
    g.fillRoundedRectangle (header.toFloat(), 20.0f);

    g.setColour (kAccent.withAlpha (0.15f));
    g.fillRoundedRectangle (header.removeFromTop (6).toFloat(), 20.0f);

    auto infoArea = getLocalBounds().reduced (28);
    infoArea.removeFromTop (110);
    infoArea.removeFromTop (12);
    auto formArea = infoArea.removeFromTop (220);
    g.setColour (kPanel);
    g.fillRoundedRectangle (formArea.toFloat(), 18.0f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (formArea.toFloat(), 18.0f, 1.0f);

    auto preview = formArea.removeFromRight (180).reduced (16);
    g.setColour (kBorder);
    g.drawRoundedRectangle (preview.toFloat(), 14.0f, 1.0f);
    if (logoPreview_.isValid())
    {
        g.drawImageWithin (logoPreview_, preview.getX() + 8, preview.getY() + 8,
                           preview.getWidth() - 16, preview.getHeight() - 16,
                           juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
                           false);
    }
    else
    {
        g.setColour (kMuted.withAlpha (0.55f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)));
        g.drawFittedText (LocalizationManager::getInstance().getText ("company_admin.logo_preview"),
                          preview.reduced (10), juce::Justification::centred, 2);
    }

    infoArea.removeFromTop (12);
    g.setColour (kPanel);
    g.fillRoundedRectangle (infoArea.toFloat(), 18.0f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (infoArea.toFloat(), 18.0f, 1.0f);
}

void CompanyAdminPage::layoutCard (StatCard& card, juce::Rectangle<int> area)
{
    card.title.setBounds (area.removeFromTop (24));
    card.value.setBounds (area.reduced (0, 8));
}

void CompanyAdminPage::resized()
{
    auto bounds = getLocalBounds().reduced (28);
    auto header = bounds.removeFromTop (110).reduced (18, 16);

    title_.setBounds (header.removeFromTop (38));
    subtitle_.setBounds (header.removeFromTop (24));
    status_.setBounds (header.removeFromTop (20));

    auto buttonsRow = header.removeFromTop (34);
    refreshButton_.setBounds (buttonsRow.removeFromLeft (120));
    buttonsRow.removeFromLeft (8);
    inviteButton_.setBounds (buttonsRow.removeFromLeft (120));
    buttonsRow.removeFromLeft (8);
    registerButton_.setBounds (buttonsRow.removeFromLeft (140));
    buttonsRow.removeFromLeft (8);
    uploadButton_.setBounds (buttonsRow.removeFromLeft (130));

    bounds.removeFromTop (16);

    auto formArea = bounds.removeFromTop (220);
    auto _previewSpace = formArea.removeFromRight (180);
    juce::ignoreUnused (_previewSpace);

    companyInfoTitle_.setBounds (formArea.removeFromTop (24));
    companyIdEditor_.setBounds (formArea.removeFromTop (28).reduced (0, 2));
    formArea.removeFromTop (4);
    applyCompanyIdButton_.setBounds (formArea.removeFromTop (28).withWidth (180));
    formArea.removeFromTop (4);
    companyIdLabel_.setBounds (formArea.removeFromTop (20));
    formArea.removeFromTop (6);
    companyNameLabel_.setBounds (formArea.removeFromTop (20));
    companyNameEditor_.setBounds (formArea.removeFromTop (28).reduced (0, 2));
    formArea.removeFromTop (4);
    companyStatusLabel_.setBounds (formArea.removeFromTop (20));
    companyStatusBox_.setBounds (formArea.removeFromTop (28).withWidth (160));
    formArea.removeFromTop (6);
    logoLabel_.setBounds (formArea.removeFromTop (20));
    logoPathLabel_.setBounds (formArea.removeFromTop (22));
    auto logoButtons = formArea.removeFromTop (30);
    browseLogoButton_.setBounds (logoButtons.removeFromLeft (120));
    logoButtons.removeFromLeft (8);
    clearLogoButton_.setBounds (logoButtons.removeFromLeft (100));
    logoButtons.removeFromLeft (8);
    saveCompanyButton_.setBounds (logoButtons.removeFromLeft (150));

    bounds.removeFromTop (12);
    auto membersArea = bounds.removeFromTop (160).reduced (12);
    membersTitle_.setBounds (membersArea.removeFromTop (22));
    membersArea.removeFromTop (4);
    memberUserIdLabel_.setBounds (membersArea.removeFromTop (18));
    memberUserIdEditor_.setBounds (membersArea.removeFromTop (26));
    membersArea.removeFromTop (4);
    memberRoleLabel_.setBounds (membersArea.removeFromTop (18));
    memberRoleBox_.setBounds (membersArea.removeFromTop (24).withWidth (160));
    membersArea.removeFromTop (6);
    auto memberButtons = membersArea.removeFromTop (28);
    saveMemberButton_.setBounds (memberButtons.removeFromLeft (170));
    memberButtons.removeFromLeft (8);
    refreshMembersButton_.setBounds (memberButtons.removeFromLeft (140));
    membersArea.removeFromTop (8);
    membersListLabel_.setBounds (membersArea);

    bounds.removeFromTop (12);
    auto grid = bounds;
    auto row1 = grid.removeFromTop ((grid.getHeight() / 2) - 8);
    auto row2 = grid;
    auto cardW = (row1.getWidth() - 16) / 3;
    layoutCard (venueCard_,   row1.removeFromLeft (cardW));
    row1.removeFromLeft (8);
    layoutCard (hostCard_,    row1.removeFromLeft (cardW));
    row1.removeFromLeft (8);
    layoutCard (deviceCard_,  row1);

    auto cardW2 = (row2.getWidth() - 8) / 2;
    layoutCard (packageCard_, row2.removeFromLeft (cardW2));
    row2.removeFromLeft (8);
    layoutCard (campaignCard_, row2);
}