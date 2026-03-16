#include "queue.hpp"
#include <algorithm>
#include <random>

namespace tplay {

/// Replaces the queue contents with `tracks` and resets queue indices.
void Queue::setTracks(std::vector<Track> tracks)
{
    original_ = tracks;
    tracks_ = std::move(tracks);
    currentIndex_ = 0;
    selectedIndex_ = 0;
    shuffled_ = false;
}

/// Shuffles the queue while preserving the current track.
void Queue::shuffle() {
    // if (tracks_.empty() || shuffled_)
    // {
    //     return;
    // }

    const Track currentTrack = tracks_[currentIndex_];
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(tracks_.begin(), tracks_.end(), rng);

    auto it = std::find_if(tracks_.begin(), tracks_.end(), [&](const Track& track) {
        return track.path == currentTrack.path;
    });
    currentIndex_ = static_cast<std::size_t>(std::distance(tracks_.begin(), it));
    selectedIndex_ = currentIndex_;
    shuffled_ = true;
}

/// Restores original ordering while preserving the current track.
void Queue::unshuffle()
{
    if (!shuffled_)
    {
        return;
    }

    const Track currentTrack = tracks_[currentIndex_];
    tracks_ = original_;
    auto it = std::find_if(tracks_.begin(), tracks_.end(), [&](const Track& track) {
        return track.path == currentTrack.path;
    });
    currentIndex_ = static_cast<std::size_t>(std::distance(tracks_.begin(), it));
    selectedIndex_ = currentIndex_;
    shuffled_ = false;
}

/// Moves the selection cursor by `delta` items within queue bounds.
void Queue::moveSelection(int delta)
{
    if (tracks_.empty()) {
        return;
    }

    const int size = static_cast<int>(tracks_.size());
    int next = static_cast<int>(selectedIndex_) + delta;
    if (next < 0) {
        next = 0;
    }
    if (next >= size) {
        next = size - 1;
    }
    selectedIndex_ = static_cast<std::size_t>(next);
}

/// Sets the selection cursor to `index` when it exists.
void Queue::select(std::size_t index)
{
    if (index < tracks_.size())
    {
        selectedIndex_ = index;
    }
}

/// Promotes the selected track to the current playback position.
void Queue::jumpToSelected()
{
    if (!tracks_.empty()) {
        currentIndex_ = selectedIndex_;
    }
}

/// Advances to the next track when one exists.
bool Queue::next()
{
    if (tracks_.empty() || currentIndex_ + 1 >= tracks_.size())
    {
        return false;
    }
    ++currentIndex_;
    selectedIndex_ = currentIndex_;
    return true;
}

/// Moves to the previous track when one exists.
bool Queue::previous()
{
    if (tracks_.empty() || currentIndex_ == 0)
    {
        return false;
    }
    --currentIndex_;
    selectedIndex_ = currentIndex_;
    return true;
}

/// Returns the current playing track, or null for an empty queue.
const Track* Queue::current() const noexcept
{
    if (tracks_.empty())
    {
        return nullptr;
    }
    return &tracks_[currentIndex_];
}

}
