/*
  ==============================================================================

    ResponsiveLayout.cpp
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Responsive layout system implementation

  ==============================================================================
*/

#include "ResponsiveLayout.h"

//==============================================================================
ResponsiveLayout::ResponsiveLayout()
{
    // Start with Full HD as default size
    currentScreenSize = ScreenSize::FHD;
    calculateScaleFactor();
}

ResponsiveLayout::~ResponsiveLayout()
{
}

//==============================================================================
ScreenSize ResponsiveLayout::detectScreenSize() const
{
    return getScreenSizeFromDimensions(getWidth(), getHeight());
}

//==============================================================================
void ResponsiveLayout::setScreenSize(ScreenSize newSize)
{
    if (newSize != currentScreenSize)
    {
        ScreenSize oldSize = currentScreenSize;
        currentScreenSize = newSize;
        calculateScaleFactor();
        onScreenSizeChanged(oldSize, newSize);
        updateChildLayouts();
        updateUIForScreenSize();
        repaint();
    }
}

//==============================================================================
void ResponsiveLayout::calculateScaleFactor()
{
    scaleFactor = getDefaultScale(currentScreenSize);
}

//==============================================================================
void ResponsiveLayout::addResponsiveChild(juce::Component* component, 
                                        const ResponsiveConstraints& constraints)
{
    if (component != nullptr)
    {
        ResponsiveChild child;
        child.component = component;
        child.constraints = constraints;
        child.isVisible = !constraints.hideOnMobile || currentScreenSize != ScreenSize::Mobile;
        
        responsiveChildren.add(child);
        addAndMakeVisible(component);
        
        // Update layout immediately
        updateChildLayouts();
    }
}

//==============================================================================
void ResponsiveLayout::removeResponsiveChild(juce::Component* component)
{
    for (int i = responsiveChildren.size() - 1; i >= 0; --i)
    {
        if (responsiveChildren[i].component == component)
        {
            removeChildComponent(component);
            responsiveChildren.remove(i);
            break;
        }
    }
}

//==============================================================================
void ResponsiveLayout::updateChildConstraints(juce::Component* component, 
                                            const ResponsiveConstraints& newConstraints)
{
    for (auto& child : responsiveChildren)
    {
        if (child.component == component)
        {
            child.constraints = newConstraints;
            updateChildLayouts();
            break;
        }
    }
}

//==============================================================================
int ResponsiveLayout::getScaledSize(int baseSize) const
{
    return static_cast<int>(baseSize * scaleFactor);
}

//==============================================================================
juce::Font ResponsiveLayout::getScaledFont(float baseSize) const
{
    return juce::Font(baseSize * scaleFactor);
}

//==============================================================================
int ResponsiveLayout::getScaledMargin(int baseMargin) const
{
    return static_cast<int>(baseMargin * scaleFactor);
}

//==============================================================================
void ResponsiveLayout::resized()
{
    // Detect if screen size category has changed
    ScreenSize detectedSize = detectScreenSize();
    if (detectedSize != currentScreenSize)
    {
        setScreenSize(detectedSize);
    }
    else
    {
        // Just update child layouts
        updateChildLayouts();
    }
}

//==============================================================================
void ResponsiveLayout::paint(juce::Graphics& g)
{
    // Base class can draw background or debug info
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    
    // Debug: Show screen size category in corner (remove in production)
    if (false)  // Set to true for debugging
    {
        g.setColour(juce::Colours::yellow);
        g.setFont(12.0f);
        
        juce::String sizeText;
        switch (currentScreenSize)
        {
            case ScreenSize::Mobile: sizeText = "Mobile"; break;
            case ScreenSize::HD_768_1366: sizeText = "Tablet"; break;
            case ScreenSize::FHD: sizeText = "Desktop"; break;
            case ScreenSize::QHD: sizeText = "Large"; break;
            case ScreenSize::UWFHD: sizeText = "Ultra-wide"; break;
        }
        
        g.drawText(sizeText + " (" + juce::String(scaleFactor, 1) + "x)", 
                   getLocalBounds().reduced(5), 
                   juce::Justification::topRight, false);
    }
}

//==============================================================================
void ResponsiveLayout::updateChildLayouts()
{
    for (auto& child : responsiveChildren)
    {
        if (child.component != nullptr)
        {
            // Check visibility based on screen size
            bool shouldBeVisible = true;
            if (child.constraints.hideOnMobile && currentScreenSize == ScreenSize::Mobile)
                shouldBeVisible = false;
            
            child.isVisible = shouldBeVisible;
            child.component->setVisible(shouldBeVisible);
            
            if (shouldBeVisible)
            {
                // Calculate bounds for this child
                juce::Rectangle<int> childBounds = getChildBounds(child.component, child.constraints);
                child.component->setBounds(childBounds);
            }
        }
    }
}

