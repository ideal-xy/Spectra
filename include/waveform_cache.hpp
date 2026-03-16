#pragma once

#include <algorithm>
#include <complex>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace tplay {

struct SpectrumFrame 
{
    double timeSeconds = 0.0;
    std::vector<float> bars;
};

struct WaveformData 
{
    std::string trackPath;
    double durationSeconds = 0.0;
    double frameStepSeconds = 0.02;
    std::vector<SpectrumFrame> frames;

    bool empty() const noexcept { return frames.empty(); }

    std::vector<float> barsAt(double playbackTimeSeconds) const 
    {
        if (frames.empty()) 
        {
            return {};
        }

        const double clamped = std::clamp(playbackTimeSeconds, 0.0, durationSeconds);
        const std::size_t index = frameIndexAt(clamped);
        return frames[index].bars;
    }

    std::size_t frameIndexAt(double playbackTimeSeconds) const noexcept 
    {
        if (frames.empty()) 
        {
            return 0;
        }

        const double clamped = std::max(0.0, playbackTimeSeconds);
        const std::size_t guessed = static_cast<std::size_t>(clamped / frameStepSeconds);
        return std::min<std::size_t>(guessed, frames.size() - 1);
    }
};

class WaveformCache {
public:
    void clear() 
    {
        items_.clear();
    }

    void put(WaveformData data) {
        for (auto& item : items_) 
        {
            if (item.trackPath == data.trackPath) 
            {
                item = std::move(data);
                return;
            }
        }
        items_.push_back(std::move(data));
    }

    const WaveformData* find(const std::string& trackPath) const noexcept 
    {
        for (const auto& item : items_) 
        {
            if (item.trackPath == trackPath) 
            {
                return &item;
            }
        }
        return nullptr;
    }

private:
    std::vector<WaveformData> items_;
};

class WaveformSerializer {
public:
    // Simple text format:
    // track=<path>
    // duration=<seconds>
    // step=<seconds>
    // frame=<time>|1,2,3,4
    static bool save(const WaveformData& data, const std::string& path) 
    {
        std::ofstream out(path);
        if (!out) {
            return false;
        }

        out << "track=" << data.trackPath << "\n";
        out << "duration=" << data.durationSeconds << "\n";
        out << "step=" << data.frameStepSeconds << "\n";
        for (const auto& frame : data.frames) 
        {
            out << "frame=" << frame.timeSeconds << "|";
            for (std::size_t i = 0; i < frame.bars.size(); ++i) 
            {
                if (i != 0) 
                {
                    out << ",";
                }
                out << frame.bars[i];
            }
            out << "\n";
        }
        return true;
    }

    static std::optional<WaveformData> load(const std::string& path) 
    {
        std::ifstream in(path);
        if (!in) 
        {
            return std::nullopt;
        }

        WaveformData data;
        std::string line;
        while (std::getline(in, line)) 
        {
            if (line.rfind("track=", 0) == 0) 
            {
                data.trackPath = line.substr(6);
                continue;
            }
            if (line.rfind("duration=", 0) == 0) 
            {
                data.durationSeconds = std::stod(line.substr(9));
                continue;
            }
            if (line.rfind("step=", 0) == 0) 
            {
                data.frameStepSeconds = std::stod(line.substr(5));
                continue;
            }
            if (line.rfind("frame=", 0) != 0) 
            {
                continue;
            }

            const std::string payload = line.substr(6);
            const std::size_t split = payload.find('|');
            if (split == std::string::npos) 
            {
                continue;
            }

            SpectrumFrame frame;
            frame.timeSeconds = std::stod(payload.substr(0, split));

            std::istringstream barsStream(payload.substr(split + 1));
            std::string token;
            while (std::getline(barsStream, token, ',')) 
            {
                if (!token.empty()) 
                {
                    frame.bars.push_back(std::stof(token));
                }
            }

            if (!frame.bars.empty()) 
            {
                data.frames.push_back(std::move(frame));
            }
        }

        if (data.trackPath.empty() || data.frames.empty()) 
        {
            return std::nullopt;
        }
        return data;
    }
};

