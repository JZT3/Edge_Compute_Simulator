// src/agents/hql_agent.cpp
#include "../include/sim/agents/hql_agent.hpp"
#include <cassert>
#include <algorithm>

namespace sigint_sim {

HQLAgent::HQLAgent(LearnerConfig config, std::uint64_t seed)
    : learner_(std::move(config), seed) {}

// ---------------------------------------------------------------------------
// Observation
// ---------------------------------------------------------------------------
game_theory::Observation HQLAgent::buildObservation(
    const NodeState& my_state, const std::vector<LinkState>& links) const
{
    game_theory::Observation obs;
    obs.buffer_level = std::min(3, my_state.buffer_size);
    obs.battery_level = 2;   // placeholder until energy model is added
    obs.neighbor_bits = 0;
    for (size_t i = 0; i < links.size() && i < 4; ++i) {
        if (links[i].active) obs.neighbor_bits |= (1u << i);
    }
    return obs;
}

// ---------------------------------------------------------------------------
// Action selection
// ---------------------------------------------------------------------------
Action HQLAgent::selectAction(const NodeState& my_state,
                              const std::vector<NodeState>& /*all_states*/,
                              const std::vector<LinkState>& links,
                              const EventLog& /*recent_events*/)
{
    // 1. Build observation from current state
    auto obs = buildObservation(my_state, links);

    // 2. Notify learner of the new state (this completes the previous
    //    transition's next_obs and next_action using the action chosen
    //    in the *previous* step – that linkage is handled inside the learner).
    learner_.observeNewState(obs, step_counter_);

    // 3. Determine valid neighbours (outgoing active links)
    last_valid_neighbor_ids_.clear();
    std::vector<int> neighbor_indices;      // indices 0..K-1 for the learner
    for (const auto& link : links) {
        if (link.from == my_state.id && link.active) {
            neighbor_indices.push_back(
                static_cast<int>(last_valid_neighbor_ids_.size()));
                last_valid_neighbor_ids_.push_back(static_cast<int>(link.to)); 
            }
    }

    // 4. If no neighbours, force silent action
    if (neighbor_indices.empty()) {
        game_theory::Action silent_learner{};
        learner_.addTransition(obs, silent_learner, {}, step_counter_);
        ++step_counter_;
        return Action{};   // simulator silent action
    }

    // 5. Ask the learner for an action
    constexpr int num_freq = 200;
    constexpr int num_power = 4;
    auto learner_action = learner_.chooseAction(obs, neighbor_indices,
                                                num_freq, num_power);

    // 6. Record the transition (packet IDs will be filled later when the
    //    simulator calls processLocalAck).
    learner_.addTransition(obs, learner_action, {}, step_counter_);
    ++step_counter_;

    // 7. Convert to simulator action
    return fromLearnerAction(learner_action);
}

// ---------------------------------------------------------------------------
// Action conversion
// ---------------------------------------------------------------------------
Action HQLAgent::fromLearnerAction(const game_theory::Action& learner_action) const {
    Action sim_action{};
    if (learner_action.is_silent) {
        return sim_action;
    }

    sim_action.stay_silent = false;
    int n_idx = learner_action.neighbor_idx;
    assert(n_idx >= 0 && n_idx < static_cast<int>(last_valid_neighbor_ids_.size()));
    int target_node = last_valid_neighbor_ids_[n_idx];

    // Build burst
    sim_action.burst = Action::Burst{};
    sim_action.burst->target_node_id = target_node;
    sim_action.burst->phy_mode = 0;       // default MCS

    constexpr double power_table[] = {0.0, 10.0, 20.0, 30.0};
    sim_action.burst->power = power_table[learner_action.power_level];

    // Frequency bin → centre frequency
    sim_action.scan_params = RFParams{};
    sim_action.scan_params->center_freq =
        2.4e9 + learner_action.freq_bin * 100e3;
    sim_action.scan_params->bandwidth = 10e6;
    sim_action.scan_params->gain = 40.0;

    return sim_action;
}

// ---------------------------------------------------------------------------
// Reward delivery
// ---------------------------------------------------------------------------
void HQLAgent::processLocalAck(std::uint32_t packet_id, double weight,
                               int current_time) {
    learner_.processLocalAck(packet_id, weight, current_time);
}

void HQLAgent::processSinkSummary(
    const std::vector<std::pair<std::uint32_t, double>>& delivered,
    int current_time) {
    learner_.processSinkSummary(delivered, current_time);
}

} // namespace sigint_sim