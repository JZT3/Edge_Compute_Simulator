#pragma once

#include "../include/sim/agents/agent_interface.hpp"
#include "../include/sim/game_theory/hysteretic_q_learner.hpp"
#include <vector>
#include <memory>
#include <span>

namespace sigint_sim {

class HQLAgent : public IAgent {
public:
    using LearnerConfig = game_theory::HystereticQLearner::Config;

    HQLAgent(LearnerConfig config, std::uint64_t seed);
    ~HQLAgent() override = default;

    // -------- IAgent interface --------
 Action selectAction(const NodeState& my_state,
                     std::span<const NodeState> /*all_states*/,
                     std::span<const LinkState> links,
                     const EventLog& /*recent_events*/) override;

    std::string agentType() const override { return "HQL"; }
    bool        isRLAgent()  const override { return true; }

    // -------- Reward delivery (called by simulator) --------
    void processLocalAck(std::uint32_t packet_id, double weight,
                         int current_time) override;

    void processSinkSummary(
        const std::vector<std::pair<std::uint32_t, double>>& delivered,
        int current_time) override;

private:
    game_theory::HystereticQLearner learner_;
    int step_counter_ = 0;

    // cached during selectAction; used by fromLearnerAction
    std::vector<int> last_valid_neighbor_ids_;   // node IDs (not indices)

    game_theory::Observation buildObservation(
        const NodeState& my_state,
        std::span<const LinkState> links) const;

    Action fromLearnerAction(
        const game_theory::Action& learner_action,
        double freq_base, double freq_step) const;
};

} // namespace sigint_sim