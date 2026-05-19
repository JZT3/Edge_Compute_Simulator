#pragma once
#include "types.hpp"
#include "node.hpp"
#include "link.hpp"
#include "channel_model.hpp"
#include "../include/sim/agents/agent_interface.hpp"
#include "event.hpp"
#include <memory>
#include <vector>
#include <random>
#include <unordered_map>

namespace sigint_sim {

class Simulator {
public:
    struct Config {
        uint64_t seed;
        double timestep = 0.1;
        double duration = 10.0;
        std::vector<std::pair<NodeId,NodeId>> topology_edges; // directed
        std::shared_ptr<ChannelModel> channel;
    };

    explicit Simulator(Config config);
    void step();
    void reset(uint64_t new_seed);

    // Attach an agent to a node (must be called before stepping).
    void setAgent(NodeId id, std::unique_ptr<IAgent> agent);

    [[nodiscard]] std::vector<NodeState> getNodeStates() const;
    [[nodiscard]] std::vector<LinkState> getLinkStates() const;
    [[nodiscard]] const EventLog& getEventLog() const;

    [[nodiscard]] TimePoint currentTime() const noexcept { return current_time_; }
    [[nodiscard]] bool isFinished() const noexcept { return current_time_ >= config_.duration; }

private:
    Config config_;
    std::vector<std::unique_ptr<SDRNode>> nodes_;
    std::vector<std::unique_ptr<Link>> links_;
    std::shared_ptr<ChannelModel> channel_;
    std::mt19937 rng_;
    TimePoint current_time_ = 0.0;
    EventLog event_log_;

    void logEvent(Event e);

    // Agent lookup
    std::unordered_map<int, std::unique_ptr<IAgent>> agents_; // key = node id as int
};

} // namespace sigint_sim