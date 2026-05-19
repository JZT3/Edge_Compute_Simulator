#include "../include/sim/agents/random_agent.hpp"
#include <random>
#include <algorithm>

namespace sigint_sim {

RandomAgent::RandomAgent(uint64_t seed) : rng_(seed) {}

Action RandomAgent::selectAction(const NodeState& my_state,
                                 const std::vector<NodeState>& /*all_states*/,
                                 const std::vector<LinkState>& links,
                                 const EventLog& /*recent_events*/) {
    Action act;
    std::uniform_int_distribution<int> mode_dist(0, 3); // IDLE, SCAN, PROCESS, TRANSMIT
    int mode = mode_dist(rng_);

    switch (mode) {
        case 1: { // SCAN
            RFParams scan;
            scan.center_freq = 2.4e9;
            scan.bandwidth = 20e6;
            scan.gain = 40.0;
            scan.sample_rate = 40e6;
            act.scan_params = scan;
            break;
        }
        case 2: { // PROCESS
            if (my_state.buffer_size > 0) {
                act.process_task_ids.push_back(0); // arbitrary signal id
            }
            break;
        }
        case 3: { // TRANSMIT
            if (!links.empty()) {
                std::vector<const LinkState*> outgoing;
                for (const auto& l : links) {
                    if (l.from == my_state.id) outgoing.push_back(&l);
                }
                if (!outgoing.empty()) {
                    std::uniform_int_distribution<size_t> link_dist(0, outgoing.size()-1);
                    const auto* chosen = outgoing[link_dist(rng_)];
                    Action::Burst burst;
                    burst.target_node_id = static_cast<int>(chosen->to);
                    burst.phy_mode = 0;
                    burst.power = 20.0;
                    act.burst = burst;
                }
            }
            break;
        }
        default: break; // IDLE
    }
    act.stay_silent = act.is_silent();
    return act;
}

} // namespace sigint_sim