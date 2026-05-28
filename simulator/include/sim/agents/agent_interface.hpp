#pragma once
#include "../core/types.hpp"
#include <vector>
#include <span>

namespace sigint_sim {

class IAgent {
public:
    virtual ~IAgent() = default;
    // Select action based on my state and the state of the world as observed.
    // The agent receives copies, not references.
    virtual Action selectAction(const NodeState& my_state,
                                std::span<const NodeState> all_states,
                                std::span<const LinkState> links,
                                const EventLog& recent_events) = 0;
    virtual std::string agentType() const { return "IAgent"; } // This provides a default implementation that the linker can resolve. 
                                                               // The trampoline can still override it in Python.
    
                                                               // Hooks for reinforcement‑learning agents (empty by default)
    virtual void processLocalAck(std::uint32_t packet_id, double weight,
                                 int current_time) {}
    virtual void processSinkSummary(const std::vector<std::pair<std::uint32_t, double>>& delivered,
                                    int current_time) {}
                                                            
    virtual bool isRLAgent() const { return false; }
    
    };

} // namespace sigint_sim