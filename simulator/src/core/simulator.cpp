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
#include <ranges>

namespace sigint_sim {

// ---- Helpers ----
static std::vector<LinkState> collectLinkStates(const std::vector<std::unique_ptr<Link>>& links) {
    return links
        | std::views::transform([](const auto& l) { return l->getState(); })
        | std::ranges::to<std::vector>();
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
      current_time_(0.0),
      next_pkt_id_(1)
{
    assert(config_.timestep > 0.0 && "Timestep must be positive");
    assert(config_.duration > 0.0 && "Duration must be positive");
    assert(config_.channel && "Channel model must not be null");

    // ----- Determine node count from topology edges -----
    int max_node_id = -1;
    for (const auto& edge : config_.topology_edges) {
        max_node_id = std::max(max_node_id,
            std::max(static_cast<int>(edge.first), static_cast<int>(edge.second)));
    }
    const int num_nodes = max_node_id + 1;
    nodes_.reserve(num_nodes);

    // Detect if we're using the sample‑processing channel
    auto* sample_channel = dynamic_cast<SampleProcessingChannel*>(channel_.get());

    for (int i = 0; i < num_nodes; ++i) {
        // --- Hardware profile ---
        HardwareProfile profile;   // default‑constructed if not found
        auto prof_it = config_.node_profiles.find(i);
        if (prof_it != config_.node_profiles.end()) {
            profile = prof_it->second;
        }

        // --- Position ---
        double x = 0.0, y = 0.0;
        auto pos_it = config_.node_positions.find(i);
        if (pos_it != config_.node_positions.end()) {
            x = pos_it->second.first;
            y = pos_it->second.second;
        }

        // --- Compute capability (derived from profile) ---
        ComputeCapability cap;
        cap.fft_ops_per_sec = profile.fft_gflops_per_sec * 1e9;   // convert GFLOPS → ops/s
        cap.gpu_tops = 0.0;
        cap.memory_bytes = static_cast<size_t>(profile.memory_mib) * 1024 * 1024;

        std::vector<Frequency> bands = {100e6, 2.4e9};

        uint64_t node_seed = config_.seed + static_cast<uint64_t>(i) * 1000;

        auto node = std::make_unique<SDRNode>(
            NodeId{i}, "Node_" + std::to_string(i),
            cap, bands,
            x, y, profile, node_seed
        );

        // If sample‑processing channel, attach a VirtualRadio and register profile
        if (sample_channel) {
            auto radio = std::make_unique<VirtualRadio>(*sample_channel, i);
            node->setRadio(std::move(radio));
            sample_channel->setNodeProfile(i, profile);
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

    // 1. Physical channel (distance‑aware or block‑fading)
    auto link_states = collectLinkStates(links_);
    channel_->update(link_states, collectNodeStates(nodes_), rng_);
    for (size_t i = 0; i < links_.size(); ++i) {
        links_[i]->updateFromChannel(link_states[i].snr,
                                     link_states[i].capacity_bps,
                                     link_states[i].outage_prob,
                                     link_states[i].active);
    }

    // 2. Gilbert‑Elliott per‑link state evolution
    for (auto& link : links_) {
        link->updateGilbertElliott(rng_);
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

        // Record transmit frequency for interference checking
        if (action.burst.has_value() && action.scan_params.has_value()) {
            last_tx_freq_[static_cast<int>(node->id())] =
                action.scan_params->center_freq;
        } else {
            last_tx_freq_.erase(static_cast<int>(node->id()));
        }
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
        bool success = false;

        // 1. Locate the target link
        const Link* target_link = nullptr;
        for (const auto& link : links_) {
            if (link->from() == node->id() &&
                link->to() == NodeId{target}) {
                target_link = link.get();
                break;
            }
        }

        if (!target_link) {
            ev.type = "TransmissionFail";
            ev.params["reason"] = 1;   // no such link
            Logger::get()->info("TX fail: node {} -> {} (no link)", nid, target);
            logEvent(ev);
            // no lpd increment here
        } else {
            // 2. Physical layer checks: SNR and Gilbert‑Elliott
            bool snr_ok = target_link->getState().active;
            bool ge_ok  = target_link->isGEActive();
            if (!snr_ok || !ge_ok) {
                ev.type = "TransmissionFail";
                ev.params["reason"] = 2;   // poor channel
                Logger::get()->info("TX fail: node {} -> {} (SNR/GE bad)", nid, target);
                logEvent(ev);
            } else {
                // 3. Interference check
                double my_freq = 2.4e9; // fallback
                auto freq_it = last_tx_freq_.find(nid);
                if (freq_it != last_tx_freq_.end())
                    my_freq = freq_it->second;

                bool interfered = false;
                for (const auto& other_node : nodes_) {
                    if (other_node->id() == node->id()) continue;
                    int other_id = static_cast<int>(other_node->id());
                    auto other_it = last_actions_.find(other_id);
                    if (other_it == last_actions_.end()) continue;
                    const Action& other_action = other_it->second;
                    if (!other_action.burst) continue;

                    double other_freq = 2.4e9;
                    auto ofreq_it = last_tx_freq_.find(other_id);
                    if (ofreq_it != last_tx_freq_.end())
                        other_freq = ofreq_it->second;

                    if (std::abs(my_freq - other_freq) < 100e3) {
                        interfered = true;
                        break;
                    }
                }

                if (interfered) {
                    ev.type = "TransmissionFail";
                    ev.params["reason"] = 3;   // interference
                    Logger::get()->info("TX fail: node {} -> {} (interference)", nid, target);
                    logEvent(ev);
                } else {
                    // All checks passed
                    success = true;
                    ev.type = "TransmissionSuccess";
                    ev.params["to"] = static_cast<double>(target);
                    ev.params["capacity_mbps"] =
                        target_link->getState().capacity_bps / 1e6;
                    Logger::get()->info(
                        "TX success: node {} -> {} | SNR {:.1f} dB | Cap {:.1f} Mbps",
                        nid, target, target_link->getState().snr,
                        target_link->getState().capacity_bps / 1e6);
                    logEvent(ev);

            //RL Reward Hook
            auto agent_it = agents_.find(nid);
            if (agent_it != agents_.end() && agent_it->second->isRLAgent()) {
                // Use the monotonic packet-ID counter (member of Simulator)
                uint32_t pkt_id = next_pkt_id_++;
                // Local ACK for the transmitter
                agent_it->second->processLocalAck(pkt_id, 1.0,
                                                static_cast<int>(current_time_));
                // Sink delivery – broadcast sink summary to ALL RL agents
                if (NodeId{target} == sink_node_id_) {
                    ++delivery_count_;
                    // Every RL agent receives the sink summary; those that didn't
                    // originate the packet will simply ignore it.
                    for (auto& [id, agent] : agents_) {
                        if (agent->isRLAgent()) {
                            agent->processSinkSummary({{pkt_id, 1.0}},
                                                    static_cast<int>(current_time_));
                        }
                    }
                }
            }
        }

        // Common metrics – executed for every transmission attempt
        current_metrics_.transmissions_attempted++;
        if (success) {
            current_metrics_.transmissions_succeeded++;
        }
        // Exactly one LPD violation per transmission (success or failure)
        current_metrics_.lpd_violations++;
        current_metrics_.cumulative_intelligence -= LPD_PENALTY_PER_TX;
    }
    Logger::get()->debug("resolveTransmissions end");
}
}
}

void Simulator::updateStepMetrics(double wall_time_us) {
    current_metrics_.step_execution_time_us = wall_time_us;
    metrics_history_.push_back(current_metrics_);
}

// ---- Reset ----
void Simulator::reset(uint64_t new_seed) {
    current_time_ = 0.0;
    event_log_.clear();
    delivery_count_ = 0;
    next_pkt_id_ = 1;
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