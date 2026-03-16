#pragma once

#include "track.hpp"
#include <vector>

namespace tplay {

class Queue {
public:
    /// Replaces the queue contents with `tracks` and resets queue indices.
    /// `tracks` becomes the new ordered playlist.
    void setTracks(std::vector<Track> tracks);
    /// Randomizes queue order while preserving the current track.
    void shuffle();
    /// Restores the original library order while preserving the current track.
    void unshuffle();
    /// Moves the selection cursor by `delta` items within valid bounds.
    /// `delta` is positive for down and negative for up.
    void moveSelection(int delta);
    /// Sets the cursor to `index` when it is within queue bounds.
    /// `index` is a zero-based queue position.
    void select(std::size_t index);
    /// Promotes the currently selected item to the active playback position.
    void jumpToSelected();
    /// Advances to the next track and returns true on success.
    bool next();
    /// Moves to the previous track and returns true on success.
    bool previous();

    /// Returns the current queue snapshot.
    const std::vector<Track>& tracks() const noexcept { return tracks_; }
    /// Returns the active track, or null when the queue is empty.
    const Track* current() const noexcept;
    /// Returns the zero-based playback index.
    std::size_t currentIndex() const noexcept { return currentIndex_; }
    /// Returns the zero-based cursor index.
    std::size_t selectedIndex() const noexcept { return selectedIndex_; }
    /// Returns true when no tracks are loaded.
    bool empty() const noexcept { return tracks_.empty(); }
    /// Returns true when queue order is currently shuffled.
    bool shuffled() const noexcept { return shuffled_; }

private:
    std::vector<Track> original_;
    std::vector<Track> tracks_;
    std::size_t currentIndex_ = 0;
    std::size_t selectedIndex_ = 0;
    bool shuffled_ = false;
};

}
