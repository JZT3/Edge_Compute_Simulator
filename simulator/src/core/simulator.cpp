#include "../include/sim/core/simulator.hpp"
#include "../include/sim/agents/agent_interface.hpp"
#include "../include/sim/logging/logger.hpp"
#include "../include/sim/core/sample_processing_channel.hpp"
#include "../include/sim/core/virtual_radio.hpp"
#include "../include/sim/core/metrics.hpp"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <chrono>

namespace sigint_sim {

// ---- Helpers ----
static std::vector<LinkState> collectLinkStates(const std::vector<std::unique_ptr<Link>>& links) {
    std::vector<LinkState> states;
    states.reserve(links.size());
    for (const auto& l : links) states.push_back(l->getState());
    return states;
}

static std::vector<NodeState> collectNodeStates(const std::vector<std::unique_ptr<SDRNode>>& nodes) {
    std::vector<NodeState> states;
    states.reserve(nodes.size());
    for (const auto& n : nodes) states.push_back(n->getState());
    return states;
}

// ---- Constructor ----
Simulator::Simulator(Config config)
    : config_(std::move(config)),
      channel_(config_.channel),
      rng_(config_.seed),
      current_time_(0.0)
{
    assert(config_.timestep > 0.0 && "Timestep must be positive");
    assert(config_.duration > 0.0 && "Duration must be positive");
    assert(config_.channel && "Channel model must not be null");

    // Determine node count from edges
    int max_node_id = -1;
    for (const auto& [from, to] : config_.topology_edges) {
        max_node_id = std::max(max_node_id, std::max(static_cast<int>(from), static_cast<int>(to)));
    }
    const int num_nodes = max_node_id + 1;

    // Detect if we're using the sample‑processing channel
    auto* sample_channel = dynamic_cast<SampleProcessingChannel*>(channel_.get());

    for (int i = 0; i < num_nodes; ++i) {
        ComputeCapability cap{1e9, 0.0, 1ull * 1024 * 1024 * 1024};
        std::vector<Frequency> bands = {100e6, 2.4e9};

        // Profile and position from config, or defaults
        HardwareProfile prof;
        if (config_.node_profiles.count(i)) prof = config_.node_profiles.at(i);
        double x = 0.0, y = 0.0;
        if (config_.node_positions.count(i)) {
            x = config_.node_positions.at(i).first;
            y = config_.node_positions.at(i).second;
        }

        // Derive a deterministic seed for the node's internal RNG
        uint64_t node_seed = config_.seed + static_cast<uint64_t>(i) * 1000;
        auto node = std::make_unique<SDRNode>(NodeId{i}, "Node_" + std::to_string(i),
                                              cap, bands, x, y, prof, node_seed);
        // If sample channel, create a VirtualRadio and inject it
        if (sample_channel) {
            auto radio = std::make_unique<VirtualRadio>(*sample_channel, i);
            node->setRadio(std::move(radio));
            sample_channel->setNodeProfile(i, prof);
        }
        nodes_.push_back(std::move(node));
    }

    // Create links from topology edges
    for (const auto& [from, to] : config_.topology_edges) {
        Frequency rep_band = 2.4e9;
        links_.push_back(std::make_unique<Link>(LinkId{static_cast<int>(links_.size())},
                                                from, to, rep_band));
    }

    event_log_.reserve(static_cast<size_t>(config_.duration / config_.timestep) * 10);
}

// ---- Agent injection ----
void Simulator::setAgent(NodeId id, std::shared_ptr<IAgent> agent) {
    agents_[static_cast<int>(id)] = std::move(agent);
}

// ---- Step --------
// ---------------------------------------------------------------------------
// Public stepping interface
// ---------------------------------------------------------------------------
void Simulator::step() {
    if (isFinished()) return;
    auto t_start = std::chrono::steady_clock::now();

    Logger::get()->debug("----------- step t={:.2f} -----------", current_time_);

    // 0. Pre‑step housekeeping
    prepareChannelParams();

    // 1. Physical layer
    updateChannel();

    // 2. Receive‑side processing
    collectRxSamples();

    // 3. Agent decisions
    makeAgentDecisions();

    // 4. Sensing / intelligence generation
    runSensing();

    // 5. Compute tasks (e.g., local signal processing)
    processComputeTasks();

    // 6. Transmission resolution & logging
    resolveTransmissions();

    // 7. Advance time
    current_time_ += config_.timestep;

    // 8. Step‑level metrics
    auto t_end = std::chrono::steady_clock::now();
    double elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();
    updateStepMetrics(elapsed_us);
}
//keep
int Simulator::runForSteps(int steps) {
    int count_before = delivery_count_;
    for (int i = 0; i < steps && !isFinished(); ++i) {
        step();
    }
    return delivery_count_ - count_before;
}

// ---------------------------------------------------------------------------
// Private helpers (each is a method of Simulator)
// ---------------------------------------------------------------------------
void Simulator::updateChannel() {
    Logger::get()->debug("updateChannel start");
    auto link_states = collectLinkStates(links_);
    channel_->update(link_states, collectNodeStates(nodes_), rng_);
    for (size_t i = 0; i < links_.size(); ++i) {
        links_[i]->updateFromChannel(link_states[i].snr,
                                     link_states[i].capacity_bps,
                                     link_states[i].outage_prob,
                                     link_states[i].active);
    }
    Logger::get()->debug("updateChannel end");
}

void Simulator::collectRxSamples() {
    Logger::get()->debug("collectRxSamples start");
    for (auto& node : nodes_) {
        node->updateProcessing(config_.timestep);
    }
    for (size_t i = 0; i < nodes_.size(); ++i) {
        double snr_sum = 0.0;
        int count = 0;
        for (const auto& link : links_) {
            if (link->to() == nodes_[i]->id() && link->getState().active) {
                snr_sum += link->getState().snr;
                count++;
            }
        }
        double avg_snr_db = count > 0 ? snr_sum / count : -200.0;
        double snr_linear = std::pow(10.0, avg_snr_db / 10.0);
        nodes_[i]->setLastSNR(snr_linear);
    }
    Logger::get()->debug("collectRxSamples end");
}

void Simulator::makeAgentDecisions() {
    Logger::get()->debug("makeAgentDecisions start");
    auto node_states = collectNodeStates(nodes_);
    auto latest_link_states = collectLinkStates(links_);
    for (size_t i = 0; i < nodes_.size(); ++i) {
        auto& node = nodes_[i];
        auto it = agents_.find(static_cast<int>(node->id()));
        if (it == agents_.end()) {
            Logger::get()->warn("No agent for node {}", static_cast<int>(node->id()));
            continue;
        }
        Action action = it->second->selectAction(node_states[i], node_states,
                                                 latest_link_states, event_log_);
        node->applyAction(action);
        last_actions_[static_cast<int>(node->id())] = action;
    }
    Logger::get()->debug("makeAgentDecisions end");
}

void Simulator::runSensing() {
    Logger::get()->debug("runSensing start");
    for (const auto& emitter : emitters_) {
        if (current_time_ >= emitter.active_start_s && current_time_ <= emitter.active_end_s) {
            for (auto& node : nodes_) {
                if (node->getState().mode == NodeMode::SCAN) {
                    double node_freq = node->getState().current_rf.center_freq;
                    if (std::abs(node_freq - emitter.frequency_Hz) / emitter.frequency_Hz < 0.01) {
                        node->setSignalDetected(true);
                        std::vector<std::complex<float>> fake_signal(SYNTHETIC_SIGNAL_SAMPLES, {1.0f, 0.0f});
                        double snr_linear = SYNTHETIC_SIGNAL_SNR_DB;
                        node->injectSyntheticSignal(fake_signal, snr_linear);
                        current_metrics_.cumulative_intelligence += emitter.priority;
                        Logger::get()->info("Intel += {}", emitter.priority);
                        Event ev;
                        ev.time = current_time_;
                        ev.node_id = static_cast<int>(node->id());
                        ev.type = "SignalDetected";
                        ev.params["frequency"] = emitter.frequency_Hz;
                        ev.params["snr"] = snr_linear;
                        logEvent(ev);
                    }
                }
            }
        }
    }
    Logger::get()->debug("runSensing end");
}

void Simulator::processComputeTasks() {
    Logger::get()->debug("processComputeTasks start");
    for (auto& node : nodes_) {
        node->updateProcessing(config_.timestep);
    }
    Logger::get()->debug("processComputeTasks end");
}

void Simulator::resolveTransmissions() {
    Logger::get()->debug("resolveTransmissions start");
    for (size_t i = 0; i < nodes_.size(); ++i) {
        const auto& node = nodes_[i];
        int nid = static_cast<int>(node->id());
        Logger::get()->debug("resolveTransmissions node {}", nid);

        auto it = last_actions_.find(nid);
        if (it == last_actions_.end()) continue;
        const Action& last_action = it->second;
        if (!last_action.burst) continue;

        Event ev;
        ev.time = current_time_;
        ev.node_id = nid;
        int target = last_action.burst->target_node_id;
        bool found_active = false;

        for (const auto& link : links_) {
            if (link->from() == node->id() &&
                link->to() == NodeId{target} &&
                link->getState().active) {
                found_active = true;
                ev.type = "TransmissionSuccess";
                ev.params["to"] = static_cast<double>(target);
                ev.params["capacity_mbps"] = link->getState().capacity_bps / 1e6;
                Logger::get()->info("TX success: node {} -> {} | SNR {:.1f} dB | Cap {:.1f} Mbps",
                                    nid, target, link->getState().snr,
                                    link->getState().capacity_bps / 1e6);
                break;
            }
        }

        if (!found_active) {
            ev.type = "TransmissionFail";
            ev.params["reason"] = 0;
            Logger::get()->info("TX fail: node {} -> {} (no active link)", nid, target);
        }

        current_metrics_.transmissions_attempted++;
        if (found_active) {
            current_metrics_.transmissions_succeeded++;
        } else {
            current_metrics_.lpd_violations++;
        }
        current_metrics_.cumulative_intelligence -= LPD_PENALTY_PER_TX;
        current_metrics_.lpd_violations++;

        logEvent(ev);

        // --- RL reward hook (enabled) ---
        if (found_active) {
            auto agent_it = agents_.find(nid);
            if (agent_it != agents_.end() && agent_it->second->isRLAgent()) {
                static uint32_t next_pkt_id = 1;
                agent_it->second->processLocalAck(next_pkt_id++, 1.0, static_cast<int>(current_time_));
                if (NodeId{target} == sink_node_id_) {
                    ++delivery_count_;
                }
            }
        }
    }
    Logger::get()->debug("resolveTransmissions end");
}

void Simulator::updateStepMetrics(double wall_time_us) {
    current_metrics_.step_execution_time_us = wall_time_us;
    metrics_history_.push_back(current_metrics_);
}

// ---- Reset ----
void Simulator::reset(uint64_t new_seed) {
    current_time_ = 0.0;
    event_log_.clear();
    rng_.seed(new_seed);
}

// ---- State accessors ----
std::vector<NodeState> Simulator::getNodeStates() const {
    return collectNodeStates(nodes_);
}
std::vector<LinkState> Simulator::getLinkStates() const {
    return collectLinkStates(links_);
}
const EventLog& Simulator::getEventLog() const {
    return event_log_;
}

// ---- Event logging ----
void Simulator::logEvent(Event e) {
    e.time = current_time_;
    Logger::get()->info("[t={:.2f}] {} (node {})", e.time, e.type, e.node_id);
    event_log_.push_back(std::move(e));
}

// ---- Channel parameter preparation ----
void Simulator::prepareChannelParams() {
    auto* sc = dynamic_cast<SampleProcessingChannel*>(channel_.get());
    if (!sc) return;
    for (const auto& link : links_) {
        int from = static_cast<int>(link->from());
        int to   = static_cast<int>(link->to());
        auto* from_node = getNodeById(from);
        auto* to_node   = getNodeById(to);
        if (!from_node || !to_node) continue;
        double dist = std::hypot(from_node->posX() - to_node->posX(),
                                 from_node->posY() - to_node->posY());
        double tx_rate = from_node->getState().current_rf.sample_rate > 0.0 ?
                         from_node->getState().current_rf.sample_rate : 1e6;
        double rx_rate = to_node->getState().current_rf.sample_rate > 0.0 ?
                         to_node->getState().current_rf.sample_rate : 1e6;
        double delta_f_ppm = from_node->profile().frequency_accuracy_ppm +
                             to_node->profile().frequency_accuracy_ppm;
        double freq_offset = link->band_center() * delta_f_ppm * 1e-6;
        sc->setLinkParams(from, to, dist, link->band_center(), tx_rate, rx_rate, freq_offset);
    }
}

    // ---- Node lookup helper ----
    SDRNode* Simulator::getNodeById(int id) {
        for (auto& n : nodes_) {
            if (static_cast<int>(n->id()) == id) return n.get();
        }
        return nullptr;
    }


} // namespace sigint_sim