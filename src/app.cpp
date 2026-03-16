#include "app.hpp"

#include <csignal>
#include <iostream>
#include <poll.h>
#include <unistd.h>

namespace tplay {

/// Initializes the app from CLI arguments and enters the TUI event loop.
int App::run(int argc, char** argv)
{
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        printHelp(argv[0]);
        return 0;
    }

    std::signal(SIGPIPE, SIG_IGN);
    musicRoot_ = argv[1];
    auto tracks = library_.scan(musicRoot_);
    if (tracks.empty())
    {
        std::cerr << "No audio files found in " << musicRoot_ << "\n";
        return 1;
    }

    queue_.setTracks(std::move(tracks));
    player_.mutableState().status = "ready";
    redraw();
    startCurrentTrack();
    redraw();
    return runLoop();
}

/// Runs the main poll/render loop until the user quits.
int App::runLoop()
{
    while (!quit_)
    {
        pollfd fds[1];
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        (void)poll(fds, 1, 120);

        if (fds[0].revents & POLLIN)
        {
            char ch = 0;
            while (read(STDIN_FILENO, &ch, 1) > 0)
            {
                handleInput(ch);
                if (quit_)
                {
                    break;
                }
            }
        }

        if (quit_)
        {
            break;
        }

        player_.refresh();
        if (player_.consumeExit()) {
            if (!notice_.empty()) {
                autoplay_ = false;
            } else if (repeat_) {
                startCurrentTrack();
            } else if (autoplay_ && queue_.next()) {
                startCurrentTrack();
            }
        }
        redraw();
    }

    player_.stop();
    waveform_.clear();
    std::cout << "\033[2J\033[H";
    return 0;
}

/// Applies a single raw keypress to application state.
bool App::handleInput(char ch)
{
    switch (ch) {
        case 'q':
            quit_ = true;
            return true;
        case 'j':
            queue_.moveSelection(1);
            return true;
        case 'k':
            queue_.moveSelection(-1);
            return true;
        case '\n':
        case '\r':
            queue_.jumpToSelected();
            startCurrentTrack();
            return true;
        case ' ':
            player_.togglePause();
            return true;
        case 'n':
            if (queue_.next())
            {
                startCurrentTrack();
            }
            return true;
        case 'p':
            if (queue_.previous()) {
                startCurrentTrack();
            }
            return true;
        case 's':
            // if (queue_.shuffled())
            // {
            //     queue_.unshuffle();
            // } else {
            //     queue_.shuffle();
            // }
            queue_.shuffle();
            return true;
        case 'a':
            autoplay_ = !autoplay_;
            return true;
        case 'R':
            repeat_ = !repeat_;
            return true;
        case 'h':
            player_.seek(-5);
            return true;
        case 'l':
            player_.seek(5);
            return true;
        case '-':
            player_.adjustVolume(-5);
            return true;
        case '+':
        case '=':
            player_.adjustVolume(5);
            return true;
        default:
            return false;
    }
}

/// Refreshes cached playback state from the player subsystem.
void App::syncPlayback() {
    player_.refresh();
}

/// Starts the currently selected track and synchronizes the waveform feeder.
void App::startCurrentTrack()
{
    if (const Track* track = queue_.current())
    {
        try
        {
            (void)waveform_.prepareTrack(*track);
            player_.play(*track);
            notice_.clear();
        } catch (const std::exception& ex) {
            player_.mutableState().title = track->title;
            player_.mutableState().active = false;
            player_.mutableState().paused = false;
            player_.mutableState().elapsed = 0.0;
            player_.mutableState().duration = track->duration;
            player_.mutableState().status = "mpv unavailable";
            waveform_.clear();
            notice_ = ex.what();
        }
    }
}

/// Renders the latest frame to stdout.
void App::redraw()
{
    std::cout << ui_.render(queue_,
                            player_.state(),
                            waveform_,
                            notice_,
                            repeat_,
                            autoplay_,
                            terminal_.width(),
                            terminal_.height())
              << std::flush;
}

/// Prints the command-line help text using `argv0` as the executable name.
void App::printHelp(const char* argv0) const {
    std::cout
        << "Usage: " << argv0 << " <music_dir>\n"
        << "Controls:\n"
        << "  j/k      move selection\n"
        << "  enter    play selected track\n"
        << "  space    pause/resume\n"
        << "  n/p      next/previous\n"
        << "  h/l      seek -/+ 5 seconds\n"
        << "  +/-      volume down/up\n"
        << "  s        shuffle toggle\n"
        << "  a        autoplay toggle\n"
        << "  R        repeat current track\n"
        << "  q        quit\n";
}

}
