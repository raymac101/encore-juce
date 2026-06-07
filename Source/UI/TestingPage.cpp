#include "TestingPage.h"
#include "../Localization/LocalizationManager.h"

namespace
{
    void configureNumericSlider(juce::Slider& s, int min, int max, int value)
    {
        s.setSliderStyle(juce::Slider::IncDecButtons);
        s.setIncDecButtonsMode(juce::Slider::incDecButtonsDraggable_AutoDirection);
        s.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 80, 24);
        s.setRange(min, max, 1);
        s.setValue(value, juce::dontSendNotification);
    }
}

TestingPage::TestingPage()
    : progressBar_(progressValue_)
{
    auto& lm = LocalizationManager::getInstance();

    titleLabel_.setText(lm.getText("testing.title"), juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions().withHeight(28.0f)).boldened());

    currentResolutionLabel_.setJustificationType(juce::Justification::centredLeft);
    currentResolutionLabel_.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)));

    resolutionBox_.setTextWhenNothingSelected(lm.getText("testing.select_resolution"));
    populateResolutionDropdown();

    applyResolutionButton_.setButtonText(lm.getText("testing.apply_resolution"));
    applyResolutionButton_.addListener(this);

    mobileLabel_.setText(lm.getText("testing.mobile_singers"), juce::dontSendNotification);
    encoreLabel_.setText(lm.getText("testing.encore_singers"), juce::dontSendNotification);
    songsMinLabel_.setText(lm.getText("testing.songs_min"), juce::dontSendNotification);
    songsMaxLabel_.setText(lm.getText("testing.songs_max"), juce::dontSendNotification);
    pitchLabel_.setText(lm.getText("testing.random_pitch"), juce::dontSendNotification);

    configureNumericSlider(mobileSlider_, 0, 100, 5);
    configureNumericSlider(encoreSlider_, 0, 100, 5);
    configureNumericSlider(songsMinSlider_, 1, 10, 1);
    configureNumericSlider(songsMaxSlider_, 1, 10, 3);

    randomPitchToggle_.setToggleState(false, juce::dontSendNotification);

    createQueueButton_.setButtonText(lm.getText("testing.create_queue"));
    createQueueButton_.addListener(this);

    progressTextLabel_.setText("", juce::dontSendNotification);
    progressTextLabel_.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(titleLabel_);
    addAndMakeVisible(currentResolutionLabel_);
    addAndMakeVisible(resolutionBox_);
    addAndMakeVisible(applyResolutionButton_);
    addAndMakeVisible(mobileLabel_);
    addAndMakeVisible(encoreLabel_);
    addAndMakeVisible(songsMinLabel_);
    addAndMakeVisible(songsMaxLabel_);
    addAndMakeVisible(pitchLabel_);
    addAndMakeVisible(mobileSlider_);
    addAndMakeVisible(encoreSlider_);
    addAndMakeVisible(songsMinSlider_);
    addAndMakeVisible(songsMaxSlider_);
    addAndMakeVisible(randomPitchToggle_);
    addAndMakeVisible(createQueueButton_);
    addAndMakeVisible(progressBar_);
    addAndMakeVisible(progressTextLabel_);

    progressBar_.setVisible(false);

    if (resolutionBox_.getNumItems() > 0)
        resolutionBox_.setSelectedId((int) ScreenSize::FHD + 1, juce::dontSendNotification);

    currentResolutionLabel_.setText(lm.getText("testing.current_window") + currentWindowResolutionText(),
                                    juce::dontSendNotification);
}

void TestingPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121212));

    auto panel = getLocalBounds().reduced(24);
    g.setColour(juce::Colour(0xff1d1d1d));
    g.fillRoundedRectangle(panel.toFloat(), 10.0f);
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawRoundedRectangle(panel.toFloat(), 10.0f, 1.0f);
}

void TestingPage::resized()
{
    auto area = getLocalBounds().reduced(40);

    titleLabel_.setBounds(area.removeFromTop(36));
    area.removeFromTop(8);

    currentResolutionLabel_.setBounds(area.removeFromTop(24));
    area.removeFromTop(6);

    auto resRow = area.removeFromTop(30);
    resolutionBox_.setBounds(resRow.removeFromLeft(420));
    resRow.removeFromLeft(12);
    applyResolutionButton_.setBounds(resRow.removeFromLeft(180));

    area.removeFromTop(22);

    auto addRow = [&area](juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop(34);
        label.setBounds(row.removeFromLeft(320));
        row.removeFromLeft(8);
        slider.setBounds(row.removeFromLeft(200));
        area.removeFromTop(8);
    };

    addRow(mobileLabel_, mobileSlider_);
    addRow(encoreLabel_, encoreSlider_);
    addRow(songsMinLabel_, songsMinSlider_);
    addRow(songsMaxLabel_, songsMaxSlider_);

    auto pitchRow = area.removeFromTop(28);
    pitchLabel_.setBounds(pitchRow.removeFromLeft(320));
    randomPitchToggle_.setBounds(pitchRow.removeFromLeft(40));

    area.removeFromTop(18);
    createQueueButton_.setBounds(area.removeFromTop(32).removeFromLeft(180));

    area.removeFromTop(10);
    progressBar_.setBounds(area.removeFromTop(18).removeFromLeft(420));
    area.removeFromTop(6);
    progressTextLabel_.setBounds(area.removeFromTop(24));

    currentResolutionLabel_.setText(LocalizationManager::getInstance().getText("testing.current_window") + currentWindowResolutionText(),
                                    juce::dontSendNotification);
}

