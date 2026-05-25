#include "../include/sim/game_theory/hysteretic_q_learner.hpp"
#include <cassert>
#include <cmath>
#include <algorithm>
#include <limits>

namespace sigint_sim::game_theory {

HystereticQLearner::HystereticQLearner(Config config, std::uint64_t seed)
    : config_(std::move(config)), mu_(config_.mu), gamma_loc_(0.5),
      theta_(kFeatureVectorSize, 0.0),
      eligibility_(kFeatureVectorSize, 0.0),
      rng_(seed), epsilon_(config_.epsilon_start)
{
    // replay_buffer_.clear();
    // packet_to_trans_.clear();
    // step_counter_ = 0;
}

// ----------------------------------------------------------------------
// Q-value computation
// ----------------------------------------------------------------------
double HystereticQLearner::computeQ(const Observation& obs, const Action& action) const {
    auto active = activeFeatureIndices(obs, action);
    double sum = 0.0;
    for (int idx : active) sum += theta_[idx];
    return sum;
}

double HystereticQLearner::getQValue(const Observation& obs, const Action& action) const {
    return computeQ(obs, action);
}

// ----------------------------------------------------------------------
// Action list generation
// ----------------------------------------------------------------------
std::vector<Action> HystereticQLearner::buildActionList(
    const std::vector<int>& valid_neighbors,
    int num_freq_bins, int num_power_levels) const
{
    std::vector<Action> actions;
    // silent action
    actions.push_back(Action{});
    // all combinations of neighbor, freq, power
    for (int n : valid_neighbors) {
        for (int f = 0; f < num_freq_bins; ++f) {
            for (int p = 0; p < num_power_levels; ++p) {
                actions.push_back({false, n, f, p});
            }
        }
    }
    return actions;
}

// ----------------------------------------------------------------------
// Action selection (ε‑greedy)
// ----------------------------------------------------------------------
Action HystereticQLearner::chooseAction(const Observation& obs,
                                        const std::vector<int>& valid_neighbors,
                                        int num_freq_bins, int num_power_levels)
    {
    auto actions = buildActionList(valid_neighbors, num_freq_bins, num_power_levels);
    assert(!actions.empty());

    // Compute Q for all actions
    std::vector<double> q_vals(actions.size());
    for (size_t i = 0; i < actions.size(); ++i) {
        q_vals[i] = computeQ(obs, actions[i]);
    }

    // ε‑greedy
    size_t chosen_idx;
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    if (uni(rng_) < epsilon_) {
        std::uniform_int_distribution<size_t> dist(0, actions.size()-1);
        chosen_idx = dist(rng_);
    } else {
        chosen_idx = static_cast<size_t>(
            std::distance(q_vals.begin(), std::max_element(q_vals.begin(), q_vals.end())));
    }

    // Decay Epsilon
    if (step_counter_ < config_.epsilon_decay_steps) {
        double progress = static_cast<double>(step_counter_) / config_.epsilon_decay_steps;
        epsilon_ = config_.epsilon_start + (config_.epsilon_end - config_.epsilon_start) * progress;
    }
    ++step_counter_;

    // Store for completing transition later
    last_action_ = actions[chosen_idx];
    last_valid_neighbors_ = valid_neighbors;  // store for next transition's next_obs
    last_action_set_ = true;

    updateTraces(obs, last_action_);

    return last_action_;
    }

// ----------------------------------------------------------------------
// Transition storage & completion
// ----------------------------------------------------------------------
    void HystereticQLearner::addTransition(const Observation& obs, const Action& action,
                                        const std::vector<std::uint32_t>& packet_ids,
                                        int current_time)
    {
        Transition trans;
        trans.timestamp = current_time;
        trans.obs = obs;
        trans.action = action;
        trans.packet_ids = packet_ids;

        replay_buffer_.push_back(std::move(trans));
        while (replay_buffer_.size() > static_cast<size_t>(config_.trace_length)) {
            for (auto pid : replay_buffer_.front().packet_ids)
                packet_to_trans_.erase(pid);
            replay_buffer_.pop_front();
        }

        auto it = std::prev(replay_buffer_.end());
        for (auto pid : packet_ids)
            packet_to_trans_[pid] = it;

        last_action_set_ = false;
    }

// ----------------------------------------------------------------------
// Observe new state – complete previous transition
// ----------------------------------------------------------------------
    void HystereticQLearner::observeNewState(const Observation& obs, int /*current_time*/) {
        // Complete the most recent transition that hasn't had next_obs set

        if (replay_buffer_.empty()) return;
        auto& trans = replay_buffer_.back();
        trans.next_obs = obs;
        trans.valid_neighbors = last_valid_neighbors_;
    }