class WaveformBuilder {
public:
    // This is the core rule:
    // 1. Analyze audio once, offline or ahead of playback.
    // 2. Store one spectrum frame every `frameStepSeconds`.
    // 3. During playback, render bars by mpv playback time.
    //
    // `samples` is interleaved stereo PCM in [-1.0, 1.0].
    static WaveformData buildFromPcm(const std::string& trackPath,
                                     const std::vector<float>& samples,
                                     int channels,
                                     int sampleRate,
                                     double frameStepSeconds,
                                     int barCount) {
        WaveformData out;
        out.trackPath = trackPath;
        out.frameStepSeconds = frameStepSeconds;

        if (channels <= 0 || sampleRate <= 0 || frameStepSeconds <= 0.0 || barCount <= 0) {
            return out;
        }
        if (samples.empty()) {
            return out;
        }

        const std::size_t samplesPerFrame =
            static_cast<std::size_t>(std::max(1.0, frameStepSeconds * sampleRate)) * static_cast<std::size_t>(channels);
        const std::size_t totalFrames = (samples.size() + samplesPerFrame - 1) / samplesPerFrame;
        out.durationSeconds = static_cast<double>(samples.size()) /
                              static_cast<double>(channels * sampleRate);
        out.frames.reserve(totalFrames);

        for (std::size_t offset = 0; offset < samples.size(); offset += samplesPerFrame) {
            const std::size_t end = std::min(samples.size(), offset + samplesPerFrame);
            SpectrumFrame frame;
            frame.timeSeconds =
                static_cast<double>(offset / static_cast<std::size_t>(channels)) / static_cast<double>(sampleRate);
            frame.bars = computeBars(samples, offset, end, channels, barCount);
            out.frames.push_back(std::move(frame));
        }

        normalizeFrames(out.frames, barCount);

        return out;
    }

private:
    static void normalizeFrames(std::vector<SpectrumFrame>& frames, int barCount) {
        if (frames.empty() || barCount <= 0) {
            return;
        }

        std::vector<float> peaks(static_cast<std::size_t>(barCount), 0.0f);
        for (const auto& frame : frames) {
            for (int i = 0; i < barCount && i < static_cast<int>(frame.bars.size()); ++i) {
                peaks[static_cast<std::size_t>(i)] =
                    std::max(peaks[static_cast<std::size_t>(i)], frame.bars[static_cast<std::size_t>(i)]);
            }
        }

        for (auto& frame : frames) {
            for (int i = 0; i < barCount && i < static_cast<int>(frame.bars.size()); ++i) {
                const float peak = std::max(0.01f, peaks[static_cast<std::size_t>(i)] * 0.92f);
                const float normalized = std::clamp(frame.bars[static_cast<std::size_t>(i)] / peak, 0.0f, 1.0f);
                const float ratio = static_cast<float>(i) / static_cast<float>(std::max(1, barCount - 1));
                const float contour = 0.58f + 0.42f * std::sqrt(ratio);
                const float curve = 1.18f - 0.28f * ratio;
                frame.bars[static_cast<std::size_t>(i)] =
                    std::clamp(std::pow(normalized, curve) * contour, 0.0f, 1.0f);
            }
        }

        if (frames.size() < 3) {
            for (auto& frame : frames) {
                smoothAcrossBands(frame.bars);
            }
            return;
        }

        std::vector<SpectrumFrame> smoothed = frames;
        for (std::size_t frameIndex = 1; frameIndex + 1 < frames.size(); ++frameIndex) {
            for (int bar = 0; bar < barCount && bar < static_cast<int>(frames[frameIndex].bars.size()); ++bar) {
                smoothed[frameIndex].bars[static_cast<std::size_t>(bar)] =
                    0.2f * frames[frameIndex - 1].bars[static_cast<std::size_t>(bar)] +
                    0.6f * frames[frameIndex].bars[static_cast<std::size_t>(bar)] +
                    0.2f * frames[frameIndex + 1].bars[static_cast<std::size_t>(bar)];
            }
        }
        for (auto& frame : smoothed) {
            smoothAcrossBands(frame.bars);
        }
        frames = std::move(smoothed);
    }

    static void smoothAcrossBands(std::vector<float>& bars) {
        if (bars.size() < 3) {
            return;
        }

        std::vector<float> copy = bars;
        for (std::size_t i = 1; i + 1 < bars.size(); ++i) {
            copy[i] = 0.2f * bars[i - 1] + 0.6f * bars[i] + 0.2f * bars[i + 1];
        }
        bars = std::move(copy);
    }

    static std::size_t nextPow2(std::size_t value) {
        std::size_t result = 1;
        while (result < value) {
            result <<= 1U;
        }
        return result;
    }