//==============================================================================
juce::Rectangle<int> ResponsiveLayout::getChildBounds(juce::Component* child, 
                                                     const ResponsiveConstraints& constraints) const
{
    juce::Rectangle<int> parentBounds = getLocalBounds();
    
    // Apply margins
    parentBounds.removeFromLeft(getScaledMargin(constraints.marginLeft));
    parentBounds.removeFromTop(getScaledMargin(constraints.marginTop));
    parentBounds.removeFromRight(getScaledMargin(constraints.marginRight));
    parentBounds.removeFromBottom(getScaledMargin(constraints.marginBottom));
    
    // Calculate initial size
    int width = static_cast<int>(parentBounds.getWidth() * constraints.desktopWidthRatio);
    int height = parentBounds.getHeight();
    
    // Apply size constraints
    width = juce::jmax(width, getScaledSize(constraints.minWidth));
    width = juce::jmin(width, getScaledSize(constraints.maxWidth));
    height = juce::jmax(height, getScaledSize(constraints.minHeight));
    height = juce::jmin(height, getScaledSize(constraints.maxHeight));
    
    // Create rectangle with calculated size
    juce::Rectangle<int> childBounds(0, 0, width, height);
    
    // Apply placement within parent bounds
    return constraints.placement.appliedTo(childBounds, parentBounds);
}

//==============================================================================
// Static Screen Size Methods
//==============================================================================
ScreenSize ResponsiveLayout::getScreenSizeFromDimensions(int width, int height)
{
    // Mobile devices
    if (width < 768)
        return ScreenSize::Mobile;
    
    // Match specific standard resolutions with tolerance for scaling
    auto matchesResolution = [](int w, int h, int targetW, int targetH, float tolerance = 0.05f) {
        float wRatio = std::abs(static_cast<float>(w - targetW)) / targetW;
        float hRatio = std::abs(static_cast<float>(h - targetH)) / targetH;
        return (wRatio <= tolerance && hRatio <= tolerance);
    };
    
    // Check specific resolutions in order of likelihood
    
    // Standard HD resolutions
    if (matchesResolution(width, height, 1366, 768)) return ScreenSize::HD_768_1366;
    if (matchesResolution(width, height, 1360, 768)) return ScreenSize::HD_768_1360;
    if (matchesResolution(width, height, 1280, 720)) return ScreenSize::WXGA_720;
    if (matchesResolution(width, height, 1280, 800)) return ScreenSize::WXGA_800;
    if (matchesResolution(width, height, 1280, 1024)) return ScreenSize::SXGA;
    
    // Extended resolutions
    if (matchesResolution(width, height, 1440, 900)) return ScreenSize::WXGA_Plus;
    if (matchesResolution(width, height, 1536, 864)) return ScreenSize::HD_864;
    if (matchesResolution(width, height, 1600, 900)) return ScreenSize::HD_Plus;
    if (matchesResolution(width, height, 1600, 1200)) return ScreenSize::UXGA;
    if (matchesResolution(width, height, 1680, 1050)) return ScreenSize::WSXGA_Plus;
    
    // Full HD and WUXGA
    if (matchesResolution(width, height, 1920, 1080)) return ScreenSize::FHD;
    if (matchesResolution(width, height, 1920, 1200)) return ScreenSize::WUXGA;
    if (matchesResolution(width, height, 2048, 1152)) return ScreenSize::QWXGA;
    if (matchesResolution(width, height, 2048, 1536)) return ScreenSize::QXGA;
    
    // Ultra-wide and high resolution
    if (matchesResolution(width, height, 2560, 1080)) return ScreenSize::UWFHD;
    if (matchesResolution(width, height, 2560, 1440)) return ScreenSize::QHD;
    if (matchesResolution(width, height, 2560, 1600)) return ScreenSize::WQXGA;
    if (matchesResolution(width, height, 3440, 1440)) return ScreenSize::UWQHD;
    
    // 4K and beyond
    if (matchesResolution(width, height, 3840, 2160)) return ScreenSize::UHD_4K;
    if (matchesResolution(width, height, 5120, 2880)) return ScreenSize::UHD_5K;
    if (matchesResolution(width, height, 7680, 2160)) return ScreenSize::DUHD;
    if (matchesResolution(width, height, 7680, 4320)) return ScreenSize::UHD_8K;
    
    // Fallback based on width ranges
    if (width < 1024)
        return ScreenSize::WXGA_720;      // Treat as standard HD
    else if (width < 1600)
        return ScreenSize::HD_768_1366;   // Most common laptop size
    else if (width < 2048)
        return ScreenSize::FHD;           // Standard Full HD
    else if (width < 3000)
        return ScreenSize::QHD;           // Quad HD
    else if (width < 6000)
        return ScreenSize::UHD_4K;        // 4K
    else
        return ScreenSize::UHD_8K;        // 8K and beyond
}

