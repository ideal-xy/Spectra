#include "waveform_service.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <sstream>
#include <unistd.h>

namespace fs = std::filesystem;

namespace tplay {

namespace {

constexpr int kSampleRate = 44100;
constexpr int kChannels = 2;
constexpr double kFrameStepSeconds = 0.02;
constexpr int kStoredBarCount = 56;
constexpr const char* kCacheVersion = "v7";

std::string stableHash(const std::string& value) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : value) {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }

    std::ostringstream out;
    out << std::hex << hash;
    return out.str();
}

std::string cacheRoot() 
{
    return "/tmp/tplay-wavecache";
}

std::string lastWriteToken(const std::string& trackPath) {
    std::error_code ec;
    const auto stamp = fs::last_write_time(trackPath, ec);
    if (ec) {
        return "0";
    }
    return std::to_string(static_cast<long long>(stamp.time_since_epoch().count()));
}

std::vector<float> smoothDisplayBars(std::vector<float> bars) {
    if (bars.size() < 3) {
        return bars;
    }

    std::vector<float> smoothed = bars;
    for (std::size_t i = 1; i + 1 < bars.size(); ++i) {
        smoothed[i] = 0.2f * bars[i - 1] + 0.6f * bars[i] + 0.2f * bars[i + 1];
    }

    const std::size_t edgeCount = std::min<std::size_t>(6, smoothed.size());
    for (std::size_t i = 0; i < edgeCount; ++i) {
        const float ratio = static_cast<float>(i + 1) / static_cast<float>(edgeCount + 1);
        const float fade = 0.35f + 0.65f * std::sqrt(ratio);
        smoothed[i] *= fade;
    }

    return smoothed;
}

}

bool WaveformService::prepareTrack(const Track& track) 
{
    currentTrackPath_ = track.path;

    if (const auto* cached = cache_.find(track.path)) {
        status_ = "waveform ready";
        return !cached->empty();
    }

    const std::string path = cachePathFor(track.path);
    if (const auto loaded = WaveformSerializer::load(path)) {
        cache_.put(*loaded);
        status_ = "waveform cache";
        return true;
    }

    std::vector<float> samples;
    std::string error;
    status_ = "building waveform";
    if (!decodeTrack(track.path, samples, error)) 
    {
        status_ = error.empty() ? "waveform unavailable" : error;
        return false;
    }

    WaveformData data = WaveformBuilder::buildFromPcm(track.path,
                                                      samples,
                                                      kChannels,
                                                      kSampleRate,
                                                      kFrameStepSeconds,
                                                      kStoredBarCount);
    if (data.empty()) {
        status_ = "waveform empty";
        return false;
    }

    std::error_code ec;
    fs::create_directories(cacheRoot(), ec);
    if (!WaveformSerializer::save(data, path)) 
    {
        status_ = "waveform ready (uncached)";
        cache_.put(std::move(data));
        return true;
    }

    cache_.put(std::move(data));
    status_ = "waveform built";
    return true;
}

void WaveformService::clear() {
    currentTrackPath_.clear();
    status_ = "idle";
}

std::vector<float> WaveformService::barsAt(double playbackTimeSeconds, int preferredBars) const 
{
    if (currentTrackPath_.empty()) 
    {
        return std::vector<float>(static_cast<std::size_t>(std::max(0, preferredBars)), 0.0f);
    }

    const auto* data = cache_.find(currentTrackPath_);
    if (!data) 
    {
        return std::vector<float>(static_cast<std::size_t>(std::max(0, preferredBars)), 0.0f);
    }

    return resizeBars(data->barsAt(playbackTimeSeconds), preferredBars);
}

std::string WaveformService::cachePathFor(const std::string& trackPath) 
{
    return cacheRoot() + "/" + stableHash(std::string(kCacheVersion) + ":" + trackPath + ":" + lastWriteToken(trackPath)) + ".wavecache";
}

std::string WaveformService::shellQuote(const std::string& path) 
{
    std::string escaped = "'";
    for (char ch : path) {
        if (ch == '\'') {
            escaped += "'\\''";
        } else {
            escaped += ch;
        }
    }
    escaped += "'";
    return escaped;
}

std::string WaveformService::ffmpegPath() 
{
    if (access("/opt/homebrew/bin/ffmpeg", X_OK) == 0) 
    {
        return "/opt/homebrew/bin/ffmpeg";
    }
    return "ffmpeg";
}

bool WaveformService::decodeTrack(const std::string& trackPath,
                                  std::vector<float>& samples,
                                  std::string& error) 
{
    const std::string command = ffmpegPath()
        + " -v error -nostdin -i " + shellQuote(trackPath)
        + " -f f32le -acodec pcm_f32le -ac 2 -ar 44100 -";

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) 
    {
        error = "failed to launch ffmpeg";
        return false;
    }

    std::array<float, 4096> chunk {};
    while (true) {
        const std::size_t n = std::fread(chunk.data(), sizeof(float), chunk.size(), pipe.get());
        if (n > 0) {
            samples.insert(samples.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(n));
        }
        if (n < chunk.size()) {
            if (std::ferror(pipe.get())) {
                error = "failed to read ffmpeg output";
                return false;
            }
            break;
        }
    }

    const int rc = pclose(pipe.release());
    if (rc != 0 && samples.empty()) 
    {
        error = "ffmpeg decode failed";
        return false;
    }
    if (samples.empty()) {
        error = "waveform unavailable";
        return false;
    }
    return true;
}

std::vector<float> WaveformService::resizeBars(const std::vector<float>& bars, int preferredBars) 
{
    if (preferredBars <= 0) {
        return {};
    }
    if (bars.empty()) {
        return std::vector<float>(static_cast<std::size_t>(preferredBars), 0.0f);
    }
    if (static_cast<int>(bars.size()) == preferredBars) {
        return smoothDisplayBars(bars);
    }

    std::vector<float> resized(static_cast<std::size_t>(preferredBars), 0.0f);
    for (int i = 0; i < preferredBars; ++i) {
        const double start = static_cast<double>(i) * static_cast<double>(bars.size()) / static_cast<double>(preferredBars);
        const double end = static_cast<double>(i + 1) * static_cast<double>(bars.size()) / static_cast<double>(preferredBars);
        const int left = static_cast<int>(std::floor(start));
        const int right = static_cast<int>(std::ceil(end));
        float accum = 0.0f;
        float weight = 0.0f;
        for (int source = left; source < right; ++source) {
            if (source < 0 || source >= static_cast<int>(bars.size())) {
                continue;
            }
            const double segStart = std::max(start, static_cast<double>(source));
            const double segEnd = std::min(end, static_cast<double>(source + 1));
            const float w = static_cast<float>(std::max(0.0, segEnd - segStart));
            accum += bars[static_cast<std::size_t>(source)] * w;
            weight += w;
        }
        resized[static_cast<std::size_t>(i)] = weight > 0.0f ? accum / weight : 0.0f;
    }
    return smoothDisplayBars(std::move(resized));
}

}
