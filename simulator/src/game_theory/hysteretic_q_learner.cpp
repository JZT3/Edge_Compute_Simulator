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
// Action selection (ε‑greedy)
// ----------------------------------------------------------------------

std::pair<Action, double> HystereticQLearner::greedyAction(
    const Observation& obs,
    const std::span<const int>& neighbors,
    int num_freq, int num_power) const
{
    Action best{};            // silent action
    double best_q = computeQ(obs, best);

    for (int n : neighbors) {
        for (int f = 0; f < num_freq; ++f) {
            for (int p = 0; p < num_power; ++p) {
                Action a{false, n, f, p};
                double q = computeQ(obs, a);
                if (q > best_q) {
                    best_q = q;
                    best = a;
                }
            }
        }
    }
    return {best, best_q};
}


Action HystereticQLearner::chooseAction(const Observation& obs,
                                        std::span<const int> valid_neighbors,
                                        int num_freq_bins, int num_power_levels) {
    // Decay epsilon (existing logic)
    if (step_counter_ < config_.epsilon_decay_steps) {
        double progress = static_cast<double>(step_counter_) / config_.epsilon_decay_steps;
        epsilon_ = config_.epsilon_start + (config_.epsilon_end - config_.epsilon_start) * progress;
    }
    ++step_counter_;

    // Update eligibility traces for the *previous* action (decay then increment)
    // This is called here because chooseAction is the start of a new step.
    // The traces were already updated at the end of the previous step; now we just decay?
    // Actually, the TRG says: after choosing action, increment its trace.
    // We'll do that later.

    // ε‑greedy selection without building an action vector
    const int total_actions = 1 + static_cast<int>(valid_neighbors.size())
                                * num_freq_bins * num_power_levels;

    std::uniform_real_distribution<double> uni(0.0, 1.0);
    Action chosen;

    if (uni(rng_) < epsilon_) {
        // Random action
        std::uniform_int_distribution<int> dist(0, total_actions - 1);
        int idx = dist(rng_);
        if (idx == 0) {
            chosen = Action{};   // silent
        } else {
            idx--;   // skip silent
            int n_idx  = idx / (num_freq_bins * num_power_levels);
            int rest   = idx % (num_freq_bins * num_power_levels);
            int f      = rest / num_power_levels;
            int p      = rest % num_power_levels;
            chosen = Action{false, valid_neighbors[n_idx], f, p};
        }
    } else {
        // Greedy action
        auto [best, best_q] = greedyAction(obs, valid_neighbors,
                                           num_freq_bins, num_power_levels);
        chosen = best;
    }

    // Update eligibility traces for the chosen action
    updateTraces(obs, chosen);

    last_action_ = chosen;
    last_valid_neighbors_.assign(valid_neighbors.begin(), valid_neighbors.end());
    last_action_set_ = true;
    return chosen;
}

// ----------------------------------------------------------------------
// Transition storage & completion
// ----------------------------------------------------------------------
    void HystereticQLearner::addTransition(const Observation& obs, const Action& action,
                                           const std::vector<std::uint32_t>& packet_ids,
                                           int current_time) {
        Transition trans{
            .timestamp   = current_time,
            .obs         = obs,
            .action      = action,
            .packet_ids  = packet_ids,
        };

        replay_buffer_.push_back(std::move(trans));

        // Compute the absolute index of the newly added transition
        std::size_t new_index = evicted_count_ + replay_buffer_.size() - 1;

        // Register each packet ID with this absolute index
        for (auto pid : packet_ids) {
            packet_to_index_[pid] = new_index;
        }

        // Enforce capacity
        while (replay_buffer_.size() > static_cast<std::size_t>(config_.trace_length)) {
            // Remove the mapping for all packet IDs in the oldest transition
            for (auto pid : replay_buffer_.front().packet_ids) {
                // Only erase if the stored index still points to this evicted entry
                auto it = packet_to_index_.find(pid);
                if (it != packet_to_index_.end() && it->second == evicted_count_) {
                    packet_to_index_.erase(it);
                }
            }
            replay_buffer_.pop_front();
            ++evicted_count_;
        }
    }

// ----------------------------------------------------------------------
// Observe new state – complete previous transition
// ----------------------------------------------------------------------
    void HystereticQLearner::observeNewState(const Observation& obs, int /*current_time*/) {
        // The caller must have already called addTransition for the previous step,
        // so replay_buffer_ is non‑empty and its back() is the transition being completed.
        // On the very first call (step 0) the buffer is empty → no‑op.

        if (replay_buffer_.empty()) return;
        auto& trans = replay_buffer_.back();
        // Guard against double‑completion (should never happen)
        assert(trans.next_obs.buffer_level < 0);   // still unset

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
    void HystereticQLearner::processLocalAck(std::uint32_t packet_id,
                                             double packet_weight, int current_time) {
        auto it = packet_to_index_.find(packet_id);
        if (it == packet_to_index_.end()) return;

        std::size_t abs_idx = it->second;
        // Ignore if the transition has already been evicted
        if (abs_idx < evicted_count_) return;

        std::size_t rel_idx = abs_idx - evicted_count_;
        assert(rel_idx < replay_buffer_.size());
        double reward = packet_weight * gamma_loc_;
        applyTDUpdate(replay_buffer_[rel_idx], reward, !use_sarsa_);
    }

void HystereticQLearner::processSinkSummary(
    std::span<const std::pair<std::uint32_t, double>> delivered,
    int current_time) {
    for (const auto& [pid, weight] : delivered) {
        for (const auto& [pid, weight] : delivered) {
            auto it = packet_to_index_.find(pid);
            if (it == packet_to_index_.end()) continue;
            std::size_t abs_idx = it->second;
            if (abs_idx < evicted_count_) continue;
            std::size_t rel_idx = abs_idx - evicted_count_;
            assert(rel_idx < replay_buffer_.size());
            applyTDUpdate(replay_buffer_[rel_idx], weight, !use_sarsa_);
        }
    }
}


// ----------------------------------------------------------------------
// Internal TD update
// ----------------------------------------------------------------------
    void HystereticQLearner::applyTDUpdate(const Transition& trans, double reward, bool use_max) {
        double target;
        if (trans.valid_neighbors.empty() || trans.next_obs.buffer_level < 0) {
            target = reward;                     // terminal or incomplete next state
        } else {
            double next_q;
            if (use_max) {
                // Lazy scan – no allocation
                auto [_, max_q] = greedyAction(trans.next_obs, trans.valid_neighbors, 200, 4);
                next_q = max_q;
            } else {
                // SARSA: use actual next action taken
                if (trans.next_action.is_silent && trans.next_action.neighbor_idx < 0) {
                    next_q = 0.0;
                } else {
                    next_q = computeQ(trans.next_obs, trans.next_action);
                }
            }
            target = reward + config_.gamma * next_q;
        }

        double current_q = computeQ(trans.obs, trans.action);
        double td_error = target - current_q;

        // The asymmetry is the whole point: overestimate joint Q-values by
        // discounting negative experiences. This reduces oscillation in cooperative MARL
        // where another agent's policy change looks like environment noise.
        double alpha = (td_error > 0.0) ? config_.alpha : config_.beta;

        auto active = activeFeatureIndices(trans.obs, trans.action);
        for (int idx : active) {
            theta_[idx] += alpha * td_error * eligibility_[idx];
        }

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