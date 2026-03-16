#include "player.hpp"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

namespace tplay {

namespace {

constexpr const char* kMpvSocketPath = "/tmp/mpv.sock";

int connect_mpv() 
{
    sockaddr_un addr {};
    addr.sun_family = AF_UNIX;

    constexpr size_t path_len = sizeof(addr.sun_path) - 1;
    std::strncpy(addr.sun_path, kMpvSocketPath, path_len);

    const int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) 
    {
        return -1;
    }

    for (int i = 0; i < 400; ++i) 
    {
        if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) 
        {
            return sock;
        }

        if (errno == ENOENT || errno == ECONNREFUSED) 
        {
            usleep(i < 10 ? 15000 : 50000);
            continue;
        }

        break;
    }

    close(sock);
    return -1;
}


/// Sends a raw JSON command to `sock`.
/// `sock` is the connected mpv IPC socket and `json` is the payload.
void mpv_cmd(int sock, const char* json) 
{
    write(sock, json, std::strlen(json));
}

/// Queries mpv playback time over `mpv_sock`.
/// `mpv_sock` is the connected mpv IPC socket.
double mpv_get_elapsed(int mpv_sock) 
{
    const char* cmd = R"({"command": ["get_property", "playback-time"]})"
                      "\n";

    if (write(mpv_sock, cmd, std::strlen(cmd)) < 0) {
        return -1.0;
    }

    char buf[1024];
    const ssize_t n = read(mpv_sock, buf, sizeof(buf) - 1);
    if (n <= 0) {
        return -1.0;
    }

    buf[n] = '\0';

    const char* p = std::strstr(buf, "\"data\":");
    if (!p) {
        return -1.0;
    }

    p += 7;
    return std::strtod(p, nullptr);
}

}

/// Initializes the player with the fixed socket path and default volume.
Player::Player() : socketPath_(kMpvSocketPath)
{
    state_.volume = 70;
}

/// Stops any active child process before destruction.
Player::~Player() {
    stop();
}

/// Returns the active mpv pid when available.
std::optional<pid_t> Player::pid() const noexcept {
    if (childPid_ <= 0) {
        return std::nullopt;
    }
    return childPid_;
}

/// Launches a song with mpv and connects the IPC control socket.
void Player::play(const Track& track) 
{
    stop();
    // unlink(socketPath_.c_str());

    childExited_ = false;
    childPid_ = fork();
    if (childPid_ < 0)
    {
        throw std::runtime_error("fork failed");
    }

    if (childPid_ == 0) 
    {
        std::string ipcArg = "--input-ipc-server=" + socketPath_;
        const int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        close(devnull);

        execlp("mpv",
               "mpv",
               "--no-video",
               "--really-quiet",
               "--terminal=no",
               ipcArg.c_str(),
               track.path.c_str(),
               nullptr);
        _exit(127);
    }

    state_.active = true;
    state_.paused = false;
    state_.title = track.title;
    state_.duration = track.duration;
    state_.elapsed = 0.0;
    state_.volume = 70;
    state_.status = "starting";

    connectSocket();
    state_.status = "playing";
}

/// Stops mpv, closes IPC resources, and resets cached playback state.
void Player::stop() {
    if (socketFd_ != -1)
    {
        sendCommand(R"({"command":["quit"]})");
        close(socketFd_);
        socketFd_ = -1;
    }

    if (childPid_ > 0) {
        kill(childPid_, SIGTERM);
        waitpid(childPid_, nullptr, 0);
        childPid_ = -1;
    }
    unlink(socketPath_.c_str());

    state_.active = false;
    state_.paused = false;
    state_.elapsed = 0.0;
    state_.status = "stopped";
}

/// Toggles pause state by sending the corresponding mpv property update.
void Player::togglePause() {
    if (!state_.active) {
        return;
    }
    mpv_cmd(socketFd_, state_.paused
        ? R"({"command": ["set_property", "pause", false]})""\n"
        : R"({"command": ["set_property", "pause", true]})""\n");
    state_.paused = !state_.paused;
    state_.status = state_.paused ? "paused" : "playing";
}

/// Seeks playback relative to the current position by `seconds`.
void Player::seek(int seconds) {
    if (!state_.active) {
        return;
    }
    const std::string cmd = "{\"command\": [\"seek\", " + std::to_string(seconds) + "]}\n";
    mpv_cmd(socketFd_, cmd.c_str());
}

/// Adjusts cached and remote volume by `delta`.
void Player::adjustVolume(int delta) {
    if (!state_.active) {
        return;
    }
    state_.volume = std::max(0, std::min(100, state_.volume + delta));
    const std::string cmd =
        "{\"command\": [\"set_property\", \"volume\", " + std::to_string(state_.volume) + "]}\n";
    mpv_cmd(socketFd_, cmd.c_str());
}

/// Reaps exited children and refreshes cached elapsed playback time.
void Player::refresh() {
    if (childPid_ > 0) {
        int status = 0;
        const pid_t result = waitpid(childPid_, &status, WNOHANG);
        if (result == childPid_) {
            childExited_ = true;
        }
    }

    if (!state_.active) {
        return;
    }
    state_.elapsed = mpv_get_elapsed(socketFd_);
    if (state_.elapsed < 0.0) {
        state_.elapsed = 0.0;
    }
}

/// Returns true once after mpv exits so the app can advance the queue.
bool Player::consumeExit() {
    if (!childExited_) {
        return false;
    }
    childExited_ = false;
    state_.active = false;
    state_.paused = false;
    state_.elapsed = state_.duration;
    state_.status = "stopped";
    if (socketFd_ != -1) {
        close(socketFd_);
        socketFd_ = -1;
    }
    if (childPid_ > 0) {
        waitpid(childPid_, nullptr, 0);
        childPid_ = -1;
    }
    return true;
}

/// Connects `socketFd_` to the mpv IPC server or throws on failure.
void Player::connectSocket() 
{
    socketFd_ = connect_mpv();
    if (socketFd_ == -1) 
    {
        throw std::runtime_error("failed to connect to mpv IPC socket");
    }
}

/// Sends `json` as a newline-terminated command over the mpv IPC socket.
void Player::sendCommand(const std::string& json) {
    if (socketFd_ == -1) {
        return;
    }
    const std::string payload = json + "\n";
    (void)write(socketFd_, payload.c_str(), payload.size());
}

/// Reads a numeric mpv property named `property` as a double.
double Player::queryDouble(const std::string& property) {
    if (socketFd_ == -1) {
        return 0.0;
    }
    const std::string command =
        "{\"command\":[\"get_property\",\"" + property + "\"]}\n";
    if (write(socketFd_, command.c_str(), command.size()) < 0) {
        return 0.0;
    }

    char buffer[1024] = {0};
    const ssize_t n = read(socketFd_, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        return 0.0;
    }

    const char* pos = std::strstr(buffer, "\"data\":");
    if (!pos) {
        return 0.0;
    }
    return std::strtod(pos + 7, nullptr);
}

/// Reads a numeric mpv property named `property` as an integer.
int Player::queryInt(const std::string& property) {
    return static_cast<int>(queryDouble(property));
}

/// Refreshes cached state from mpv properties.
void Player::queryState() {
    state_.elapsed = mpv_get_elapsed(socketFd_);
}

}
