/*
  ==============================================================================

    Preset.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Preset model implementation

  ==============================================================================
*/

#include "Preset.h"

//==============================================================================
juce::String Preset::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id", juce::String(id));
    obj->setProperty("preset", juce::String(preset));
    obj->setProperty("font", juce::String(font));
    obj->setProperty("fontSize", (double)fontSize);
    obj->setProperty("bold", bold);
    obj->setProperty("italic", italic);
    obj->setProperty("glow", glow);
    obj->setProperty("lineSpacing", (double)lineSpacing);
    obj->setProperty("numLines", numLines);
    obj->setProperty("highlightLyricFill", juce::String(highlightLyricFill));
    obj->setProperty("highlightLyricOutline", juce::String(highlightLyricOutline));
    obj->setProperty("normLyricFill", juce::String(normLyricFill));
    obj->setProperty("normLyricOutline", juce::String(normLyricOutline));
    return juce::JSON::toString(juce::var(obj.get()));
}

Preset Preset::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

Preset Preset::fromJsonObject(juce::DynamicObject* obj)
{
    Preset p;
    if (obj == nullptr) return p;
    p.id                    = obj->getProperty("id").toString().toStdString();
    p.preset                = obj->getProperty("preset").toString().toStdString();
    p.font                  = obj->getProperty("font").toString().toStdString();
    p.fontSize              = (float)(double)obj->getProperty("fontSize");
    p.bold                  = (bool)obj->getProperty("bold");
    p.italic                = (bool)obj->getProperty("italic");
    p.glow                  = (bool)obj->getProperty("glow");
    p.lineSpacing           = (float)(double)obj->getProperty("lineSpacing");
    p.numLines              = (int)obj->getProperty("numLines");
    p.highlightLyricFill    = obj->getProperty("highlightLyricFill").toString().toStdString();
    p.highlightLyricOutline = obj->getProperty("highlightLyricOutline").toString().toStdString();
    p.normLyricFill         = obj->getProperty("normLyricFill").toString().toStdString();
    p.normLyricOutline      = obj->getProperty("normLyricOutline").toString().toStdString();
    return p;
}

bool Preset::isValid() const
{
    return !id.empty() && !preset.empty();
}
