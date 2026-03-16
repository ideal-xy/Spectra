#pragma once

#include "player.hpp"
#include "queue.hpp"
#include "waveform_service.hpp"

#include <string>
#include <vector>

namespace tplay {

class Ui {
public:
    /// Renders the full TUI frame for the current application state.
    /// `queue` is the playlist, `player` is playback state, `waveform` is waveform state,
    /// `notice` is an optional warning line, `repeat` and `autoplay` are playback modes,
    /// and `width`/`height` are the current terminal dimensions.
    std::string render(const Queue& queue,
                       const PlayerState& player,
                       const WaveformService& waveform,
                       const std::string& notice,
                       bool repeat,
                       bool autoplay,
                       int width,
                       int height) const;

private:
    /// Formats seconds as `MM:SS`.
    /// `seconds` is the duration to display.
    static std::string formatTime(double seconds);
    /// Builds the styled progress line for `elapsed` against `duration`.
    /// `width` is the maximum printable width budget for the rendered line.
    static std::string progressBar(double elapsed, double duration, int width);
    /// Builds a multi-row colored waveform block sized to `width` by `height`.
    /// `bars` supplies bar amplitudes for the current frame.
    static std::vector<std::string> waveLines(const std::vector<float>& bars, int width, int height);
    /// Trims a title-like string to a printable width budget.
    /// `value` is the input text and `maxWidth` is the maximum printable width.
    static std::string trimTitle(const std::string& value, std::size_t maxWidth);
};

}
