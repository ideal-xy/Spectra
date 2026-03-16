#pragma once

#include <termios.h>

namespace tplay {

class Terminal {
public:
    /// Switches stdin into raw mode for single-key TUI interaction.
    Terminal();
    /// Restores the original terminal configuration.
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    /// Returns the current terminal width in character cells.
    int width() const;
    /// Returns the current terminal height in character cells.
    int height() const;

private:
    termios original_{};
    bool active_ = false;
};

}
