#include "lyrics.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <unistd.h>

namespace fs = std::filesystem;

namespace tplay {

namespace {

std::string trim(const std::string& value) {
    std::size_t left = 0;
    while (left < value.size() && std::isspace(static_cast<unsigned char>(value[left])) != 0) {
        ++left;
    }

    std::size_t right = value.size();
    while (right > left && std::isspace(static_cast<unsigned char>(value[right - 1])) != 0) {
        --right;
    }

    return value.substr(left, right - left);
}

std::optional<double> parseTimestamp(const std::string& token) {
    const std::size_t colon = token.find(':');
    if (colon == std::string::npos) {
        return std::nullopt;
    }

    try {
        const int minutes = std::stoi(token.substr(0, colon));
        const double seconds = std::stod(token.substr(colon + 1));
        return static_cast<double>(minutes) * 60.0 + seconds;
    } catch (...) {
        return std::nullopt;
    }
}

std::string ffprobePath() {
    if (access("/opt/homebrew/bin/ffprobe", X_OK) == 0) {
        return "/opt/homebrew/bin/ffprobe";
    }
    return "ffprobe";
}

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

std::string jsonUnescape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) {
            out.push_back(value[i]);
            continue;
        }

        ++i;
        switch (value[i]) {
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case '\\':
            case '"':
            case '/':
                out.push_back(value[i]);
                break;
            default:
                out.push_back(value[i]);
                break;
        }
    }
    return out;
}

std::optional<std::string> extractJsonTagValue(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    std::size_t pos = json.find(needle);
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    pos = json.find('"', pos);
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    ++pos;
    std::string raw;
    bool escaped = false;
    for (; pos < json.size(); ++pos) {
        const char ch = json[pos];
        if (escaped) {
            raw.push_back('\\');
            raw.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            break;
        }
        raw.push_back(ch);
    }

    if (pos >= json.size()) {
        return std::nullopt;
    }
    return jsonUnescape(raw);
}

bool parseTimedLyrics(const std::string& contents, LyricsData& data) {
    std::istringstream in(contents);
    std::string rawLine;
    while (std::getline(in, rawLine)) {
        std::vector<double> stamps;
        std::size_t cursor = 0;
        while (cursor < rawLine.size() && rawLine[cursor] == '[') {
            const std::size_t close = rawLine.find(']', cursor);
            if (close == std::string::npos) {
                break;
            }

            const auto parsed = parseTimestamp(rawLine.substr(cursor + 1, close - cursor - 1));
            if (!parsed) {
                break;
            }
            stamps.push_back(*parsed);
            cursor = close + 1;
        }

        if (stamps.empty()) {
            continue;
        }

        const std::string text = trim(rawLine.substr(cursor));
        for (double stamp : stamps) {
            data.lines.push_back(LyricLine {stamp, text});
        }
    }

    std::sort(data.lines.begin(), data.lines.end(), [](const LyricLine& lhs, const LyricLine& rhs) {
        return lhs.timeSeconds < rhs.timeSeconds;
    });
    return !data.lines.empty();
}

void parsePlainLyrics(const std::string& contents, LyricsData& data) {
    std::istringstream in(contents);
    std::string rawLine;
    while (std::getline(in, rawLine)) {
        const std::string text = trim(rawLine);
        if (!text.empty()) {
            data.lines.push_back(LyricLine {0.0, text});
        }
    }
}

}

std::size_t LyricsData::activeIndex(double playbackTimeSeconds) const noexcept {
    if (lines.empty()) {
        return 0;
    }
    if (!synced) {
        return 0;
    }

    const auto it = std::upper_bound(lines.begin(),
                                     lines.end(),
                                     playbackTimeSeconds,
                                     [](double value, const LyricLine& line) {
                                         return value < line.timeSeconds;
                                     });
    if (it == lines.begin()) {
        return 0;
    }
    return static_cast<std::size_t>(std::distance(lines.begin(), std::prev(it)));
}

bool LyricsService::prepareTrack(const Track& track) {
    currentData_ = {};
    current_ = nullptr;

    const auto loaded = loadLrc(track);
    if (!loaded || loaded->empty()) {
        status_ = "no local or embedded lyrics";
        return false;
    }

    currentData_ = *loaded;
    current_ = &currentData_;
    status_ = currentData_.sourcePath == "embedded metadata" ? "embedded lyrics" : "lyrics ready";
    return true;
}

void LyricsService::clear() {
    currentData_ = {};
    current_ = nullptr;
    status_ = "no local or embedded lyrics";
}

std::optional<LyricsData> LyricsService::loadLrc(const Track& track) {
    const auto path = localLyricsPathFor(track);
    if (path) {
        std::ifstream in(*path);
        if (in) {
            LyricsData data;
            data.trackPath = track.path;
            data.sourcePath = *path;
            data.synced = true;

            std::ostringstream contents;
            contents << in.rdbuf();
            if (parseTimedLyrics(contents.str(), data)) {
                return data;
            }
        }
    }

    const std::string command = ffprobePath() + " -v quiet -of json -show_entries format_tags "
        + shellQuote(track.path);
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        return std::nullopt;
    }

    std::ostringstream json;
    char buffer[4096] = {0};
    while (std::fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        json << buffer;
    }
    const int rc = pclose(pipe.release());
    if (rc != 0) {
        return std::nullopt;
    }

    const std::vector<std::string> candidates = {"lyrics", "LYRICS", "Lyrics"};
    std::optional<std::string> embedded;
    for (const auto& key : candidates) {
        embedded = extractJsonTagValue(json.str(), key);
        if (embedded && !trim(*embedded).empty()) {
            break;
        }
    }
    if (!embedded || trim(*embedded).empty()) {
        return std::nullopt;
    }

    LyricsData data;
    data.trackPath = track.path;
    data.sourcePath = "embedded metadata";
    data.synced = true;
    if (!parseTimedLyrics(*embedded, data)) {
        data.lines.clear();
        data.synced = false;
        parsePlainLyrics(*embedded, data);
    }

    if (data.lines.empty()) {
        return std::nullopt;
    }
    return data;
}

std::optional<std::string> LyricsService::localLyricsPathFor(const Track& track) {
    const fs::path audio(track.path);
    const fs::path sibling = audio.parent_path() / (audio.stem().string() + ".lrc");
    if (fs::exists(sibling)) {
        return sibling.string();
    }
    return std::nullopt;
}

}
