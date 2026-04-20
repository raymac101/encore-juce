/*
  ==============================================================================

    CdgGroup.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    CDG Group model implementation

  ==============================================================================
*/

#include "CdgGroup.h"

//==============================================================================
juce::String CdgGroup::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    obj->setProperty("id", juce::String(id));
    obj->setProperty("name", juce::String(name));
    obj->setProperty("type", juce::String(type));

    juce::Array<juce::var> dataArr;
    for (auto& d : data) dataArr.add(juce::String(d));
    obj->setProperty("data", juce::var(dataArr));

    return juce::JSON::toString(juce::var(obj.get()));
}

//==============================================================================
CdgGroup CdgGroup::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

CdgGroup CdgGroup::fromJsonObject(juce::DynamicObject* obj)
{
    CdgGroup g;
    if (obj == nullptr) return g;

    g.id   = obj->getProperty("id").toString().toStdString();
    g.name = obj->getProperty("name").toString().toStdString();
    g.type = obj->getProperty("type").toString().toStdString();

    auto dataVar = obj->getProperty("data");
    if (dataVar.isArray())
        for (int i = 0; i < dataVar.size(); ++i)
            g.data.push_back(dataVar[i].toString().toStdString());

    return g;
}

//==============================================================================
bool CdgGroup::isValid() const
{
    return !id.empty() && !name.empty();
}
