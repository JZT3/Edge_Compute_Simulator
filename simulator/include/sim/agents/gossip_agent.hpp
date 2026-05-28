#pragma once
#include "sim/agents/agent_interface.hpp"
#include <random>
#include <span>

namespace sigint_sim {

class GossipAgent : public IAgent {
public:
    GossipAgent() = default;

    Action selectAction(const NodeState& my_state,
                        std::span<const NodeState> ,
                        std::span<const LinkState> links,
                        const EventLog& recent_events) override;

    std::string agentType() const override { return "Gossip"; }

    GossipAgent(uint64_t seed = 0);

private:
    bool has_token_ = false;
    std::mt19937 rng_;

};

} // namespace sigint_sim