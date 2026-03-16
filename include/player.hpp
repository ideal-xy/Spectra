#pragma once

#include "track.hpp"

#include <optional>
#include <string>

namespace tplay {

struct PlayerState {
    bool active = false;
    bool paused = false;
    int volume = 70;
    double elapsed = 0.0;
    double duration = 0.0;
    std::string title;
    std::string status = "idle";
};

class Player {
public:
    /// Prepares the player with default volume and IPC state.
    Player();
    /// Stops playback and releases child-process resources.
    ~Player();

    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    /// Starts playback for `track` and connects to the mpv IPC socket.
    /// `track` describes the file to play.
    void play(const Track& track);
    /// Stops the current mpv child if one is running.
    void stop();
    /// Toggles the current pause state.
    void togglePause();
    /// Seeks relative to the current position by `seconds`.
    /// `seconds` may be negative for backward seeks.
    void seek(int seconds);
    /// Adjusts output volume by `delta`, clamped to the supported range.
    /// `delta` is measured in percentage points.
    void adjustVolume(int delta);
    /// Refreshes cached playback state and child liveness.
    void refresh();
    /// Returns true once when the mpv child has exited.
    bool consumeExit();

    /// Returns the cached playback state snapshot.
    const PlayerState& state() const noexcept { return state_; }
    /// Returns a mutable state reference for app-level status updates.
    PlayerState& mutableState() noexcept { return state_; }
    /// Returns the active mpv pid when playback is running.
    std::optional<pid_t> pid() const noexcept;

private:
    /// Opens the unix socket connection to mpv IPC.
    void connectSocket();
    /// Sends a raw JSON command to mpv IPC.
    /// `json` is the command payload to transmit.
    void sendCommand(const std::string& json);
    /// Reads a numeric mpv property as a double.
    /// `property` is the mpv property name.
    double queryDouble(const std::string& property);
    /// Reads a numeric mpv property as an integer.
    /// `property` is the mpv property name.
    int queryInt(const std::string& property);
    /// Refreshes cached values derived from mpv properties.
    void queryState();

    std::string socketPath_;
    int socketFd_ = -1;
    pid_t childPid_ = -1;
    bool childExited_ = false;
    PlayerState state_{};
};

}
