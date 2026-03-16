#pragma once

#include <string>
#include <vector>

namespace tplay {

class CavaService {
public:
    /// Prepares fifo-backed cava integration state.
    CavaService();
    /// Stops cava and any waveform feeder child process.
    ~CavaService();

    CavaService(const CavaService&) = delete;
    CavaService& operator=(const CavaService&) = delete;

    /// Launches cava and prepares the fifo transport used for waveform capture.
    void start();
    /// Stops cava, the feeder process, and removes temporary resources.
    void stop();
    /// Polls raw cava output and refreshes cached bar values.
    void poll();
    /// Starts or restarts waveform feeding for `path` beginning at `offsetSeconds`.
    /// `path` is the track to decode and `offsetSeconds` is the seek offset in seconds.
    void playTrack(const std::string& path, double offsetSeconds = 0.0);
    /// Stops the feeder and clears the current waveform display.
    void pause();

    /// Returns the latest parsed bar magnitudes.
    const std::vector<int>& bars() const noexcept { return bars_; }
    /// Returns true when a cava binary is available.
    bool available() const noexcept { return available_; }
    /// Returns true while the cava reader pipe is active.
    bool running() const noexcept { return running_; }
    /// Returns the latest human-readable cava status string.
    const std::string& status() const noexcept { return status_; }

private:
    /// Stops and reaps the ffmpeg waveform feeder child if it exists.
    void stopFeeder();

    std::string cavaPath_;
    std::string configPath_;
    std::string fifoPath_;
    std::string currentTrack_;
    FILE* pipe_ = nullptr;
    int fd_ = -1;
    pid_t feederPid_ = -1;
    std::vector<int> bars_;
    std::string pending_;
    bool available_ = false;
    bool running_ = false;
    std::string status_;
};

}
