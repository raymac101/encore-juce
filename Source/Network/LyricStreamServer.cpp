/*
  ==============================================================================

    LyricStreamServer.cpp

    See LyricStreamServer.h.

  ==============================================================================
*/

#include "LyricStreamServer.h"
#include <cstring>
#include <iterator>

namespace
{
    constexpr int kFirstPort = 8791;
    constexpr int kPortAttempts = 10;

    // Polls /frame roughly 10x/sec and draws whatever comes back into a
    // full-viewport <img>. Deliberately avoids <canvas> so there's nothing
    // platform-specific for iOS Safari vs Android Chrome to disagree about.
    const char* kIndexHtml = R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>Encore Karaoke Lyrics</title>
<style>
  html, body { margin: 0; padding: 0; background: #000; height: 100%; overflow: hidden; }
  #frame { display: block; width: 100vw; height: 100vh; object-fit: contain; background: #000; }
</style>
</head>
<body>
<img id="frame" alt="Lyrics">
<script>
(function () {
  var img = document.getElementById('frame');
  var lastUrl = null;
  function poll() {
    fetch('/frame', { cache: 'no-store' })
      .then(function (r) { return r.blob(); })
      .then(function (blob) {
        var url = URL.createObjectURL(blob);
        var previous = lastUrl;
        img.onload = function () { if (previous) URL.revokeObjectURL(previous); };
        img.src = url;
        lastUrl = url;
      })
      .catch(function () {})
      .finally(function () { setTimeout(poll, 100); });
  }
  poll();
})();
</script>
</body>
</html>
)HTML";
}

LyricStreamServer::LyricStreamServer()
    : juce::Thread ("LyricStreamServer")
{
}

LyricStreamServer::~LyricStreamServer()
{
    stop();
}

bool LyricStreamServer::start()
{
    if (listening_.load())
        return true;

    for (int attempt = 0; attempt < kPortAttempts; ++attempt)
    {
        auto candidate = std::make_unique<juce::StreamingSocket>();
        if (candidate->createListener (kFirstPort + attempt))
        {
            listener_ = std::move (candidate);
            boundPort_ = kFirstPort + attempt;
            break;
        }
    }

    if (listener_ == nullptr)
        return false;

    listening_.store (true);
    pushPlaceholder ("Waiting for the next song...");
    startThread();
    return true;
}

void LyricStreamServer::stop()
{
    if (! listening_.load())
        return;

    listening_.store (false);

    if (listener_ != nullptr)
        listener_->close();

    stopThread (2000);
    listener_.reset();
    boundPort_ = 0;
}

int LyricStreamServer::getConnectedClientCount() const
{
    constexpr juce::int64 kFreshWindowMs = 3000;
    const auto now = juce::Time::currentTimeMillis();

    const juce::ScopedLock sl (recentClientsLock_);
    int count = 0;
    for (const auto& entry : recentClientLastSeenMs_)
        if (now - entry.second < kFreshWindowMs)
            ++count;
    return count;
}

void LyricStreamServer::noteClientSeen (const juce::String& clientKey)
{
    constexpr juce::int64 kPruneAfterMs = 10000;
    const auto now = juce::Time::currentTimeMillis();

    const juce::ScopedLock sl (recentClientsLock_);
    recentClientLastSeenMs_[clientKey] = now;

    for (auto it = recentClientLastSeenMs_.begin(); it != recentClientLastSeenMs_.end();)
        it = (now - it->second > kPruneAfterMs) ? recentClientLastSeenMs_.erase (it) : std::next (it);
}

juce::String LyricStreamServer::getUrl() const
{
    if (! listening_.load() || boundPort_ == 0)
        return {};

    const auto loopback = juce::IPAddress::local (false);
    for (const auto& addr : juce::IPAddress::getAllAddresses (false))
        if (! addr.isNull() && addr != loopback)
            return "http://" + addr.toString() + ":" + juce::String (boundPort_) + "/";

    return {};
}

void LyricStreamServer::pushFrame (const juce::Image& image)
{
    if (! image.isValid())
        return;

    juce::MemoryOutputStream mo;
    juce::PNGImageFormat png;
    if (! png.writeImageToStream (image, mo))
        return;

    const juce::ScopedLock sl (frameLock_);
    latestFramePng_ = mo.getMemoryBlock();
}

void LyricStreamServer::pushPlaceholder (const juce::String& message)
{
    juce::Image image (juce::Image::RGB, 960, 540, true);
    juce::Graphics g (image);
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions().withHeight (28.0f)).boldened());
    g.drawFittedText (message, image.getBounds().reduced (60), juce::Justification::centred, 4);

    pushFrame (image);
}

void LyricStreamServer::run()
{
    while (! threadShouldExit())
    {
        if (listener_ == nullptr)
            break;

        std::unique_ptr<juce::StreamingSocket> client (listener_->waitForNextConnection());
        if (threadShouldExit() || client == nullptr)
            break;

        auto* self = this;
        auto* rawClient = client.release();
        juce::Thread::launch ([self, rawClient]
        {
            self->handleConnection (rawClient);
        });
    }
}

juce::String LyricStreamServer::readRequestLine (juce::StreamingSocket& socket)
{
    juce::String line;
    for (int i = 0; i < 4096; ++i)
    {
        if (! socket.waitUntilReady (true, 2000))
            break;

        char c = 0;
        const auto n = socket.read (&c, 1, true);
        if (n <= 0)
            break;
        if (c == '\n')
            break;
        if (c != '\r')
            line += juce::String::charToString (c);
    }
    return line;
}

void LyricStreamServer::writeResponse (juce::StreamingSocket& socket, int statusCode, const juce::String& statusText,
                                        const juce::String& contentType, const void* body, size_t bodyBytes)
{
    juce::String header;
    header << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
           << "Content-Type: " << contentType << "\r\n"
           << "Content-Length: " << (int) bodyBytes << "\r\n"
           << "Cache-Control: no-store\r\n"
           << "Connection: close\r\n"
           << "\r\n";

    const auto headerBytes = header.toRawUTF8();
    socket.write (headerBytes, (int) strlen (headerBytes));

    if (bodyBytes > 0 && body != nullptr)
        socket.write (body, (int) bodyBytes);
}

void LyricStreamServer::handleConnection (juce::StreamingSocket* rawSocket)
{
    std::unique_ptr<juce::StreamingSocket> socket (rawSocket);

    const auto requestLine = readRequestLine (*socket);
    juce::StringArray tokens;
    tokens.addTokens (requestLine, " ", "");
    const juce::String path = tokens.size() >= 2 ? tokens[1] : juce::String();

    if (path == "/")
    {
        writeResponse (*socket, 200, "OK", "text/html; charset=utf-8", kIndexHtml, strlen (kIndexHtml));
    }
    else if (path == "/frame")
    {
        const auto clientKey = socket->getHostName();
        noteClientSeen (clientKey.isNotEmpty() ? clientKey : "unknown");

        juce::MemoryBlock png;
        {
            const juce::ScopedLock sl (frameLock_);
            png = latestFramePng_;
        }
        writeResponse (*socket, 200, "OK", "image/png", png.getData(), png.getSize());
    }
    else
    {
        static const char* notFound = "Not found";
        writeResponse (*socket, 404, "Not Found", "text/plain", notFound, strlen (notFound));
    }

    socket->close();
}
