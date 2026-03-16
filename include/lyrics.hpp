#pragma once

#include "track.hpp"

#include <optional>
#include <string>
#include <vector>

namespace tplay {

struct LyricLine {
    double timeSeconds = 0.0;
    std::string text;
};

struct LyricsData {
    std::string trackPath;
    std::string sourcePath;
    bool synced = true;
    std::vector<LyricLine> lines;

    bool empty() const noexcept { return lines.empty(); }
    std::size_t activeIndex(double playbackTimeSeconds) const noexcept;
};

class LyricsService {
public:
    bool prepareTrack(const Track& track);
    void clear();

    const LyricsData* current() const noexcept { return current_; }
    const std::string& status() const noexcept { return status_; }

private:
    static std::optional<LyricsData> loadLrc(const Track& track);
    static std::optional<std::string> localLyricsPathFor(const Track& track);

    LyricsData currentData_;
    const LyricsData* current_ = nullptr;
    std::string status_ = "no local or embedded lyrics";
};

}
