#include "../include/sim/core/simulator.hpp"
#include "../include/sim/agents/agent_interface.hpp"
#include "../include/sim/logging/logger.hpp"
#include <cassert>
#include <algorithm>

namespace sigint_sim {

// --- Helper to collect current link states (vector of copy) ---
static std::vector<LinkState> collectLinkStates(const std::vector<std::unique_ptr<Link>>& links) {
    std::vector<LinkState> states;
    states.reserve(links.size());
    for (const auto& l : links) {
        states.push_back(l->getState());
    }
    return states;
}

// --- Helper to collect current node states ---
static std::vector<NodeState> collectNodeStates(const std::vector<std::unique_ptr<SDRNode>>& nodes) {
    std::vector<NodeState> states;
    states.reserve(nodes.size());
    for (const auto& n : nodes) {
        states.push_back(n->getState());
    }
    return states;
}

Simulator::Simulator(Config config)
    : config_(std::move(config)),
    channel_(config_.channel), 
    rng_(config_.seed), 
    current_time_(0.0)
{
    assert(config_.timestep > 0.0 && "Timestep must be positive");
    assert(config_.duration > 0.0 && "Duration must be positive");
    assert(config_.channel && "Channel model must not be null");

    // Create nodes: one per unique node ID in topology_edges
    // For simplicity, assume nodes numbered 0..N-1 and edges refer to them.
    int max_node_id = -1;
    for (const auto& [from, to] : config_.topology_edges) {
        max_node_id = std::max(max_node_id, std::max(static_cast<int>(from), static_cast<int>(to)));
    }
    const int num_nodes = max_node_id + 1;

    for (int i = 0; i < num_nodes; ++i) {
        ComputeCapability cap{1e9, 0.0, 1ull*1024*1024*1024}; // 1 GFLOPS, 1 GiB
        std::vector<Frequency> bands = {100e6, 2.4e9};        // typical bands
        nodes_.push_back(std::make_unique<SDRNode>(NodeId{i}, "Node_" + std::to_string(i), cap, bands));
    }

    // Create links from topology edges
    for (const auto& [from, to] : config_.topology_edges) {
        Frequency rep_band = 2.4e9; // default; later could be per-edge
        // ensure link IDs are unique: use incremental
        links_.push_back(std::make_unique<Link>(LinkId{static_cast<int>(links_.size())},
                                                from, to, rep_band));
    }

    event_log_.reserve(static_cast<size_t>(config_.duration / config_.timestep) * 10); // guess
}

void Simulator::setAgent(NodeId id, std::unique_ptr<IAgent> agent) {
    agents_[static_cast<int>(id)] = std::move(agent);
}

void Simulator::step() {
    if (isFinished()) return;

    // 1. Update channel (links)
    auto link_states = collectLinkStates(links_);
    channel_->update(link_states, collectNodeStates(nodes_), rng_);
    // Push updated link states back to Link objects
    for (size_t i = 0; i < links_.size(); ++i) {
        links_[i]->updateFromChannel(link_states[i].snr,
                                     link_states[i].capacity_bps,
                                     link_states[i].outage_prob,
                                     link_states[i].active);
    }

    // 2. Get fresh node states (after any previous step effects)
    auto node_states = collectNodeStates(nodes_);
    auto latest_link_states = collectLinkStates(links_); // post-update

    // 3. For each node, ask its agent for an action
    for (size_t i = 0; i < nodes_.size(); ++i) {
        auto& node = nodes_[i];
        auto it = agents_.find(static_cast<int>(node->id()));
        if (it == agents_.end()) {
            Logger::get()->warn("No agent for node {}", static_cast<int>(node->id()));
            continue; // node stays idle
        }
        // Prepare the world view for this agent
        // (We pass all node states and all link states; the agent sees the full picture.)
        Action action = it->second->selectAction(node_states[i], node_states, latest_link_states, event_log_);
        node->applyAction(action);
    }

    // 4. Simulate sensing: if node is SCANing and a signal is present (hardcoded for demo)
    //    For MVP we introduce a very simple emitter that node 0 can hear.
    if (current_time_ < 5.0) { // signal present during first 5 seconds
        for (auto& node : nodes_) {
            if (node->getState().mode == NodeMode::SCAN) {
                // Check if the node's scan band covers the emitter frequency (2.4 GHz)
                if (node->getState().current_rf.center_freq > 2.3e9 &&
                    node->getState().current_rf.center_freq < 2.5e9) {
                    node->setSignalDetected(true);
                    Event ev;
                    ev.time = current_time_;
                    ev.node_id = static_cast<int>(node->id());
                    ev.type = "SignalDetected";
                    ev.params["frequency"] = 2.4e9;
                    ev.params["snr"] = 15.0;
                    logEvent(ev);
                }
            }
        }
    }

    // 5. Process compute tasks (drain buffers)
    for (auto& node : nodes_) {
        node->updateProcessing(config_.timestep);
    }

    // 6. Handle transmissions (check link activity)
    for (size_t i = 0; i < nodes_.size(); ++i) {
        const auto& node = nodes_[i];
        // The last action is not stored, but we can deduce from node's current mode
        // Better: store last action per node. For simplicity, we'll re-derive:
        // (This is a simplification; in later versions we store last action)
        // We'll skip burst handling for now because we don't have action history.
        // Instead we'll add a placeholder: since RandomAgent may choose TX, we'll just
        // log a transmission attempt when node mode is TRANSMIT.
        if (node->getState().mode == NodeMode::TRANSMIT) {
            Event ev;
            ev.time = current_time_;
            ev.node_id = static_cast<int>(node->id());
            // Find a link from this node to any other
            bool found_link = false;
            for (const auto& link : links_) {
                if (link->from() == node->id() && link->getState().active) {
                    ev.type = "TransmissionSuccess";
                    ev.params["to"] = static_cast<double>(link->to());
                    ev.params["capacity_mbps"] = link->getState().capacity_bps / 1e6;
                    found_link = true;
                    break;
                }
            }
            if (!found_link) {
                ev.type = "TransmissionFail";
                ev.params["reason"] = 0; // no active link
            }
            logEvent(ev);
        }
    }

    // 7. Advance time
    current_time_ += config_.timestep;
}

void Simulator::reset(uint64_t new_seed) {
    current_time_ = 0.0;
    event_log_.clear();
    rng_.seed(new_seed);
    // Reset nodes/links? For now, just re-create same config would be needed.
    // This is a minimal reset; in practice you'd reinitialise nodes as well.
}

std::vector<NodeState> Simulator::getNodeStates() const {
    return collectNodeStates(nodes_);
}

std::vector<LinkState> Simulator::getLinkStates() const {
    return collectLinkStates(links_);
}

const EventLog& Simulator::getEventLog() const {
    return event_log_;
}

void Simulator::logEvent(Event e) {
    e.time = current_time_;
    event_log_.push_back(std::move(e));
    // Also write to spdlog
    Logger::get()->info("[t={:.2f}] {} (node {})", e.time, e.type, e.node_id);
}

} // namespace sigint_sim