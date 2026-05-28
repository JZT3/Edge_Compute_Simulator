#pragma once
#include "sim/agents/agent_interface.hpp"
#include <random>


namespace sigint_sim {

class DTNAgent : public IAgent {
public:
    DTNAgent() = default;

    Action selectAction(const NodeState& my_state,
                        std::span<const NodeState> /* all states*/,
                        std::span<const LinkState> links,
                        const EventLog& recent_events) override;

    std::string agentType() const override { return "DTN"; }

    DTNAgent(uint64_t seed = 0);

private:
  std::mt19937 rng_;

};

} // namespace sigint_sim