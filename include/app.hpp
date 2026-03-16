#pragma once

#include "library.hpp"
#include "player.hpp"
#include "queue.hpp"
#include "terminal.hpp"
#include "ui.hpp"
#include "waveform_service.hpp"

#include <string>

namespace tplay {

class App {
public:
    /// Parses CLI arguments, initializes subsystems, and starts the TUI loop.
    /// `argc` and `argv` are the process command-line arguments.
    int run(int argc, char** argv);

private:
    /// Runs the main event/render loop until shutdown.
    int runLoop();
    /// Handles a single raw keypress and returns true when state changed.
    /// `ch` is the raw input byte from stdin.
    bool handleInput(char ch);
    /// Refreshes cached playback state from the player subsystem.
    void syncPlayback();
    /// Starts playback for the current queue item and synchronizes the waveform feeder.
    void startCurrentTrack();
    /// Draws the current frame to stdout.
    void redraw();
    /// Prints CLI usage instructions.
    /// `argv0` is the executable name shown in the help text.
    void printHelp(const char* argv0) const;

    Terminal terminal_;
    Library library_;
    Queue queue_;
    Player player_;
    WaveformService waveform_;
    Ui ui_;
    bool repeat_ = false;
    bool autoplay_ = true;
    bool quit_ = false;
    std::string musicRoot_;
    std::string notice_;
};

}
