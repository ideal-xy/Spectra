# tplay - Terminal Music Player

`tplay` is a feature-rich, terminal-based music player (TUI) written in C++17. It provides a polished user interface with real-time waveform visualization, frequency bars, and lyrics display.

## Project Overview

*   **Main Goal**: Provide a lightweight yet visually rich music playback experience in the terminal.
*   **Technologies**:
    *   **C++17**: Core application logic.
    *   **mpv**: Backend for audio playback, controlled via JSON IPC over Unix sockets.
    *   **ffmpeg**: Used for high-quality waveform generation.
    *   **cava**: Integrated for real-time frequency spectrum visualization.
*   **Architecture**:
    *   `App`: Coordinates subsystems, handles input, and runs the main event loop.
    *   `Player`: Manages the `mpv` lifecycle and communicates via IPC to control playback (volume, seek, pause).
    *   `Ui`: Handles terminal rendering, including progress bars, track info, and waveform drawing.
    *   `WaveformService`: Generates, caches, and provides waveform data for the current track.
    *   `Library` & `Queue`: Manage file scanning and playlist logic (shuffle, repeat, autoplay).
    *   `Terminal`: Manages raw mode and input for a seamless TUI experience.

## Building and Running

### Build Commands
*   `make`: Compiles the project and generates the executable in `bin/tplay`.
*   `make clean`: Removes objects and the compiled binary.

### Running the Application
*   `./bin/tplay <music_directory>`: Starts the player scanning the specified directory.
*   `make run`: Shortcut to build and run the player (requires music directory configuration or default setup).

### Requirements
*   `clang++` or `g++` (C++17 support).
*   `mpv`, `ffmpeg`, and `cava` installed and available in the system path.

## Development Conventions

*   **Namespace**: All project-specific code is wrapped in the `tplay` namespace.
*   **Naming**:
    *   Classes use `PascalCase`.
    *   Member variables end with an underscore (e.g., `player_`).
    *   Methods use `camelCase`.
*   **Documentation**: Public headers use Doxygen-style triple-slash comments (`///`) to describe classes and methods.
*   **Headers**: Use `#pragma once` for header guards.
*   **Error Handling**: Prefer exceptions for fatal initialization errors; use status strings or return codes for transient state updates.
*   **Structure**: 
    *   `include/`: All public and private header files.
    *   `src/`: All implementation files.
    *   `obj/`: Build artifacts (automatically created).
    *   `bin/`: Output binaries (automatically created).
