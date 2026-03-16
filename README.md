# Spectra - Terminal Music Player

`Spectra` is a feature-rich, terminal-based music player (TUI) written in C++17. It provides a polished user interface with real-time waveform visualization, frequency bars, and lyrics display.

![Spectra logo](./shot.png)

## Features

- **TUI Interface:** A clean, responsive terminal UI built for performance.
- **Waveform Visualization:** Real-time waveform rendering for the currently playing track.
- **Spectrum Visualizer:** Integrated frequency bar visualizer using `cava`.
- **Smart Queue:** Support for shuffle, repeat, and autoplay modes.
- **Low Resource Usage:** Leverages `mpv` as a robust playback backend.
- **Recursive Library Scanning:** Easily scan entire music directories.

## Requirements

To build and run `tplay`, you need:

- **Compiler:** A C++17 compliant compiler (e.g., `clang++` or `g++`).
- **Dependencies:**
  - `mpv`: Used for audio playback and IPC control.
  - `ffmpeg`: Used for waveform generation.
  - `cava`: (Optional) Used for the frequency bar visualizer.
  - A POSIX-compliant environment (macOS or Linux).

## Installation

### 1. Install Dependencies

On macOS (using Homebrew):
```bash
brew install mpv ffmpeg cava
```

On Linux (using apt):
```bash
sudo apt update
sudo apt install mpv ffmpeg cava


### 2. Build from Source

Clone the repository and run `make`:

```bash
git clone <repository-url>
cd tplay
make
```

The executable will be generated in `bin/tplay`.

## Usage

Start `tplay` by providing the path to your music directory:

```bash
./bin/tplay ~/Music
```

### Controls

| Key | Action |
|-----|--------|
| `j` / `k` | Move selection up/down |
| `Enter` | Play selected track |
| `Space` | Pause / Resume |
| `n` / `p` | Next / Previous track |
| `h` / `l` | Seek backward/forward 5 seconds |
| `+` / `-` | Volume up/down |
| `s` | Toggle Shuffle |
| `a` | Toggle Autoplay |
| `R` | Toggle Repeat Current Track |
| `q` | Quit |

## Project Structure

- `src/`: Implementation files (.cpp).
- `include/`: Header files (.hpp).
- `bin/`: Compiled executable.
- `obj/`: Build artifacts.

## License

[MIT License](LICENSE) (or specify your license)
