/*
  ==============================================================================

    AdMetadata.cpp

  ==============================================================================
*/

#include "AdMetadata.h"

bool AdMetadata::isVideo() const noexcept
{
    const auto type = mediaType.trim().toLowerCase();
    if (type == "video") return true;
    if (type == "image") return false;

    const auto mime = mimeType.trim().toLowerCase();
    if (mime.startsWith ("video/"))
        return true;
    if (mime.contains ("mp4") || mime.contains ("m4v") || mime.contains ("mov")
        || mime.contains ("webm") || mime.contains ("quicktime"))
        return true;

    const auto lowerName = name.toLowerCase();
    const auto lowerUrl  = url.toLowerCase();
    return lowerName.endsWith (".mp4") || lowerName.endsWith (".m4v")
        || lowerName.endsWith (".mov") || lowerName.endsWith (".webm")
        || lowerUrl.contains (".mp4") || lowerUrl.contains (".m4v")
        || lowerUrl.contains (".mov") || lowerUrl.contains (".webm");
}

bool AdMetadata::isActiveAt (juce::int64 nowMs) const noexcept
{
    if (startDateMs > 0 && nowMs < startDateMs)
        return false;
    if (endDateMs > 0 && nowMs > endDateMs)
        return false;
    return true;
}