    static void fft(std::vector<std::complex<double>>& data) {
        const std::size_t n = data.size();
        if (n <= 1) {
            return;
        }

        for (std::size_t i = 1, j = 0; i < n; ++i) {
            std::size_t bit = n >> 1U;
            for (; (j & bit) != 0; bit >>= 1U) {
                j ^= bit;
            }
            j ^= bit;
            if (i < j) {
                std::swap(data[i], data[j]);
            }
        }

        for (std::size_t len = 2; len <= n; len <<= 1U) {
            const double angle = -2.0 * 3.14159265358979323846 / static_cast<double>(len);
            const std::complex<double> root(std::cos(angle), std::sin(angle));
            for (std::size_t i = 0; i < n; i += len) {
                std::complex<double> w(1.0, 0.0);
                for (std::size_t j = 0; j < len / 2; ++j) {
                    const std::complex<double> even = data[i + j];
                    const std::complex<double> odd = data[i + j + len / 2] * w;
                    data[i + j] = even + odd;
                    data[i + j + len / 2] = even - odd;
                    w *= root;
                }
            }
        }
    }

    static std::vector<float> computeBars(const std::vector<float>& samples,
                                          std::size_t begin,
                                          std::size_t end,
                                          int channels,
                                          int barCount) {
        const std::size_t frameSamples = end - begin;
        if (frameSamples == 0) {
            return std::vector<float>(static_cast<std::size_t>(barCount), 0.0f);
        }

        const std::size_t monoSamples = frameSamples / static_cast<std::size_t>(channels);
        if (monoSamples == 0) {
            return std::vector<float>(static_cast<std::size_t>(barCount), 0.0f);
        }

        const std::size_t fftSize = nextPow2(std::max<std::size_t>(monoSamples, 512));
        std::vector<std::complex<double>> windowed(fftSize, std::complex<double>(0.0, 0.0));
        for (std::size_t i = 0; i < monoSamples; ++i) {
            double mixed = 0.0;
            for (int ch = 0; ch < channels; ++ch) {
                mixed += samples[begin + i * static_cast<std::size_t>(channels) + static_cast<std::size_t>(ch)];
            }
            mixed /= static_cast<double>(channels);

            const double window = 0.5 - 0.5 * std::cos((2.0 * 3.14159265358979323846 * static_cast<double>(i)) /
                                                        static_cast<double>(std::max<std::size_t>(1, monoSamples - 1)));
            windowed[i] = std::complex<double>(mixed * window, 0.0);
        }

        fft(windowed);

        const double sampleRate = 44100.0;
        const double minFreq = 40.0;
        const double maxFreq = 18000.0;
        const std::size_t maxBin = fftSize / 2U;
        std::vector<float> bars(static_cast<std::size_t>(barCount), 0.0f);
        for (int i = 0; i < barCount; ++i) {
            const double leftRatio = static_cast<double>(i) / static_cast<double>(barCount);
            const double rightRatio = static_cast<double>(i + 1) / static_cast<double>(barCount);
            const double startFreq = minFreq * std::pow(maxFreq / minFreq, leftRatio);
            const double endFreq = minFreq * std::pow(maxFreq / minFreq, rightRatio);
            const std::size_t startBin = std::max<std::size_t>(1, static_cast<std::size_t>(std::floor(startFreq * static_cast<double>(fftSize) / sampleRate)));
            const std::size_t endBin = std::min<std::size_t>(maxBin - 1, std::max(startBin + 1, static_cast<std::size_t>(std::ceil(endFreq * static_cast<double>(fftSize) / sampleRate))));

            double energy = 0.0;
            for (std::size_t bin = startBin; bin <= endBin; ++bin) {
                const double mag = std::abs(windowed[bin]);
                energy += mag * mag;
            }

            const double average = energy / static_cast<double>(std::max<std::size_t>(1, endBin - startBin + 1));
            const double normalized = average / static_cast<double>(fftSize * fftSize);
            const double db = 10.0 * std::log10(1.0 + normalized * 800000.0);
            const double tilt = 0.9 + 0.4 * std::sqrt(rightRatio);
            bars[static_cast<std::size_t>(i)] =
                static_cast<float>(std::max(0.0, db * tilt));
        }

        for (std::size_t i = 1; i + 1 < bars.size(); ++i) {
            bars[i] = 0.25f * bars[i - 1] + 0.5f * bars[i] + 0.25f * bars[i + 1];
        }
        return bars;
    }
};

}  // namespace tplay
