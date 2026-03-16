#pragma once

#include "track.hpp"

#include <string>
#include <vector>

namespace tplay {

class Library {
public:
    /// Recursively scans `root` for supported audio files and returns sorted track metadata.
    /// `root` is the directory to search for playable media.
    std::vector<Track> scan(const std::string& root) const;

private:
    /// Returns true when `extension` matches a supported audio suffix.
    /// `extension` is expected to be lowercase and include the leading dot.
    static bool isAudioFile(const std::string& extension);
    /// Extracts the filename portion from `path`.
    /// `path` is an absolute or relative filesystem path.
    static std::string basename(const std::string& path);
    /// Probes the media duration for `path` in seconds.
    /// `path` is the audio file to inspect.
    static double probeDuration(const std::string& path);
};

}
