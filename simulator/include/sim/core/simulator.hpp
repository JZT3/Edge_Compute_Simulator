#pragma once
#include "types.hpp"
#include "node.hpp"
#include "link.hpp"
#include "channel_model.hpp"
#include "../include/sim/agents/agent_interface.hpp"
#include "metrics.hpp"
#include <memory>
#include <vector>
#include <random>
#include <unordered_map>

namespace sigint_sim {

// Forward declaration
class SampleProcessingChannel;

class Simulator {
public:
    struct Config {
        uint64_t seed;
        double timestep = 0.1;
        double duration = 10.0;
        std::vector<std::pair<NodeId, NodeId>> topology_edges;
        std::shared_ptr<ChannelModel> channel;

        // Optional per‑node data for the new PHY
        std::unordered_map<int, HardwareProfile> node_profiles;
        std::unordered_map<int, std::pair<double, double>> node_positions;
    };

    explicit Simulator(Config config);
    void step();
    void reset(uint64_t new_seed);

    void setAgent(NodeId id, std::shared_ptr<IAgent> agent); // works better with pybind11 
                                                             // and the trampoline class

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
    std::unordered_map<int, std::shared_ptr<IAgent>> agents_;
    std::unordered_map<int, Action> last_actions_;   // Tracks the last action chosen by each node (key = int node id)

    void logEvent(Event e);
    void prepareChannelParams();
    SDRNode* getNodeById(int id);

public:
    const StepMetrics&    getCurrentMetrics() const noexcept { return current_metrics_; }
    const MetricsHistory& getMetricsHistory() const noexcept { return metrics_history_; }
    void setEmitters(const std::vector<EmitterDesc>& emitters) { emitters_ = emitters; }

private:
    StepMetrics    current_metrics_;
    MetricsHistory metrics_history_;
    std::vector<EmitterDesc> emitters_;   // active emitter descriptions

};

} // namespace sigint_sim