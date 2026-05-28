#pragma once

#include "hql_config.hpp"
#include "feature_vector.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>
#include <span>

namespace sigint_sim::game_theory {
/*
 * Distributed cooperative Q‑learner with hysteretic learning rates.
 *
 * This agent implements **hysteretic Q‑learning** (Matignon et al., 2007),
 * designed for cooperative multi‑agent systems where the environment appears
 * non‑stationary from any single agent’s perspective.  The core idea:
 *   - **alpha** (larger) is used when the temporal‑difference (TD) error is
 *     positive — the agent “optimistically” reinforces actions that led to
 *     better‑than‑expected outcomes.
 *   - **beta** (smaller, typically α/5 … α/10) is used for negative TD errors —
 *     the agent is slow to penalise actions that may have failed because of
 *     another agent’s exploration rather than the action itself.
 * This simple asymmetry has been shown to improve convergence to optimal joint
 * policies in potential games and, empirically, in many partially observable
 * stochastic games.
 *
 * Architecture & Learning
 *   - Linear function approximation with a fixed 813‑dimensional feature vector
 *     (bias, buffer×TX, battery×TX, neighbour reliability, frequency‑power pairs).
 *   - ε‑greedy action selection; the argmax is computed lazily (no allocation).
 *   - Replacing eligibility traces (Q(λ)) with trace‑decay parameter λ.
 *   - Delayed rewards are handled via a replay buffer of recent transitions,
 *     indexed by packet ID (absolute indices, safe across deque insertions).
 *   - Global credit assignment: the originator of a packet receives the full
 *     reward when the packet reaches the sink (processSinkSummary); forwarding
 *     nodes receive a local proxy reward (processLocalAck, scaled by γ_loc).
 *
 * SARSA Fallback (Divergence Recovery)
 *   If any weight’s absolute value exceeds `max_weight`, the learner
 *   permanently switches to **SARSA(λ)** (on‑policy).  This eliminates the
 *   max‑operator that can cause off‑policy divergence in linear TD and ensures
 *   that the weights remain bounded.  The fallback is a safety measure; in
 *   normal operation the hysteretic update keeps weights stable.
 *
 * Thread Safety & Determinism
 *   All mutable state is private; every method that modifies weights is called
 *   from a single thread.  The internal RNG is seeded at construction and
 *   preserved across resets, guaranteeing reproducible experiments.
 *
 *The learner is completely independent of the simulation engine; it only
 *       sees Observation snapshots and returns Action values.  This allows
 *      unit‑testing it with deterministic reward sequences.
 */

class HystereticQLearner {
public:
    using Config = HystereticQLearnerConfig;

    struct Transition {
        int timestamp = 0;
        Observation obs;
        Action action;
        std::vector<std::uint32_t> packet_ids;  // packets sent in this action
        // filled later by observeNewState / setNextAction
        Observation next_obs;
        std::vector<int> valid_neighbors;       // neighbors valid at the time of next_obs
        Action next_action;                     // action actually taken at next state
    };

    explicit HystereticQLearner(Config config, std::uint64_t seed);
    ~HystereticQLearner() = default;

    // ----- Action selection -----
    // valid_neighbors : list of neighbor indices that can be chosen.
    // num_freq_bins   : typically 200
    // num_power_levels: typically 4
    [[nodiscard]] Action chooseAction(const Observation& obs,
                         std::span<const int> valid_neighbors,
                         int num_freq_bins, int num_power_levels);

    // ----- Transition storage -----
    // Call after action is executed, before the next step, to record the transition.
    void addTransition(const Observation& obs, const Action& action,
                       const std::vector<std::uint32_t>& packet_ids, int current_time);

    // ----- State notification -----
    // Call once per timestep to supply the new observation (s_t).
    // This completes the previous transition's next_obs and next_action.
    void observeNewState(const Observation& obs, int current_time);

    // After chooseAction returns the action for the next step, call this to
    // record it as next_action of the *previous* transition.
    void setNextActionForLastTransition(const Action& action);

    // ----- Reward delivery -----
    // Immediate local ACK: the packet with given ID was successfully forwarded.
    // weight = packet priority ω; the learner uses γ_loc to compute proxy reward.
    // For test simplicity, γ_loc can be fixed via setGammaLoc().
    void processLocalAck(std::uint32_t packet_id, double packet_weight, int current_time);

    // Sink summary: delivered packet IDs with their original weight.
    void processSinkSummary(std::span<const std::pair<std::uint32_t, double>> delivered,
                            int current_time);

    // ----- Parameter control -----
    void setMu(double new_mu);
    double getMu() const { return mu_; }
    void setGammaLoc(double gamma_loc) { gamma_loc_ = gamma_loc; }
    [[nodiscard]] double maxAbsoluteWeight() const;

    // ----- Testing / introspection -----
    double getQValue(const Observation& obs, const Action& action) const;
    const std::vector<double>& getWeights() const { return theta_; }

private:
    // Compute Q(s,a) = dot(theta, phi(s,a))
    [[nodiscard]] double computeQ(const Observation& obs, const Action& action) const;
    
    // Return (best_action, best_q) without any allocations.
    [[nodiscard]] std::pair<Action, double> greedyAction(
        const Observation& obs,
        const std::span<const int>& neighbors,
        int num_freq, int num_power) const;

    // TD update from a transition entry using either max (Q-learning) or SARSA target
    void applyTDUpdate(const Transition& trans, double reward, bool use_max);

    // Update eligibility traces: decay all, then increment active features for given state-action
    void updateTraces(const Observation& obs, const Action& action);

    Config config_;
    double mu_;
    double gamma_loc_ = 0.5;   // initial estimate; can be set externally
    bool use_sarsa_ = false;   // switched on when weights diverge

    std::vector<double> theta_;       // length kFeatureVectorSize (813)
    std::vector<double> eligibility_; // length kFeatureVectorSize (813)

    std::deque<Transition> replay_buffer_;

    // maps packet_id -> absolute index (never changes after assignment)
    std::unordered_map<std::uint32_t, std::size_t> packet_to_index_;
    std::size_t evicted_count_ = 0;   // number of transitions that have been popped from the front

    int step_counter_ = 0;     // used for epsilon decay
    std::mt19937 rng_;
    double epsilon_;

    // Store last chosen action and valid neighbors for completing next transition
    Action last_action_;
    std::vector<int> last_valid_neighbors_;
    bool last_action_set_ = false;
};

} // namespace