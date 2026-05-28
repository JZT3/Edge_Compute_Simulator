#include "../include/sim/agents/random_agent.hpp"
#include "../include/sim/core/sim_config.hpp"
#include <random>
#include <algorithm>

namespace sigint_sim {

RandomAgent::RandomAgent(uint64_t seed) : rng_(seed) {}

Action RandomAgent::selectAction(const NodeState& my_state,
                                 std::span<const NodeState>,
                                 std::span<const LinkState> links,
                                 const EventLog& /*recent_events*/) {
    // Weighted action distribution – idempotent, safe against zero sum
    std::array<double, 4> weights = {
        RA_PROB_IDLE, RA_PROB_SCAN, RA_PROB_PROCESS, RA_PROB_TRANSMIT
    };
    double sum = weights[0] + weights[1] + weights[2] + weights[3];
    if (sum <= 0.0) {
        // fallback to uniform distribution
        weights = {0.25, 0.25, 0.25, 0.25};
        sum = 1.0;
    }
    std::discrete_distribution<int> mode_dist(weights.begin(), weights.end());
    int mode = mode_dist(rng_);   // 0=IDLE, 1=SCAN, 2=PROCESS, 3=TX

    Action act;
    switch (mode) {
        case 0: break; // IDLE
        case 1: { // SCAN
            std::uniform_real_distribution<double> freq_dist(
                my_state.min_freq_hz, my_state.max_freq_hz);
            RFParams scan;
            scan.center_freq = freq_dist(rng_);
            scan.bandwidth    = AGENT_SCAN_BW_HZ;
            scan.gain         = AGENT_SCAN_GAIN_DB;
            scan.sample_rate  = AGENT_SCAN_SAMPLE_RATE_HZ;
            act.scan_params = scan;
            break;
        }
        case 2: { // PROCESS
            if (my_state.buffer_size > 0)
                act.process_task_ids.push_back(0);
            break;
        }
        case 3: { // TRANSMIT
            if (!links.empty()) {
                std::vector<const LinkState*> outgoing;
                for (const auto& l : links)
                    if (l.from == my_state.id) outgoing.push_back(&l);
                if (!outgoing.empty()) {
                    std::uniform_int_distribution<size_t> link_dist(0, outgoing.size()-1);
                    const auto* chosen = outgoing[link_dist(rng_)];
                    Action::Burst burst;
                    burst.target_node_id = static_cast<int>(chosen->to);
                    burst.phy_mode = 0;
                    burst.power = AGENT_TX_POWER_DBM;
                    act.burst = burst;
                }
            }
            break;
        }
    }
    act.stay_silent = act.is_silent();
    return act;
}
} //namespace sigint_sim