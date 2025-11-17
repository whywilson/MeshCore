#pragma once

#include <cstddef>

namespace canned {

constexpr size_t kMaxMessages = 8;
constexpr size_t kMaxMessageLen = 64;

inline constexpr const char* kDefaultMessages[kMaxMessages] = {
    "All good",
    "Stop and wait",
    "Need assistance",
    "Emergency, send help",
    "Found water",
    "Returning to camp",
    "Running behind",
    "Found the point"};

inline constexpr size_t kDefaultMessageCount = kMaxMessages;

} // namespace canned
