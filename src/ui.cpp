#include "ui.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <cwchar>
#include <clocale>
#include <string>
#include <sstream>
#include <vector>

namespace tplay {

namespace {

const char* kReset = "\033[0m";
const char* kTitle = "\033[38;2;245;167;109m";
const char* kAccent = "\033[38;2;104;196;255m";
const char* kMuted = "\033[38;2;117;129;160m";
const char* kGreen = "\033[38;2;127;221;157m";
const char* kPink = "\033[38;2;244;143;177m";
const char* kGold = "\033[38;2;244;208;112m";
const char* kRed = "\033[38;2;255;135;135m";
const char* kPanel = "\033[38;2;70;79;105m";

struct Rgb {
    int r;
    int g;
    int b;
};

/// Linearly interpolates between `start` and `end` using `ratio`.
Rgb blend(const Rgb& start, const Rgb& end, double ratio) {
    return {
        static_cast<int>(start.r + (end.r - start.r) * ratio),
        static_cast<int>(start.g + (end.g - start.g) * ratio),
        static_cast<int>(start.b + (end.b - start.b) * ratio),
    };
}

/// Converts an RGB triplet into a 24-bit ANSI foreground escape sequence.
std::string rgb(const Rgb& color) {
    return "\033[38;2;" + std::to_string(color.r) + ";" +
           std::to_string(color.g) + ";" + std::to_string(color.b) + "m";
}

/// Computes the visible terminal width of `text`, ignoring ANSI escapes.
int visibleWidth(const std::string& text) {
    int width = 0;
    std::mbstate_t state {};
    const char* ptr = text.c_str();
    const char* end = ptr + text.size();

    while (ptr < end) {
        if (*ptr == '\033') {
            while (ptr < end && *ptr != 'm') {
                ++ptr;
            }
            if (ptr < end) {
                ++ptr;
            }
            continue;
        }

        wchar_t wc = 0;
        const std::size_t len = std::mbrtowc(&wc, ptr, static_cast<std::size_t>(end - ptr), &state);
        if (len == static_cast<std::size_t>(-1) || len == static_cast<std::size_t>(-2) || len == 0) {
            ++width;
            ++ptr;
            std::mbstate_t reset {};
            state = reset;
            continue;
        }

        const int cell = wcwidth(wc);
        width += cell > 0 ? cell : 0;
        ptr += len;
    }

    return width;
}

/// Repeats `token` exactly `count` times.
std::string repeatText(const std::string& token, int count) {
    if (count <= 0) {
        return {};
    }
    std::string result;
    result.reserve(static_cast<std::size_t>(count) * token.size());
    for (int i = 0; i < count; ++i) {
        result += token;
    }
    return result;
}

/// Pads `text` with spaces so its visible width reaches `width`.
std::string padRight(const std::string& text, int width) {
    if (width <= 0) {
        return {};
    }

    const int printed = visibleWidth(text);
    if (printed >= width) {
        return text;
    }
    return text + std::string(static_cast<std::size_t>(width - printed), ' ');
}

/// Trims `text` to at most `width` visible terminal cells.
std::string trimVisible(const std::string& text, int width) {
    if (width <= 0) {
        return {};
    }

    std::string result;
    std::mbstate_t state {};
    const char* ptr = text.c_str();
    const char* end = ptr + text.size();
    int used = 0;

    while (ptr < end) {
        if (*ptr == '\033') {
            const char* start = ptr;
            while (ptr < end && *ptr != 'm') {
                ++ptr;
            }
            if (ptr < end) {
                ++ptr;
            }
            result.append(start, ptr);
            continue;
        }

        wchar_t wc = 0;
        const std::size_t len = std::mbrtowc(&wc, ptr, static_cast<std::size_t>(end - ptr), &state);
        if (len == static_cast<std::size_t>(-1) || len == static_cast<std::size_t>(-2) || len == 0) {
            if (used + 1 > width) {
                break;
            }
            result.push_back(*ptr);
            ++ptr;
            ++used;
            std::mbstate_t reset {};
            state = reset;
            continue;
        }

        const int cell = std::max(0, wcwidth(wc));
        if (used + cell > width) {
            break;
        }
        result.append(ptr, len);
        ptr += len;
        used += cell;
    }

    return result;
}

/// Renders one boxed body line within a printable width budget of `innerWidth`.
std::string boxLine(const std::string& text, int innerWidth) {
    const std::string fitted = visibleWidth(text) > innerWidth ? trimVisible(text, innerWidth) : text;
    return std::string(kPanel) + "│" + kReset + " " + padRight(fitted, innerWidth) + " " + std::string(kPanel) + "│" + kReset;
}

/// Appends a titled boxed panel to `out`.
/// `title` names the panel, `lines` are the body rows, `width` is the outer width,
/// and `titleColor` colors the title text.
void appendBox(std::ostringstream& out,
               const std::string& title,
               const std::vector<std::string>& lines,
               int width,
               const char* titleColor) {
    const int innerWidth = std::max(10, width - 4);
    const std::string heading = std::string(titleColor) + " " + title + " " + kReset;
    const int topSpanWidth = innerWidth + 2;
    const int fillWidth = std::max(0, topSpanWidth - visibleWidth(heading));
    out << std::string(kPanel) << "┌" << kReset << heading << std::string(kPanel)
        << repeatText("─", fillWidth) << "┐" << kReset << "\n";
    for (const auto& line : lines) {
        out << boxLine(line, innerWidth) << "\n";
    }
    out << std::string(kPanel) << "└" << repeatText("─", innerWidth + 2) << "┘" << kReset << "\n";
}

}

/// Formats a duration in seconds as `MM:SS`.
std::string Ui::formatTime(double seconds) {
    const int total = static_cast<int>(seconds);
    const int mins = total / 60;
    const int secs = total % 60;
    std::ostringstream out;
    out << std::setw(2) << std::setfill('0') << mins
        << ":"
        << std::setw(2) << std::setfill('0') << secs;
    return out.str();
}

/// Builds the colored progress line for `elapsed` against `duration` within `width`.
std::string Ui::progressBar(double elapsed, double duration, int width) {
    const double shownElapsed = (duration > 0.0) ? std::min(elapsed, duration) : 0.0;
    const Rgb start {251, 128, 114};
    const Rgb end {80, 200, 179};
    const float ratio = (duration > 0.0) ? static_cast<float>(shownElapsed / duration) : 0.0f;
    const std::string suffix = " " + [&]() {
        std::ostringstream s;
        s << std::fixed << std::setprecision(1) << (ratio * 100.0f) << "% "
          << "(" << formatTime(shownElapsed) << "/" << formatTime(duration) << ")";
        return s.str();
    }();

    int usable = std::max(8, width - static_cast<int>(suffix.size()) - 5);
    std::string rendered;
    while (usable >= 8) {
        const int filled = static_cast<int>(ratio * usable);
        std::ostringstream bar;
        bar << rgb(start) << "⏻ " << "";
        for (int i = 1; i <= usable; ++i) {
            const double step = static_cast<double>(i) / static_cast<double>(usable);
            const Rgb current = blend(start, end, step);
            if (i <= filled) {
                bar << rgb(current) << "█";
            } else {
                bar << kMuted << "━";
            }
        }
        bar << (ratio >= 1.0f ? rgb(end) : std::string(kMuted)) << ""
            << kReset << kMuted << suffix << kReset;
        rendered = bar.str();
        if (visibleWidth(rendered) <= width) {
            return rendered;
        }
        --usable;
    }
    return trimVisible(rendered, width);
}

/// Builds a compact bottom-up spectrum from waveform bars within `width` and `height`.
std::vector<std::string> Ui::waveLines(const std::vector<float>& bars, int width, int height) {
    const int limit = std::min<int>(width, bars.size());
    const Rgb start {112, 215, 255};
    const Rgb mid {165, 255, 214};
    const Rgb end {255, 177, 122};
    const int rows = std::max(4, height);
    static const char* kLevels[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    std::vector<std::string> lines(static_cast<std::size_t>(rows), "");
    for (int row = 0; row < rows; ++row) {
        lines[static_cast<std::size_t>(row)].reserve(static_cast<std::size_t>(limit * 20));
    }

    for (int i = 0; i < limit; ++i) {
        const float amplitude = std::clamp(bars[static_cast<std::size_t>(i)], 0.0f, 1.0f);
        const float totalUnits = amplitude * static_cast<float>(rows * 8);
        const double ratio = limit > 1 ? static_cast<double>(i) / static_cast<double>(limit - 1) : 0.0;
        const Rgb color = ratio < 0.5
            ? blend(start, mid, ratio * 2.0)
            : blend(mid, end, (ratio - 0.5) * 2.0);

        for (int row = 0; row < rows; ++row) {
            const int fromBottom = rows - 1 - row;
            const float rowFill = std::clamp(totalUnits - static_cast<float>(fromBottom * 8), 0.0f, 8.0f);
            const int level = static_cast<int>(std::round(rowFill));
            lines[static_cast<std::size_t>(row)] += (level > 0 ? rgb(color) : std::string(kMuted)) + kLevels[level];
        }
    }

    for (auto& line : lines) {
        line += kReset;
    }
    return lines;
}

/// Trims `value` to the printable width budget `maxWidth`, preserving an ellipsis.
std::string Ui::trimTitle(const std::string& value, std::size_t maxWidth) {
    if (visibleWidth(value) <= static_cast<int>(maxWidth)) {
        return value;
    }
    if (maxWidth < 4) {
        return trimVisible(value, static_cast<int>(maxWidth));
    }
    return trimVisible(value, static_cast<int>(maxWidth) - 3) + "...";
}

/// Renders the full boxed TUI for the current playback, queue, and waveform state.
std::string Ui::render(const Queue& queue,
                       const PlayerState& player,
                       const WaveformService& waveform,
                       const std::string& notice,
                       bool repeat,
                       bool autoplay,
                       int width,
                       int height) const {
    std::setlocale(LC_ALL, "");

    std::ostringstream out;
    out << "\033[2J\033[H";
    const int outerWidth = std::max(72, width - 6);
    const int innerWidth = outerWidth - 4;
    out << std::string(kPanel) << "╔" << repeatText("═", outerWidth - 2) << "╗" << kReset << "\n";
    out << boxLine(trimTitle(std::string(kTitle) + "Spectra" + kReset + "  " + kMuted + "A boxed terminal player with live queue, status, and spectrum" + kReset,
                             static_cast<std::size_t>(outerWidth - 4)), outerWidth - 4) << "\n";
    out << boxLine(trimTitle(std::string(kAccent) + "[space]" + kReset + " pause  "
                  + kAccent + "[j/k]" + kReset + " move  "
                  + kAccent + "[enter]" + kReset + " play  "
                  + kAccent + "[n/p]" + kReset + " next/prev  "
                  + kAccent + "[s]" + kReset + " shuffle  "
                  + kAccent + "[q]" + kReset + " quit",
                  static_cast<std::size_t>(outerWidth - 4)), outerWidth - 4) << "\n";
    out << std::string(kPanel) << "╠" << repeatText("═", outerWidth - 2) << "╣" << kReset << "\n";

    std::vector<std::string> nowPlaying;
    nowPlaying.push_back(std::string(kGold) + trimTitle(player.title.empty() ? "idle" : player.title, static_cast<std::size_t>(innerWidth - 2)) + kReset);
    // nowPlaying.push_back("\n");
    nowPlaying.push_back(progressBar(player.elapsed, player.duration, std::max(14, innerWidth - 22)));
    // nowPlaying.push_back(trimTitle("state: " + player.status +
    //                      "   volume: " + std::to_string(player.volume) +
    //                      "   repeat: " + (repeat ? "on" : "off") +
    //                      "   autoplay: " + (autoplay ? "on" : "off") +
    //                      "   order: " + (queue.shuffled() ? "shuffled" : "library"),
    //                      static_cast<std::size_t>(innerWidth - 2)));
    appendBox(out, "Now Playing", nowPlaying, outerWidth, kGreen);

    if (!notice.empty()) {
        appendBox(out, "Notice", {std::string(kRed) + trimTitle(notice, static_cast<std::size_t>(innerWidth - 2)) + kReset}, outerWidth, kPink);
    }

    std::vector<std::string> waveBox;
    const std::string waveStatus = player.paused ? "paused" : waveform.status();
    waveBox.push_back(trimTitle("status: " + waveStatus,
                                static_cast<std::size_t>(innerWidth - 2)));
    const int spectrumWidth = std::min(72, std::max(40, innerWidth - 2));
    const auto bars = player.paused
        ? std::vector<float>(static_cast<std::size_t>(spectrumWidth), 0.0f)
        : waveform.barsAt(player.elapsed, spectrumWidth);
    const auto spectrumRows = waveLines(bars, spectrumWidth, 7);
    for (const auto& row : spectrumRows) {
        const int pad = std::max(0, (innerWidth - static_cast<int>(bars.size())) / 2);
        waveBox.push_back(std::string(static_cast<std::size_t>(pad), ' ') + row);
    }
    appendBox(out, "Spectrum", waveBox, outerWidth, kPink);

    std::vector<std::string> queueLines;
    const auto& tracks = queue.tracks();
    const std::size_t visible = height > 28 ? 14 : 10;
    std::size_t start = 0;
    if (queue.selectedIndex() > visible / 2) {
        start = queue.selectedIndex() - visible / 2;
    }
    if (start + visible > tracks.size() && tracks.size() > visible) {
        start = tracks.size() - visible;
    }
    const std::size_t end = std::min(tracks.size(), start + visible);
    for (std::size_t i = start; i < end; ++i) {
        const bool selected = i == queue.selectedIndex();
        const bool current = i == queue.currentIndex();
        std::ostringstream line;
        line << (selected ? std::string(kAccent) + ">" + kReset : " ");
        line << (current ? std::string(kGold) + "*" + kReset : " ");
        line << " ";
        line << std::setw(2) << (i + 1) << " ";
        line << trimTitle(tracks[i].title, static_cast<std::size_t>(std::max(10, innerWidth - 24)));
        line << "  " << kMuted << formatTime(tracks[i].duration) << kReset;
        queueLines.push_back(line.str());
    }
    appendBox(out, "Queue", queueLines, outerWidth, kAccent);
    out << std::string(kPanel) << "╚" << repeatText("═", outerWidth - 2) << "╝" << kReset;

    return out.str();
}

}
