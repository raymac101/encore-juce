/*
  ==============================================================================

    ResponsiveLayout.h
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Responsive layout system for adaptive UI across different screen sizes

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Comprehensive screen size categories for responsive design
    Based on standard display resolutions and aspect ratios
*/
enum class ScreenSize
{
    // Mobile and Small Screens
    Mobile,          // < 768px width (phones, small tablets)
    
    // Standard Definition and HD Ready
    WXGA_720,        // 1280 x 720   (16:9)
    WXGA_800,        // 1280 x 800   (16:10)
    SXGA,            // 1280 x 1024  (5:4)
    HD_768_1360,     // 1360 x 768   (16:9)
    HD_768_1366,     // 1366 x 768   (16:9) - Most common laptop
    
    // Extended Wide Screens
    WXGA_Plus,       // 1440 x 900   (16:10)
    HD_864,          // 1536 x 864   (16:9)
    HD_Plus,         // 1600 x 900   (16:9)
    UXGA,            // 1600 x 1200  (4:3)
    WSXGA_Plus,      // 1680 x 1050  (16:10)
    
    // Full HD and WUXGA
    FHD,             // 1920 x 1080  (16:9) - Standard Full HD
    WUXGA,           // 1920 x 1200  (16:10)
    QWXGA,           // 2048 x 1152  (16:9)
    QXGA,            // 2048 x 1536  (4:3)
    
    // Ultra-Wide and High Resolution
    UWFHD,           // 2560 x 1080  (21:9) - Ultra-wide FHD
    QHD,             // 2560 x 1440  (16:9) - Quad HD
    WQXGA,           // 2560 x 1600  (16:10)
    UWQHD,           // 3440 x 1440  (21:9) - Ultra-wide QHD
    
    // 4K and Beyond
    UHD_4K,          // 3840 x 2160  (16:9) - 4K Ultra HD
    UHD_5K,          // 5120 x 2880  (16:9) - 5K Ultra HD
    DUHD,            // 7680 x 2160  (32:9) - Dual Ultra HD
    UHD_8K           // 7680 x 4320  (16:9) - 8K Ultra HD
};

/**
    Screen categories for UI layout purposes
*/
enum class ScreenCategory
{
    Mobile,          // Phone and small tablet layouts
    Standard,        // Standard desktop and laptop screens
    Wide,            // Wide desktop screens and monitors
    UltraWide,       // Ultra-wide monitors (21:9, 32:9)
    HighResolution   // 4K/5K/8K displays
};

//==============================================================================
/**
    Responsive constraints for child components
*/
struct ResponsiveConstraints
{
    // Minimum/maximum sizes
    int minWidth = 0, maxWidth = INT_MAX;
    int minHeight = 0, maxHeight = INT_MAX;
    
    // Responsive behavior
    bool hideOnMobile = false;
    bool stackVerticallyOnTablet = false;
    float desktopWidthRatio = 1.0f;  // Proportion of parent width
    
    // Anchor points (for fluid layouts)
    juce::RectanglePlacement placement = juce::RectanglePlacement::centred;
    
    // Margins
    int marginTop = 0, marginRight = 0, marginBottom = 0, marginLeft = 0;
};

//==============================================================================
/**
    Base class for responsive components that adapt to different screen sizes
*/
class ResponsiveLayout : public juce::Component
{
public:
    //==============================================================================
    ResponsiveLayout();
    ~ResponsiveLayout() override;
    
    //==============================================================================
    // Screen Size Management
    
    /** Get the current screen size category */
    ScreenSize getCurrentScreenSize() const { return currentScreenSize; }
    
    /** Manually set screen size (useful for testing) */
    void setScreenSize(ScreenSize newSize);
    
    /** Get scale factor for current screen size */
    float getScaleFactor() const { return scaleFactor; }
    
    //==============================================================================
    // Child Component Management
    
    /** Add a child component with responsive constraints */
    void addResponsiveChild(juce::Component* component, 
                           const ResponsiveConstraints& constraints);
    
    /** Remove a responsive child component */
    void removeResponsiveChild(juce::Component* component);
    
    /** Update constraints for an existing child */
    void updateChildConstraints(juce::Component* component, 
                               const ResponsiveConstraints& newConstraints);
    
    //==============================================================================
    // Adaptive Sizing Helpers
    
    /** Get size adjusted for current scale factor */
    int getScaledSize(int baseSize) const;
    
    /** Get font scaled for current screen size */
    juce::Font getScaledFont(float baseSize) const;
    
    /** Get margin scaled for current screen size */
    int getScaledMargin(int baseMargin) const;
    
    //==============================================================================
    // Component Overrides
    
    void resized() override;
    void paint(juce::Graphics& g) override;
    
protected:
    //==============================================================================
    // Screen Size Information
    static ScreenSize getScreenSizeFromDimensions(int width, int height);
    static ScreenCategory getScreenCategory(ScreenSize size);
    static juce::String getScreenSizeText(ScreenSize size);
    static juce::String getScreenResolutionText(ScreenSize size);
    static float getAspectRatio(ScreenSize size);
    static bool isUltraWideScreen(ScreenSize size);
    static float getDefaultScale(ScreenSize size);
    
    //==============================================================================
    /** Called when screen size changes - override in subclasses */
    virtual void updateUIForScreenSize() {}
    
    /** Get the ideal layout bounds for a child component */
    juce::Rectangle<int> getChildBounds(juce::Component* child, 
                                       const ResponsiveConstraints& constraints) const;
    
private:
    //==============================================================================
    struct ResponsiveChild
    {
        juce::Component* component;
        ResponsiveConstraints constraints;
        bool isVisible = true;
    };
    
    juce::Array<ResponsiveChild> responsiveChildren;
    ScreenSize currentScreenSize = ScreenSize::FHD;
    float scaleFactor = 1.0f;
    
    //==============================================================================
    /** Detect screen size based on component width */
    ScreenSize detectScreenSize() const;
    
    /** Calculate scale factor for current screen size */
    void calculateScaleFactor();
    
    /** Update layout of all responsive children */
    void updateChildLayouts();
    
    /** Handle screen size change */
    void onScreenSizeChanged(ScreenSize oldSize, ScreenSize newSize);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResponsiveLayout)
};