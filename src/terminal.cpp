#include "terminal.hpp"

#include <stdexcept>
#include <sys/ioctl.h>
#include <unistd.h>

namespace tplay {

/// Enables raw input mode so the TUI can react to single keypresses.
Terminal::Terminal() {
    if (tcgetattr(STDIN_FILENO, &original_) == -1) {
        throw std::runtime_error("failed to read terminal attributes");
    }

    termios raw = original_;
    raw.c_lflag &= static_cast<unsigned long>(~(ICANON | ECHO));
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        throw std::runtime_error("failed to enable raw terminal mode");
    }

    active_ = true;
}

/// Restores the original terminal attributes when the TUI exits.
Terminal::~Terminal() {
    if (active_) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_);
    }
}

/// Returns the current terminal width, or a conservative default.
int Terminal::width() const {
    winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return 120;
    }
    return ws.ws_col;
}

/// Returns the current terminal height, or a conservative default.
int Terminal::height() const {
    winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_row == 0) {
        return 36;
    }
    return ws.ws_row;
}

}