    void HystereticQLearner::setNextActionForLastTransition(const Action& action) {
    if (replay_buffer_.empty()) return;
    replay_buffer_.back().next_action = action;
    }




// ----------------------------------------------------------------------
// Reward processing
// ----------------------------------------------------------------------
    void HystereticQLearner::processLocalAck(std::uint32_t packet_id, double packet_weight,
                                            int current_time) {
        auto it = packet_to_trans_.find(packet_id);
        if (it == packet_to_trans_.end()) return;

        auto trans_it = it->second;
        // Compute proxy reward
        double reward = packet_weight * gamma_loc_;
        applyTDUpdate(*trans_it, reward, !use_sarsa_);  // use max if not SARSA
    }

    void HystereticQLearner::processSinkSummary(
        const std::vector<std::pair<std::uint32_t, double>>& delivered, int current_time)
    {
        for (const auto& [pkt_id, weight] : delivered) {
            auto it = packet_to_trans_.find(pkt_id);
            if (it == packet_to_trans_.end()) continue;
            auto trans_it = it->second;
            double reward = weight;  // full weight for originator
            applyTDUpdate(*trans_it, reward, !use_sarsa_);
        }
    }


// ----------------------------------------------------------------------
// Internal TD update
// ----------------------------------------------------------------------
    void HystereticQLearner::applyTDUpdate(const Transition& trans, double reward, bool use_max) {
        // Compute target
        double target;
        if (trans.valid_neighbors.empty() || trans.next_obs.buffer_level < 0) {
            // If next state not set, treat as terminal (no future reward)
            target = reward;
        } else {
            // Compute max Q or Q of next_action
            double next_q;
            if (use_max) {
                // max over all valid actions from next state
                // Build action list using stored valid_neighbors and full freq/power (200,4)
                auto actions = buildActionList(trans.valid_neighbors, 200, 4);
                double max_q = -std::numeric_limits<double>::infinity();
                for (const auto& a : actions) {
                    double q = computeQ(trans.next_obs, a);
                    if (q > max_q) max_q = q;
                }
                next_q = max_q;
            } else {
                // SARSA: use actual next action taken
                if (trans.next_action.is_silent && trans.next_action.neighbor_idx < 0) {
                    // if next action not set, fallback to max? but should be set.
                    // For safety, use 0.
                    next_q = 0.0;
                } else {
                    next_q = computeQ(trans.next_obs, trans.next_action);
                }
            }
            target = reward + config_.gamma * next_q;
        }

        double current_q = computeQ(trans.obs, trans.action);
        double td_error = target - current_q;
        double alpha = (td_error > 0.0) ? config_.alpha : config_.beta;

        // Update weights and eligibility traces
        auto active = activeFeatureIndices(trans.obs, trans.action);
        for (int idx : active) {
            theta_[idx] += alpha * td_error * eligibility_[idx];
        }

        // Check divergence
        if (!use_sarsa_ && maxAbsoluteWeight() > config_.max_weight) {
            use_sarsa_ = true;
        }
    }

// ----------------------------------------------------------------------
// Eligibility trace management
// ----------------------------------------------------------------------
    void HystereticQLearner::updateTraces(const Observation& obs, const Action& action) {
        // decay all traces
        double decay = config_.lambda * config_.gamma;
        for (auto& v : eligibility_) v *= decay;

        // increment active features
        auto active = activeFeatureIndices(obs, action);
        for (int idx : active) eligibility_[idx] += 1.0;
    }

// ----------------------------------------------------------------------
// Utilities 
// ----------------------------------------------------------------------
    void HystereticQLearner::setMu(double new_mu) { mu_ = new_mu; }

    double HystereticQLearner::maxAbsoluteWeight() const {
        double max_val = 0.0;
        for (double v : theta_) max_val = std::max(max_val, std::abs(v));
        return max_val;
    }

} // namespace