ScreenCategory ResponsiveLayout::getScreenCategory(ScreenSize size)
{
    switch (size)
    {
        case ScreenSize::Mobile:
            return ScreenCategory::Mobile;
            
        case ScreenSize::WXGA_720:
        case ScreenSize::WXGA_800:
        case ScreenSize::SXGA:
        case ScreenSize::HD_768_1360:
        case ScreenSize::HD_768_1366:
        case ScreenSize::WXGA_Plus:
        case ScreenSize::HD_864:
        case ScreenSize::HD_Plus:
        case ScreenSize::UXGA:
        case ScreenSize::WSXGA_Plus:
            return ScreenCategory::Standard;
            
        case ScreenSize::FHD:
        case ScreenSize::WUXGA:
        case ScreenSize::QWXGA:
        case ScreenSize::QXGA:
        case ScreenSize::QHD:
        case ScreenSize::WQXGA:
            return ScreenCategory::Wide;
            
        case ScreenSize::UWFHD:
        case ScreenSize::UWQHD:
        case ScreenSize::DUHD:
            return ScreenCategory::UltraWide;
            
        case ScreenSize::UHD_4K:
        case ScreenSize::UHD_5K:
        case ScreenSize::UHD_8K:
            return ScreenCategory::HighResolution;
            
        default:
            return ScreenCategory::Standard;
    }
}

juce::String ResponsiveLayout::getScreenSizeText(ScreenSize size)
{
    switch (size)
    {
        case ScreenSize::Mobile:         return "Mobile";
        case ScreenSize::WXGA_720:       return "WXGA (720p)";
        case ScreenSize::WXGA_800:       return "WXGA (800p)";
        case ScreenSize::SXGA:           return "SXGA";
        case ScreenSize::HD_768_1360:    return "HD (1360x768)";
        case ScreenSize::HD_768_1366:    return "HD (1366x768)";
        case ScreenSize::WXGA_Plus:      return "WXGA+";
        case ScreenSize::HD_864:         return "HD (864p)";
        case ScreenSize::HD_Plus:        return "HD+";
        case ScreenSize::UXGA:           return "UXGA";
        case ScreenSize::WSXGA_Plus:     return "WSXGA+";
        case ScreenSize::FHD:            return "Full HD";
        case ScreenSize::WUXGA:          return "WUXGA";
        case ScreenSize::QWXGA:          return "QWXGA";
        case ScreenSize::QXGA:           return "QXGA";
        case ScreenSize::UWFHD:          return "Ultra-Wide FHD";
        case ScreenSize::QHD:            return "Quad HD";
        case ScreenSize::WQXGA:          return "WQXGA";
        case ScreenSize::UWQHD:          return "Ultra-Wide QHD";
        case ScreenSize::UHD_4K:         return "4K Ultra HD";
        case ScreenSize::UHD_5K:         return "5K Ultra HD";
        case ScreenSize::DUHD:           return "Dual Ultra HD";
        case ScreenSize::UHD_8K:         return "8K Ultra HD";
        default:                         return "Unknown";
    }
}

juce::String ResponsiveLayout::getScreenResolutionText(ScreenSize size)
{
    switch (size)
    {
        case ScreenSize::Mobile:         return "< 768px";
        case ScreenSize::WXGA_720:       return "1280x720";
        case ScreenSize::WXGA_800:       return "1280x800";
        case ScreenSize::SXGA:           return "1280x1024";
        case ScreenSize::HD_768_1360:    return "1360x768";
        case ScreenSize::HD_768_1366:    return "1366x768";
        case ScreenSize::WXGA_Plus:      return "1440x900";
        case ScreenSize::HD_864:         return "1536x864";
        case ScreenSize::HD_Plus:        return "1600x900";
        case ScreenSize::UXGA:           return "1600x1200";
        case ScreenSize::WSXGA_Plus:     return "1680x1050";
        case ScreenSize::FHD:            return "1920x1080";
        case ScreenSize::WUXGA:          return "1920x1200";
        case ScreenSize::QWXGA:          return "2048x1152";
        case ScreenSize::QXGA:           return "2048x1536";
        case ScreenSize::UWFHD:          return "2560x1080";
        case ScreenSize::QHD:            return "2560x1440";
        case ScreenSize::WQXGA:          return "2560x1600";
        case ScreenSize::UWQHD:          return "3440x1440";
        case ScreenSize::UHD_4K:         return "3840x2160";
        case ScreenSize::UHD_5K:         return "5120x2880";
        case ScreenSize::DUHD:           return "7680x2160";
        case ScreenSize::UHD_8K:         return "7680x4320";
        default:                         return "Unknown";
    }
}

