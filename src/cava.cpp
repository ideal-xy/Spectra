#include "cava.hpp"

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

namespace tplay {

namespace {

/// Returns the temporary config path used for the cava instance.
std::string configPath() {
    return "/tmp/tplay-cava-" + std::to_string(getpid()) + ".conf";
}

/// Returns the fifo path used to feed raw PCM data into cava.
std::string fifoPath()
{
    return "/tmp/tplay-cava-" + std::to_string(getpid()) + ".fifo";
}

/// Finds `cava` in the current PATH and returns its absolute path when present.
std::string findCavaPath()
{
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) {
        return {};
    }

    std::istringstream paths(pathEnv);
    std::string entry;
    while (std::getline(paths, entry, ':')) {
        if (entry.empty()) {
            continue;
        }
        const std::string candidate = entry + "/cava";
        if (access(candidate.c_str(), X_OK) == 0) {
            return candidate;
        }
    }
    return {};
}

/// Returns true when `fd` can be read without blocking.
bool readReady(int fd) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    timeval timeout {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    const int rc = select(fd + 1, &set, nullptr, nullptr, &timeout);
    return rc > 0 && FD_ISSET(fd, &set);
}

}

/// Initializes temporary paths and the default empty bar state.
CavaService::CavaService() : configPath_(configPath()), fifoPath_(fifoPath()), bars_(32, 0) {}

/// Ensures all cava-side resources are released on destruction.
CavaService::~CavaService()
{
    stop();
}

/// Starts cava in raw-output fifo mode when the binary is available.
void CavaService::start()
{
    if (running_)
    {
        return;
    }

    cavaPath_ = std::string{"/opt/homebrew/bin/cava"};
    // if (!available_)
    // {
    //     status_ = "cava not installed";
    //     running_ = false;
    //     return;
    // }

    unlink(fifoPath_.c_str());
    if (mkfifo(fifoPath_.c_str(), 0600) == -1) {
        status_ = "failed to create wave fifo";
        running_ = false;
        return;
    }

    std::ofstream out(configPath_);
    out << "[general]\n";
    out << "bars = 48\n";
    out << "[input]\n";
    out << "method = fifo\n";
    out << "source = " << fifoPath_ << "\n";
    out << "sample_rate = 44100\n";
    out << "sample_bits = 16\n";
    out << "channels = 2\n";
    out << "[output]\n";
    out << "method = raw\n";
    out << "raw_target = /dev/stdout\n";
    out << "data_format = ascii\n";
    out << "ascii_max_range = 7\n";
    out.close();


    pipe_ = popen((cavaPath_ + " -p " + configPath_ + " 2>/dev/null").c_str(), "r");
    if (!pipe_) {
        status_ = "failed to launch cava";
        running_ = false;
        return;
    }

    fd_ = fileno(pipe_);
    const int flags = fcntl(fd_, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    }

    running_ = true;
    status_ = "live";
}

/// Stops the feeder, closes cava, and removes temporary fifo state.
void CavaService::stop() {
    stopFeeder();
    if (pipe_)
    {
        pclose(pipe_);
        pipe_ = nullptr;
    }
    fd_ = -1;
    running_ = false;
    pending_.clear();
    unlink(fifoPath_.c_str());
}

/// Polls cava raw output and updates cached amplitude bars.
void CavaService::poll()
    {
    if (!running_ || !pipe_ || fd_ == -1) {
        return;
    }

    std::array<char, 256> buffer {};
    while (readReady(fd_)) {
        const ssize_t n = read(fd_, buffer.data(), buffer.size() - 1);
        if (n > 0) {
            pending_.append(buffer.data(), static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) {
            running_ = false;
            status_ = "wave unavailable";
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        running_ = false;
        status_ = "wave unavailable";
        return;
    }

    std::size_t pos = 0;
    while ((pos = pending_.find('\n')) != std::string::npos) {
        const std::string line = pending_.substr(0, pos);
        pending_.erase(0, pos + 1);

        std::vector<int> next;
        std::string token;
        std::istringstream stream(line);
        while (std::getline(stream, token, ';')) {
            if (token.empty()) {
                continue;
            }
            try {
                next.push_back(std::stoi(token));
            } catch (...) {
            }
        }
        if (!next.empty()) {
            bars_ = std::move(next);
        }
    }
}

/// Starts an ffmpeg child that decodes `path` from `offsetSeconds` into the fifo.
void CavaService::playTrack(const std::string& path, double offsetSeconds) {
    if (!running_) {
        return;
    }

    stopFeeder();
    currentTrack_ = path;
    std::fill(bars_.begin(), bars_.end(), 0);

    feederPid_ = fork();
    if (feederPid_ < 0) {
        status_ = "wave feeder failed";
        feederPid_ = -1;
        return;
    }

    if (feederPid_ == 0) {
        const int outFd = open(fifoPath_.c_str(), O_WRONLY);
        if (outFd == -1) {
            _exit(127);
        }

        const int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, STDIN_FILENO);
        dup2(outFd, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(outFd);
        close(devnull);

        std::ostringstream offset;
        offset << std::fixed << std::setprecision(3) << (offsetSeconds < 0.0 ? 0.0 : offsetSeconds);

        execlp("/opt/homebrew/bin/ffmpeg",
               "ffmpeg",
               "-v",
               "quiet",
               "-nostdin",
               "-re",
               "-ss",
               offset.str().c_str(),
               "-i",
               path.c_str(),
               "-f",
               "s16le",
               "-acodec",
               "pcm_s16le",
               "-ac",
               "2",
               "-ar",
               "44100",
               "-",
               nullptr);
        _exit(127);
    }

    status_ = "live";
}

/// Stops waveform feeding and clears the visible bars.
void CavaService::pause()
{
    stopFeeder();
    std::fill(bars_.begin(), bars_.end(), 0);
    if (running_) {
        status_ = "paused";
    }
}

/// Terminates and reaps the ffmpeg waveform feeder child.
void CavaService::stopFeeder()
{
    if (feederPid_ > 0) {
        kill(feederPid_, SIGTERM);
        waitpid(feederPid_, nullptr, 0);
        feederPid_ = -1;
    }
}

}
