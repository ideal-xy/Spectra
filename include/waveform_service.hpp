#pragma once

#include "track.hpp"
#include "waveform_cache.hpp"

#include <string>
#include <vector>

namespace tplay {

class WaveformService {
public:
    bool prepareTrack(const Track& track);
    void clear();

    std::vector<float> barsAt(double playbackTimeSeconds, int preferredBars) const;
    const std::string& status() const noexcept { return status_; }

private:
    static std::string cachePathFor(const std::string& trackPath);
    static std::string shellQuote(const std::string& path);
    static std::string ffmpegPath();
    static bool decodeTrack(const std::string& trackPath,
                            std::vector<float>& samples,
                            std::string& error);
    static std::vector<float> resizeBars(const std::vector<float>& bars, int preferredBars);

    WaveformCache cache_;
    std::string currentTrackPath_;
    std::string status_ = "idle";
};

}
