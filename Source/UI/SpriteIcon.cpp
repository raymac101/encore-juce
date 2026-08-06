/*
  ==============================================================================

    SpriteIcon.cpp

  ==============================================================================
*/

#include "SpriteIcon.h"

namespace SpriteIcon
{

juce::File resolveAssetFile (const juce::String& relativePath)
{
    auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    auto cwd = juce::File::getCurrentWorkingDirectory();

    const juce::Array<juce::File> roots {
        cwd,
        exeDir,
        exeDir.getParentDirectory(),
        exeDir.getParentDirectory().getParentDirectory(),
        exeDir.getParentDirectory().getParentDirectory().getParentDirectory()
    };

    for (const auto& root : roots)
    {
        auto candidate = root.getChildFile (relativePath);
        if (candidate.existsAsFile())
            return candidate;
    }

    return {};
}

std::unique_ptr<juce::Drawable> create (const juce::String& symbolId, const juce::Colour& colour)
{
    auto createInlineIcon = [&]() -> std::unique_ptr<juce::Drawable>
    {
        juce::String pathData;

        if (symbolId == "icon-play3" || symbolId == "icon-play")
            pathData = "M6 4l20 12-20 12z";
        else if (symbolId == "icon-pause2")
            pathData = "M4 4h10v24h-10zM18 4h10v24h-10z";
        else if (symbolId == "icon-previous2")
            pathData = "M8 28v-24h4v11l10-10v22l-10-10v11z";
        else if (symbolId == "icon-next2")
            pathData = "M24 4v24h-4v-11l-10 10v-22l10 10v-11z";
        else if (symbolId == "icon-stop2")
            pathData = "M6 6h20v20h-20z";

        if (pathData.isEmpty())
            return {};

        const juce::String inlineSvg =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\" viewBox=\"0 0 32 32\">"
            "<path fill=\"" + colour.toDisplayString(true) + "\" d=\"" + pathData + "\"/>"
            "</svg>";

        if (auto xml = juce::XmlDocument::parse(inlineSvg))
        {
            auto tempFile = juce::File::createTempFile(".svg");
            if (xml->writeTo(tempFile))
            {
                auto drawable = juce::Drawable::createFromSVGFile(tempFile);
                tempFile.deleteFile();
                return drawable;
            }
        }

        return {};
    };

    const auto spriteFile = resolveAssetFile ("assets/images/sprite.svg");
    if (!spriteFile.existsAsFile())
        return createInlineIcon();

    auto root = juce::XmlDocument::parse(spriteFile);
    if (!root)
        return createInlineIcon();

    juce::XmlElement* symbol = nullptr;

    if (auto* defs = root->getChildByName("defs"))
    {
        for (auto* child = defs->getFirstChildElement(); child != nullptr; child = child->getNextElement())
        {
            if (child->hasTagName("symbol") && child->getStringAttribute("id") == symbolId)
            {
                symbol = child;
                break;
            }
        }
    }

    if (symbol == nullptr)
    {
        for (auto* child = root->getFirstChildElement(); child != nullptr; child = child->getNextElement())
        {
            if (child->hasTagName("symbol") && child->getStringAttribute("id") == symbolId)
            {
                symbol = child;
                break;
            }
        }
    }

    if (symbol == nullptr)
        return createInlineIcon();

    juce::XmlElement iconSvg("svg");
    iconSvg.setAttribute("xmlns", "http://www.w3.org/2000/svg");
    iconSvg.setAttribute("xmlns:xlink", "http://www.w3.org/1999/xlink");
    iconSvg.setAttribute("width", "32");
    iconSvg.setAttribute("height", "32");
    iconSvg.setAttribute("viewBox", symbol->getStringAttribute("viewBox"));

    for (auto* child = symbol->getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        auto* newChild = new juce::XmlElement(*child);
        iconSvg.addChildElement(newChild);
    }

    std::function<void(juce::XmlElement&)> tintElements = [&tintElements, &colour] (juce::XmlElement& elem)
    {
        const auto tag = elem.getTagName().toLowerCase();
        const bool isShape = tag == "path" || tag == "circle" || tag == "ellipse"
                          || tag == "rect" || tag == "polygon" || tag == "polyline"
                          || tag == "line";

        if (isShape)
        {
            auto tintAttribute = [&elem, &colour] (const char* attrName)
            {
                if (!elem.hasAttribute(attrName))
                    return;

                const auto value = elem.getStringAttribute(attrName).trim();
                if (value.equalsIgnoreCase("none"))
                    return;

                elem.setAttribute(attrName, colour.toDisplayString(true));
            };

            tintAttribute("fill");
            tintAttribute("stroke");

            if (!elem.hasAttribute("fill") && !elem.hasAttribute("stroke"))
                elem.setAttribute("fill", colour.toDisplayString(true));
        }

        for (auto* child = elem.getFirstChildElement(); child != nullptr; child = child->getNextElement())
            tintElements(*child);
    };

    tintElements(iconSvg);

    auto tempFile = juce::File::createTempFile(".svg");
    if (! iconSvg.writeTo(tempFile))
        return {};
    auto drawable = juce::Drawable::createFromSVGFile(tempFile);
    tempFile.deleteFile();
    if (drawable)
        return drawable;

    return createInlineIcon();
}

} // namespace SpriteIcon
