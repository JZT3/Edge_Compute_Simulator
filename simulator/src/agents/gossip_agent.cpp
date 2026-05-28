#include "../include/sim/agents/gossip_agent.hpp"
#include <cassert>

namespace sigint_sim {

GossipAgent::GossipAgent(uint64_t seed) : rng_(seed) {}


Action GossipAgent::selectAction(const NodeState& my_state,
                                 std::span<const NodeState> /*all_states*/,
                                 std::span<const LinkState> links,
                                 const EventLog& /*recent_events*/) {
    // Token management: simplistic – assume a single token circulates.
    // For baseline simplicity, every node acts as if it holds the token
    // in a round‑robin fashion, or we can use a static counter.
    // We'll use a simple heuristic: transmit if buffer not empty and there
    // is at least one active outgoing link, choosing one randomly.
    if (my_state.buffer_size == 0) {
        return Action{};   // silent
    }

    // Collect active outgoing neighbours
    std::vector<int> targets;
    for (const auto& link : links) {
        if (link.from == my_state.id && link.active) {
            targets.push_back(static_cast<int>(link.to));
        }
    }
    if (targets.empty()) {return Action{};}   // silent

    std::uniform_int_distribution<size_t> dist(0, targets.size() - 1);
    int choice = targets[dist(rng_)];

    Action act;
    act.stay_silent = false;
    act.burst = Action::Burst{choice, 0, 30.0, {}};
    act.scan_params = RFParams{2.4e9, 10e6, 40.0, 20e6};
    return act;

    // Random neighbour
    // int choice = targets[std::rand() % targets.size()];
}

} // namespace sigint_sim