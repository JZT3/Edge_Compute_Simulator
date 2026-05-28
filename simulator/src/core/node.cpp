#include "../include/sim/core/node.hpp"
#include "../include/sim/logging/logger.hpp"
#include "../include/sim/core/sim_config.hpp"
#include <cassert>
#include <cmath>

namespace sigint_sim {

// ---- Original constructor (delegates to extended) ----
SDRNode::SDRNode(NodeId id, std::string name, ComputeCapability cap,
                 std::vector<Frequency> bands)
    : SDRNode(std::move(id), std::move(name), std::move(cap),
              std::move(bands), 0.0, 0.0, HardwareProfile{}, 0)
{
}

// ---- Extended constructor ----
SDRNode::SDRNode(NodeId id, std::string name, ComputeCapability cap,
                 std::vector<Frequency> bands,
                 double x, double y,
                 HardwareProfile profile,
                 uint64_t seed)
    : id_(std::move(id)), name_(std::move(name)), compute_(std::move(cap)),
      bands_(std::move(bands)), x_(x), y_(y),rng_(seed), profile_(std::move(profile))
      
{
    assert(!name_.empty() && "Node name must not be empty");
    assert(!bands_.empty() && "Node must support at least one frequency band");
    assert(std::isfinite(x_) && std::isfinite(y_));
}

// ---- Radio injection ----
void SDRNode::setRadio(std::unique_ptr<IRadio> radio) {
    radio_ = std::move(radio);
}

// ---- Action application ----
void SDRNode::applyAction(const Action& action) {
    mode_ = NodeMode::IDLE;

    // SCAN
    if (action.scan_params) {
        mode_ = NodeMode::SCAN;
        current_rf_ = *action.scan_params;
    }

    // PROCESS
    if (!action.process_task_ids.empty()) {
        mode_ = NodeMode::PROCESS;
    }

    // TRANSMIT
    if (action.burst) {
        mode_ = NodeMode::TRANSMIT;

        // Safety: copy the burst so we don't read a dangling optional
        const Action::Burst& burst = *action.burst;

        if (radio_) {
            // Use valid frequencies; default to 2.4 GHz / 1 MHz if nothing set
            double freq = (current_rf_.center_freq > 0.0)   ? current_rf_.center_freq : NODE_FALLBACK_CENTER_FREQ_HZ;
            double rate = (current_rf_.sample_rate > 0.0)   ? current_rf_.sample_rate : NODE_FALLBACK_SAMPLE_RATE_HZ;

            // Create a simple pilot burst
            std::vector<std::complex<float>> samples(NODE_TX_PILOT_SAMPLES, {1.0f, 0.0f});

            // Diagnostic (remove after debugging)
            Logger::get()->debug(
                "Node {} transmitting {} samples on freq {:.1f} MHz rate {:.1f} MHz -> node {}",
                static_cast<int>(id_), samples.size(), freq/1e6, rate/1e6, burst.target_node_id
            );

            try {
                radio_->transmit(samples, freq, rate);
            } catch (const std::exception& e) {
                Logger::get()->error("Node {} transmit failed: {}", static_cast<int>(id_), e.what());
            }
            energy_used_ += NODE_TX_ENERGY_INCREMENT_J;   // placeholder TX energy
        }
    }
}

// ---- Processing update ----
void SDRNode::updateProcessing(double dt) {
    // If we have real IQ samples from a radio, use the signal processor
    if (mode_ == NodeMode::PROCESS && !rx_samples_.empty()) {
        SignalTask task;
        task.type = SignalTaskType::DETECT;    // could later be determined by action
        task.center_freq_hz = current_rf_.center_freq > 0.0 ? current_rf_.center_freq : 2.4e9;
        task.bandwidth_hz   = current_rf_.bandwidth   > 0.0 ? current_rf_.bandwidth   :   1e6;
        task.duration_s     = dt;

        // Use the SNR that was stored from the latest RX (set by simulator after collectRxSamples)
        TaskResult res = signal_processor_.execute(task, last_snr_linear_,
                                                   profile_, rng_);

        buffer_size_ = static_cast<int>(res.data.size());
        energy_used_ += res.energy_joules;
        rx_samples_.clear();
        has_unprocessed_signal_ = false;
        return;
    }

    // Fallback for the old synthetic‑signal path (no radio)
    if (mode_ == NodeMode::PROCESS && has_unprocessed_signal_) {
        energy_used_ += compute_.fft_ops_per_sec * dt * NODE_PROCESSING_ENERGY_FACTOR;
        has_unprocessed_signal_ = false;
        buffer_size_ = 0;
    }
}

// ---- Radio samples collection ----
void SDRNode::collectRxSamples() {
    if (radio_) {
        double freq = current_rf_.center_freq > 0.0 ? current_rf_.center_freq : NODE_FALLBACK_CENTER_FREQ_HZ;
        double rate = current_rf_.sample_rate > 0.0 ? current_rf_.sample_rate : NODE_FALLBACK_SAMPLE_RATE_HZ;
        rx_samples_ = radio_->receive(freq, rate, 0.0);
        // The radio is expected to store the SNR of the last successful receive.
        // We retrieve it via a dedicated method (to be added to IRadio or VirtualRadio).
        // For now we keep last_snr_linear_ from a separate update.
    }
}

// ---- Synthetic signal injection ----
void SDRNode::injectSyntheticSignal(const std::vector<std::complex<float>>& iq,
                                    double snr_linear) {
    rx_samples_ = iq;
    last_snr_linear_ = snr_linear;
    has_unprocessed_signal_ = true;
}

// ---- State snapshot ----
NodeState SDRNode::getState() const noexcept {
    NodeState s;
    s.id = id_;
    s.name = name_;
    s.mode = mode_;
    s.current_rf = current_rf_;
    s.compute = compute_;
    s.buffer_size = buffer_size_;
    s.energy_used = energy_used_;
    s.x = x_;
    s.y = y_;
    s.min_freq_hz = profile_.min_freq_hz;
    s.max_freq_hz = profile_.max_freq_hz;

    s.device_type = profile_.deviceTypeToString();   // add a helper to convert enum to string
    s.noise_figure_dB = profile_.noise_figure_dB;
    s.tx_power_dBm = profile_.tx_power_dBm;
    s.frequency_accuracy_ppm = profile_.frequency_accuracy_ppm;
    s.fft_gflops_per_sec = profile_.fft_gflops_per_sec;
    return s;
}

// ---- Simple signal detection (old interface) ----
void SDRNode::setSignalDetected(bool detected) noexcept {
    has_unprocessed_signal_ = detected;
    if (detected) buffer_size_ = 1;
    else          buffer_size_ = 0;
}

inline std::string deviceTypeToString(DeviceType t) {
    switch (t) {
        case DeviceType::RTL_SDR:   return "RTL_SDR";
        case DeviceType::HACKRF:    return "HACKRF";
        case DeviceType::LIME_SDR:  return "LIME_SDR";
        case DeviceType::USRP_B2XX: return "USRP_B2XX";
        case DeviceType::USRP_X3XX: return "USRP_X3XX";
        default:                    return "VIRTUAL";
    }
}

} // namespace sigint_sim