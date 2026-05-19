#pragma once
#include "types.hpp"
#include <string>
#include <vector>

namespace sigint_sim {

class SDRNode {
public:
    SDRNode(NodeId id, std::string name, ComputeCapability cap,
            std::vector<Frequency> supported_bands);

    // Apply an action for this timestep
    void applyAction(const Action& action);

    // Update internal processing simulation for dt seconds
    void updateProcessing(double dt);

    // Get a read‑only snapshot of the current state
    [[nodiscard]] NodeState getState() const noexcept;

    // Inject a synthetic signal detection (MVP only).
    void setSignalDetected(bool detected) noexcept;

    // Getters
    [[nodiscard]] NodeId id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] ComputeCapability computeCap() const noexcept { return compute_; }

private:
    NodeId id_;
    std::string name_;
    ComputeCapability compute_;
    std::vector<Frequency> bands_;
    NodeMode mode_ = NodeMode::IDLE;
    RFParams current_rf_;
    int buffer_size_ = 0;
    double energy_used_ = 0.0;
    // In MVP, no complex queue yet; just a flag indicating if we have unprocessed data
    bool has_unprocessed_signal_ = false;
};

} // namespace sigint_sim