void TestingPage::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    titleLabel_.setText(lm.getText("testing.title"), juce::dontSendNotification);
    resolutionBox_.setTextWhenNothingSelected(lm.getText("testing.select_resolution"));
    applyResolutionButton_.setButtonText(lm.getText("testing.apply_resolution"));
    mobileLabel_.setText(lm.getText("testing.mobile_singers"), juce::dontSendNotification);
    encoreLabel_.setText(lm.getText("testing.encore_singers"), juce::dontSendNotification);
    songsMinLabel_.setText(lm.getText("testing.songs_min"), juce::dontSendNotification);
    songsMaxLabel_.setText(lm.getText("testing.songs_max"), juce::dontSendNotification);
    pitchLabel_.setText(lm.getText("testing.random_pitch"), juce::dontSendNotification);
    createQueueButton_.setButtonText(lm.getText("testing.create_queue"));
    currentResolutionLabel_.setText(lm.getText("testing.current_window") + currentWindowResolutionText(),
                                    juce::dontSendNotification);
}

void TestingPage::buttonClicked(juce::Button* button)
{
    if (button == &applyResolutionButton_)
        applySelectedResolution();
    else if (button == &createQueueButton_)
        triggerCreateQueue();
}

void TestingPage::updateUIForScreenSize()
{
    resized();
}

void TestingPage::populateResolutionDropdown()
{
    resolutionBox_.clear(juce::dontSendNotification);
    resolutionSizes_.clear();

    for (int i = (int) ScreenSize::WXGA_720; i <= (int) ScreenSize::UHD_8K; ++i)
    {
        auto size = static_cast<ScreenSize>(i);
        const auto text = ResponsiveLayout::getScreenSizeText(size)
                        + "  "
                        + ResponsiveLayout::getScreenResolutionText(size);
        resolutionBox_.addItem(text, i + 1);
        resolutionSizes_.add(size);
    }
}

juce::String TestingPage::currentWindowResolutionText() const
{
    if (auto* win = findParentComponentOfClass<juce::TopLevelWindow>())
        return juce::String(win->getWidth()) + " x " + juce::String(win->getHeight());

    return juce::String(getWidth()) + " x " + juce::String(getHeight());
}

bool TestingPage::resolutionForSize(ScreenSize size, int& width, int& height)
{
    const auto text = ResponsiveLayout::getScreenResolutionText(size);
    if (text.containsChar('<') || text == "Unknown")
        return false;

    auto parts = juce::StringArray::fromTokens(text.replaceCharacters("xX", " "), false);
    parts.trim();
    parts.removeEmptyStrings();

    if (parts.size() < 2)
        return false;

    width = parts[0].getIntValue();
    height = parts[1].getIntValue();
    return width > 0 && height > 0;
}

void TestingPage::applySelectedResolution()
{
    const int selectedId = resolutionBox_.getSelectedId();
    if (selectedId <= 0)
        return;

    const auto size = static_cast<ScreenSize>(selectedId - 1);
    int w = 0, h = 0;
    if (!resolutionForSize(size, w, h))
        return;

    if (onApplyResolution)
        onApplyResolution(w, h);

    currentResolutionLabel_.setText("Current window: " + currentWindowResolutionText(),
                                    juce::dontSendNotification);
}

TestingPage::SeedOptions TestingPage::readOptions() const
{
    SeedOptions o;
    o.numMobileSingers = (int) mobileSlider_.getValue();
    o.numEncoreSingers = (int) encoreSlider_.getValue();
    o.numSongsMin      = (int) songsMinSlider_.getValue();
    o.numSongsMax      = (int) songsMaxSlider_.getValue();
    o.randomPitch      = randomPitchToggle_.getToggleState();

    if (o.numSongsMin > o.numSongsMax)
        std::swap(o.numSongsMin, o.numSongsMax);

    return o;
}

void TestingPage::triggerCreateQueue()
{
    if (creating_ || !onCreateQueue)
        return;

    creating_ = true;
    progressValue_ = 0.0;
    progressBar_.setVisible(true);
    createQueueButton_.setEnabled(false);
    progressTextLabel_.setText(LocalizationManager::getInstance().getText("testing.creating_queue"), juce::dontSendNotification);

    const auto options = readOptions();
    juce::Component::SafePointer<TestingPage> safe(this);

    onCreateQueue(
        options,
        [safe](float progress)
        {
            juce::MessageManager::callAsync([safe, progress]()
            {
                if (safe == nullptr) return;
                safe->progressValue_ = juce::jlimit(0.0, 1.0, (double) progress);
                const int pct = (int) std::round(safe->progressValue_ * 100.0);
                safe->progressTextLabel_.setText(juce::String(pct) + LocalizationManager::getInstance().getText("testing.percent_complete"), juce::dontSendNotification);
            });
        },
        [safe](bool ok, juce::String message)
        {
            juce::MessageManager::callAsync([safe, ok, message]()
            {
                if (safe == nullptr) return;
                safe->creating_ = false;
                safe->createQueueButton_.setEnabled(true);
                safe->progressValue_ = ok ? 1.0 : safe->progressValue_;
                safe->progressTextLabel_.setText(message, juce::dontSendNotification);

                juce::AlertWindow::showMessageBoxAsync(
                        ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                        ok ? LocalizationManager::getInstance().getText("testing.queue_created") : LocalizationManager::getInstance().getText("testing.queue_error"),
                    message);
            });
        });
}
