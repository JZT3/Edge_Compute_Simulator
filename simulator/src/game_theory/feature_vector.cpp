#include "../include/sim/game_theory/feature_vector.hpp"
#include <cassert>

namespace sigint_sim::game_theory {

std::vector<int> activeFeatureIndices(const Observation& obs, const Action& action) {
    std::vector<int> idx;
    idx.reserve(4);  // at most 4 active features: bias, buf, bat, rel, freq/power

    // Bias (index 0) is always active
    idx.push_back(0);

    if (action.is_silent) {
        return idx;  // only bias
    }

    // Transmission features
    // buf_tx
    assert(obs.buffer_level >= 0 && obs.buffer_level < 4);
    idx.push_back(1 + obs.buffer_level);

    // bat_tx
    assert(obs.battery_level >= 0 && obs.battery_level < 4);
    idx.push_back(5 + obs.battery_level);

    // rel_nhop (only if neighbour is tracked and reliable)
    if (action.neighbor_idx >= 0 && action.neighbor_idx < 4) {
        if (obs.neighbor_bits & (1u << action.neighbor_idx)) {
            idx.push_back(9 + action.neighbor_idx);
        }
    }

    // freq_pwr
    assert(action.freq_bin >= 0 && action.freq_bin < 200);
    assert(action.power_level >= 0 && action.power_level < 4);
    int fp_idx = 13 + action.freq_bin * 4 + action.power_level;
    idx.push_back(fp_idx);

    return idx;
}

} // namespace