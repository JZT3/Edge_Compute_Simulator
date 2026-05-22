#pragma once
#include "types.hpp"
#include "hardware_profile.hpp"
#include "radio_interface.hpp"
#include "signal_processor.hpp"
#include <complex>
#include <random>
#include <string>
#include <vector>

namespace sigint_sim {

class SDRNode {
public:
    // Original constructor (backward‑compatible)
    SDRNode(NodeId id, std::string name, ComputeCapability cap,
            std::vector<Frequency> bands);

    // Extended constructor with position and profile
    SDRNode(NodeId id, std::string name, ComputeCapability cap,
            std::vector<Frequency> bands,
            double x, double y,
            HardwareProfile profile,
            uint64_t seed = 0);

    // Inject a radio (called by Simulator after both are created)
    void setRadio(std::unique_ptr<IRadio> radio);

    // Apply an action for this timestep
    void applyAction(const Action& action);

    // Update internal processing simulation for dt seconds
    void updateProcessing(double dt);

    // Collect IQ samples from the radio (called once per step)
    void collectRxSamples();

    // Inject a synthetic signal directly into rx_samples_ (for MVP sensing)
    void injectSyntheticSignal(const std::vector<std::complex<float>>& iq,
                               double snr_linear);

    void setLastSNR(double snr_linear) noexcept { last_snr_linear_ = snr_linear; }

    // Get a read‑only snapshot of the current state
    [[nodiscard]] NodeState getState() const noexcept;

    // Inject a simple signal detection (MVP fallback)
    void setSignalDetected(bool detected) noexcept;

    // Accessors
    [[nodiscard]] NodeId id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] ComputeCapability computeCap() const noexcept { return compute_; }
    [[nodiscard]] double posX() const noexcept { return x_; }
    [[nodiscard]] double posY() const noexcept { return y_; }
    [[nodiscard]] const HardwareProfile& profile() const noexcept { return profile_; }
    [[nodiscard]] IRadio* radio() noexcept { return radio_.get(); }

private:
    NodeId id_;
    std::string name_;
    ComputeCapability compute_;
    std::vector<Frequency> bands_;
    NodeMode mode_ = NodeMode::IDLE;
    RFParams current_rf_;
    int buffer_size_ = 0;
    double energy_used_ = 0.0;
    bool has_unprocessed_signal_ = false;

    // --- New PHY members ---
    std::unique_ptr<IRadio> radio_;
    SignalProcessor signal_processor_;
    std::vector<std::complex<float>> rx_samples_;
    double last_snr_linear_ = 0.0;
    std::mt19937 rng_;                     // per‑node RNG for processing

    double x_ = 0.0;
    double y_ = 0.0;
    HardwareProfile profile_;
};

} // namespace sigint_sim