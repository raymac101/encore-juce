/*
  ==============================================================================

    PluginHostService.cpp

  ==============================================================================
*/

#include "PluginHostService.h"

//==============================================================================
PluginHostService::PluginHostService()
{
   #if JUCE_PLUGINHOST_VST3
    formatManager_.addFormat(new juce::VST3PluginFormat());
   #endif

    loadCache();
}

PluginHostService& PluginHostService::getInstance()
{
    static PluginHostService instance;
    return instance;
}

juce::File PluginHostService::getCacheFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("EncoreKaraoke");
    if (! dir.exists())
        dir.createDirectory();

    return dir.getChildFile("plugin-cache.xml");
}

juce::File PluginHostService::getScanTempDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("EncoreKaraoke")
                   .getChildFile("plugin-scan-tmp");
    if (! dir.exists())
        dir.createDirectory();

    return dir;
}

void PluginHostService::loadCache()
{
    auto file = getCacheFile();
    if (! file.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse(file))
    {
        knownPlugins_.recreateFromXml(*xml);
        lastScanTime_ = file.getLastModificationTime();
    }
}

void PluginHostService::saveCache() const
{
    if (auto xml = knownPlugins_.createXml())
        xml->writeTo(getCacheFile());
}

juce::Array<juce::PluginDescription> PluginHostService::getAvailablePlugins() const
{
    return knownPlugins_.getTypes();
}

//==============================================================================
bool PluginHostService::handleScanCommandLineIfPresent()
{
    const auto args = juce::JUCEApplicationBase::getCommandLineParameterArray();

    juce::String pluginPath, outputPath;
    for (auto& arg : args)
    {
        if (arg.startsWith("--scan-plugin="))
            pluginPath = arg.fromFirstOccurrenceOf("--scan-plugin=", false, false);
        else if (arg.startsWith("--scan-output="))
            outputPath = arg.fromFirstOccurrenceOf("--scan-output=", false, false);
    }

    if (pluginPath.isEmpty() || outputPath.isEmpty())
        return false;

    juce::XmlElement root("PLUGIN_SCAN_RESULT");

   #if JUCE_PLUGINHOST_VST3
    // findAllTypesForFile loads the plugin's binary module to query its
    // class factory — this is the operation that can crash for a malformed
    // or incompatible plugin. That's exactly why this function only ever
    // runs inside this disposable, isolated child-process invocation, never
    // inside the main running app (see scanForPlugins() below).
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> results;
    format.findAllTypesForFile(results, pluginPath);

    for (auto* desc : results)
        root.addChildElement(desc->createXml().release());
   #endif

    juce::File(outputPath).replaceWithText(root.toString());
    return true;
}

void PluginHostService::scanForPlugins(ProgressCallback onProgress, CompletionCallback onComplete)
{
    if (scanning_.exchange(true))
        return;

    juce::Thread::launch([this, onProgress, onComplete]()
    {
       #if JUCE_PLUGINHOST_VST3
        juce::VST3PluginFormat format;
        const auto candidates = format.searchPathsForPlugins(format.getDefaultLocationsToSearch(),
                                                              true /*recursive*/, false);
        const auto scanExecutable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        const auto tempDir = getScanTempDirectory();

        int numFound = 0;

        for (int i = 0; i < candidates.size(); ++i)
        {
            const auto& candidatePath = candidates[i];
            const auto fallbackName = juce::File(candidatePath).getFileNameWithoutExtension();
            const float progress = candidates.size() > 0 ? (float) (i + 1) / (float) candidates.size() : 1.0f;

            const auto outputFile = tempDir.getChildFile("scan_" + juce::String(i) + ".xml");
            outputFile.deleteFile();

            juce::ChildProcess child;
            const bool started = child.start(juce::StringArray {
                scanExecutable.getFullPathName(),
                "--scan-plugin=" + candidatePath,
                "--scan-output=" + outputFile.getFullPathName()
            });

            if (started)
            {
                // Generous timeout — a slow plugin shouldn't be mistaken for
                // a hang, but a genuinely stuck/crashed scan must not block
                // the whole scan indefinitely.
                const bool finished = child.waitForProcessToFinish(10000);
                if (! finished)
                    child.kill();
            }

            // Report AFTER the attempt, not before, so `found` reflects a
            // real result — this is what lets a UI show plugins appearing
            // live as they're actually discovered, not just attempted.
            bool foundAny = false;
            juce::StringArray foundNames;

            if (outputFile.existsAsFile())
            {
                if (auto xml = juce::XmlDocument::parse(outputFile))
                {
                    for (auto* descXml : xml->getChildIterator())
                    {
                        juce::PluginDescription desc;
                        if (desc.loadFromXml(*descXml))
                        {
                            knownPlugins_.addType(desc);
                            ++numFound;
                            foundAny = true;
                            foundNames.add(desc.name);
                        }
                    }
                }
                outputFile.deleteFile();
            }

            if (onProgress)
            {
                const auto reportName = foundAny ? foundNames.joinIntoString(", ") : fallbackName;
                juce::MessageManager::callAsync([onProgress, progress, reportName, foundAny]()
                    { onProgress(progress, reportName, foundAny); });
            }
        }

        saveCache();
        lastScanTime_ = juce::Time::getCurrentTime();
       #else
        const int numFound = 0;
       #endif

        scanning_ = false;

        if (onComplete)
            juce::MessageManager::callAsync([onComplete, numFound]() { onComplete(numFound); });
    });
}
