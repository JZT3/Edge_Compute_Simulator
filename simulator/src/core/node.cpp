#include "../include/sim/core/node.hpp"
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
      bands_(std::move(bands)), x_(x), y_(y), profile_(std::move(profile)),
      rng_(seed)
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
    if (action.scan_params) {
        mode_ = NodeMode::SCAN;
        current_rf_ = *action.scan_params;
    }
    if (!action.process_task_ids.empty()) {
        mode_ = NodeMode::PROCESS;
    }
    if (action.burst) {
        mode_ = NodeMode::TRANSMIT;
        // Generate a short pilot burst (100 samples of 1+0j)
        std::vector<std::complex<float>> burst(100, {1.0f, 0.0f});
        double freq = current_rf_.center_freq > 0.0 ? current_rf_.center_freq : 2.4e9;
        double rate = current_rf_.sample_rate > 0.0 ? current_rf_.sample_rate : 1e6;
        radio_->transmit(burst, freq, rate);
        energy_used_ += 0.001 * 1e-3;  // placeholder TX energy
    }
}

// ---- Processing update ----
void SDRNode::updateProcessing(double dt) {
    // If we have real IQ samples, use the signal processor
    if (mode_ == NodeMode::PROCESS && !rx_samples_.empty()) {
        SignalTask task;
        task.type = SignalTaskType::DETECT;
        task.center_freq_hz = current_rf_.center_freq > 0.0 ? current_rf_.center_freq : 2.4e9;
        task.bandwidth_hz   = current_rf_.bandwidth > 0.0   ? current_rf_.bandwidth   : 1e6;
        task.duration_s     = dt;

        TaskResult res = signal_processor_.execute(task, last_snr_linear_,
                                                   profile_, rng_);
        buffer_size_ = static_cast<int>(res.data.size());
        energy_used_ += res.energy_joules;
        rx_samples_.clear();
        has_unprocessed_signal_ = false;
        return;
    }

    // Fallback for the old synthetic‑signal path
    if (mode_ == NodeMode::PROCESS && has_unprocessed_signal_) {
        energy_used_ += compute_.fft_ops_per_sec * dt * 1e-6;
        has_unprocessed_signal_ = false;
        buffer_size_ = 0;
    }
}

// ---- Radio samples collection ----
void SDRNode::collectRxSamples() {
    if (radio_) {
        double freq = current_rf_.center_freq > 0.0 ? current_rf_.center_freq : 2.4e9;
        double rate = current_rf_.sample_rate > 0.0 ? current_rf_.sample_rate : 1e6;
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
    return s;
}

// ---- Simple signal detection (old interface) ----
void SDRNode::setSignalDetected(bool detected) noexcept {
    has_unprocessed_signal_ = detected;
    if (detected) buffer_size_ = 1;
    else          buffer_size_ = 0;
}

} // namespace sigint_sim