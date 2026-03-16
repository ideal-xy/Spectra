#include "library.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace fs = std::filesystem;

namespace tplay {

namespace {

/// Escapes `path` so it can be safely embedded in a shell command.
/// `path` may contain spaces or quote characters.
std::string shellQuote(const std::string& path) {
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

}

/// Returns true when `extension` is a supported audio suffix.
bool Library::isAudioFile(const std::string& extension) {
    static const std::array<std::string, 4> exts = {".mp3", ".m4a", ".wav", ".flac"};
    return std::find(exts.begin(), exts.end(), extension) != exts.end();
}

/// Returns the leaf filename for `path`.
std::string Library::basename(const std::string& path) {
    return fs::path(path).filename().string();
}

/// Probes `path` with `afinfo` and returns the estimated duration in seconds.
double Library::probeDuration(const std::string& path) {
    const std::string command =
        "afinfo " + shellQuote(path) + " 2>/dev/null | awk '/estimated duration/ {print $3}'";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        return 0.0;
    }

    char buffer[128] = {0};
    if (!std::fgets(buffer, sizeof(buffer), pipe.get())) {
        return 0.0;
    }

    try {
        return std::stod(buffer);
    } catch (...) {
        return 0.0;
    }
}

/// Walks `root`, extracts supported audio files, and sorts them by title.
std::vector<Track> Library::scan(const std::string& root) const {
    std::vector<Track> tracks;

    std::error_code ec;
    if (!fs::exists(root, ec)) {
        throw std::runtime_error("music path does not exist: " + root);
    }

    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end;
         it.increment(ec)) {
        if (ec || !it->is_regular_file()) {
            continue;
        }

        std::string ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (!isAudioFile(ext)) {
            continue;
        }

        Track track;
        track.path = it->path().string();
        track.title = basename(track.path);
        track.duration = probeDuration(track.path);
        tracks.push_back(std::move(track));
    }

    std::sort(tracks.begin(), tracks.end(), [](const Track& lhs, const Track& rhs) {
        return lhs.title < rhs.title;
    });

    return tracks;
}

}