float ResponsiveLayout::getAspectRatio(ScreenSize size)
{
    switch (size)
    {
        case ScreenSize::WXGA_720:       return 16.0f / 9.0f;    // 1.78
        case ScreenSize::WXGA_800:       return 16.0f / 10.0f;   // 1.6
        case ScreenSize::SXGA:           return 5.0f / 4.0f;     // 1.25
        case ScreenSize::HD_768_1360:    return 16.0f / 9.0f;    // 1.78
        case ScreenSize::HD_768_1366:    return 16.0f / 9.0f;    // 1.78
        case ScreenSize::WXGA_Plus:      return 16.0f / 10.0f;   // 1.6
        case ScreenSize::HD_864:         return 16.0f / 9.0f;    // 1.78
        case ScreenSize::HD_Plus:        return 16.0f / 9.0f;    // 1.78
        case ScreenSize::UXGA:           return 4.0f / 3.0f;     // 1.33
        case ScreenSize::WSXGA_Plus:     return 16.0f / 10.0f;   // 1.6
        case ScreenSize::FHD:            return 16.0f / 9.0f;    // 1.78
        case ScreenSize::WUXGA:          return 16.0f / 10.0f;   // 1.6
        case ScreenSize::QWXGA:          return 16.0f / 9.0f;    // 1.78
        case ScreenSize::QXGA:           return 4.0f / 3.0f;     // 1.33
        case ScreenSize::UWFHD:          return 21.0f / 9.0f;    // 2.33
        case ScreenSize::QHD:            return 16.0f / 9.0f;    // 1.78
        case ScreenSize::WQXGA:          return 16.0f / 10.0f;   // 1.6
        case ScreenSize::UWQHD:          return 21.0f / 9.0f;    // 2.33
        case ScreenSize::UHD_4K:         return 16.0f / 9.0f;    // 1.78
        case ScreenSize::UHD_5K:         return 16.0f / 9.0f;    // 1.78
        case ScreenSize::DUHD:           return 32.0f / 9.0f;    // 3.56
        case ScreenSize::UHD_8K:         return 16.0f / 9.0f;    // 1.78
        default:                         return 16.0f / 9.0f;    // Default widescreen
    }
}

bool ResponsiveLayout::isUltraWideScreen(ScreenSize size)
{
    return size == ScreenSize::UWFHD || 
           size == ScreenSize::UWQHD || 
           size == ScreenSize::DUHD;
}

float ResponsiveLayout::getDefaultScale(ScreenSize size)
{
    ScreenCategory category = getScreenCategory(size);
    
    switch (category)
    {
        case ScreenCategory::Mobile:
            return 0.8f;  // Smaller for mobile devices
            
        case ScreenCategory::Standard:
            return 0.9f;  // Standard laptop/desktop scale
            
        case ScreenCategory::Wide:
            return 1.0f;  // Base scale for Full HD and similar
            
        case ScreenCategory::UltraWide:
            return 1.1f;  // Slightly larger for ultra-wide screens
            
        case ScreenCategory::HighResolution:
            // Scale based on specific resolution
            switch (size)
            {
                case ScreenSize::UHD_4K:  return 1.5f;  // 4K needs significant scaling
                case ScreenSize::UHD_5K:  return 1.8f;  // 5K needs even more scaling
                case ScreenSize::UHD_8K:  return 2.4f;  // 8K needs substantial scaling
                default:                   return 1.2f;  // Default high-res scaling
            }
            
        default:
            return 1.0f;
    }
}

//==============================================================================
void ResponsiveLayout::onScreenSizeChanged(ScreenSize oldSize, ScreenSize newSize)
{
    DBG("ResponsiveLayout: Screen size changed from " + juce::String((int)oldSize) + " to " + juce::String((int)newSize));
    
    // Notify about the change (could emit callbacks here if needed)
    // This is where you could trigger animations or special transitions
}