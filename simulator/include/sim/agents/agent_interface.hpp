#pragma once
#include "../core/types.hpp"
#include <vector>

namespace sigint_sim {

class IAgent {
public:
    virtual ~IAgent() = default;
    // Select action based on my state and the state of the world as observed.
    // The agent receives copies, not references.
    virtual Action selectAction(const NodeState& my_state,
                                const std::vector<NodeState>& all_states,
                                const std::vector<LinkState>& links,
                                const EventLog& recent_events) = 0;
    virtual std::string agentType() const { return "IAgent"; } // This provides a default implementation that the linker can resolve. 
                                                               // The trampoline can still override it in Python.
};

} // namespace sigint_sim