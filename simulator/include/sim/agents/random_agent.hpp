#pragma once
#include "../include/sim/agents/agent_interface.hpp"
#include "../include/sim/core/sim_config.hpp"
#include <random>

namespace sigint_sim {

class RandomAgent : public IAgent {
public:
    explicit RandomAgent(uint64_t seed);
    Action selectAction(const NodeState& my_state,
                        const std::vector<NodeState>& all_states,
                        const std::vector<LinkState>& links,
                        const EventLog& recent_events) override;
    std::string agentType() const override { return "Random"; }
private:
    std::mt19937 rng_;
};

} // namespace sigint_sim