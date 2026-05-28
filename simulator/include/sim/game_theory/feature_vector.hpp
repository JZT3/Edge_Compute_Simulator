#pragma once
#include <vector>
#include <cstdint>

namespace sigint_sim::game_theory {

struct Observation {
    int buffer_level = 0;       // 0..3
    int battery_level = 0;      // 0..3
    std::uint8_t neighbor_bits = 0; // bits 0-3: reliability flags
};

struct Action {
    bool is_silent = true;
    int  neighbor_idx = -1;     // index in valid_neighbors, -1 if silent
    int  freq_bin = 0;          // 0..199
    int  power_level = 0;       // 0..3
};

// Returns the indices of all active features for the given (obs, action).
// The feature vector size is fixed at 813.
std::vector<int> activeFeatureIndices(const Observation& obs, const Action& action);

constexpr int kFeatureVectorSize = 813;

} // namespace