#pragma once

#include "hql_config.hpp"
#include "feature_vector.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

namespace sigint_sim::game_theory {

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
    Action chooseAction(const Observation& obs,
                        const std::vector<int>& valid_neighbors,
                        int num_freq_bins,
                        int num_power_levels);

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
    void processSinkSummary(const std::vector<std::pair<std::uint32_t, double>>& delivered,
                            int current_time);

    // ----- Parameter control -----
    void setMu(double new_mu);
    double getMu() const { return mu_; }
    void setGammaLoc(double gamma_loc) { gamma_loc_ = gamma_loc; }
    double maxAbsoluteWeight() const;

    // ----- Testing / introspection -----
    double getQValue(const Observation& obs, const Action& action) const;
    const std::vector<double>& getWeights() const { return theta_; }

private:
    // Compute Q(s,a) = dot(theta, phi(s,a))
    double computeQ(const Observation& obs, const Action& action) const;
    
    // Build list of valid actions (including silent)
    std::vector<Action> buildActionList(const std::vector<int>& valid_neighbors,
                                        int num_freq_bins, int num_power_levels) const;

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

    // maps packet_id -> iterator into replay_buffer_
    std::unordered_map<std::uint32_t, std::deque<Transition>::iterator> packet_to_trans_;

    int step_counter_ = 0;     // used for epsilon decay
    std::mt19937 rng_;
    double epsilon_;

    // Store last chosen action and valid neighbors for completing next transition
    Action last_action_;
    std::vector<int> last_valid_neighbors_;
    bool last_action_set_ = false;
};

} // namespace