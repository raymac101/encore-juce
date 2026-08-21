#import <AVFoundation/AVFoundation.h>
#import <ImageIO/ImageIO.h>

#include "AdVideoThumbnail.h"

juce::Image AdVideoThumbnail::create (const juce::File& videoFile, int maximumSize)
{
    @autoreleasepool
    {
        const auto path = videoFile.getFullPathName();
        auto* pathString = [NSString stringWithUTF8String:path.toRawUTF8()];
        if (pathString == nil)
            return {};

        auto* url = [NSURL fileURLWithPath:pathString];
        auto* asset = [AVURLAsset URLAssetWithURL:url options:nil];
        auto* generator = [[AVAssetImageGenerator alloc] initWithAsset:asset];
        generator.appliesPreferredTrackTransform = YES;
        generator.maximumSize = CGSizeMake ((CGFloat) maximumSize, (CGFloat) maximumSize);

        NSError* error = nil;
        const auto requestedTime = CMTimeMakeWithSeconds (1.0, 600);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        CGImageRef frame = [generator copyCGImageAtTime:requestedTime actualTime:nil error:&error];
#pragma clang diagnostic pop
        [generator release];

        if (frame == nullptr)
            return {};

        auto* encoded = CFDataCreateMutable (kCFAllocatorDefault, 0);
        auto* destination = CGImageDestinationCreateWithData (encoded, CFSTR ("public.png"), 1, nullptr);
        if (destination != nullptr)
        {
            CGImageDestinationAddImage (destination, frame, nullptr);
            CGImageDestinationFinalize (destination);
            CFRelease (destination);
        }
        CGImageRelease (frame);

        juce::Image image;
        if (CFDataGetLength (encoded) > 0)
        {
            juce::MemoryInputStream input (CFDataGetBytePtr (encoded),
                                           (size_t) CFDataGetLength (encoded), false);
            image = juce::ImageFileFormat::loadFrom (input);
        }
        CFRelease (encoded);
        return image;
    }
}