#pragma once
#include "types.hpp"
#include "hardware_profile.hpp"
#include <string>
#include <vector>

namespace sigint_sim {

class SDRNode {
public:
     // Old constructor (kept for backward compatibility)
    SDRNode(NodeId id, std::string name, ComputeCapability cap,
            std::vector<Frequency> bands);

    // New constructor with position & profile
    SDRNode(NodeId id, std::string name, ComputeCapability cap,
            std::vector<Frequency> bands,
            double x, double y,
            HardwareProfile profile);

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

public:
    // Getters for position
    [[nodiscard]] double posX() const noexcept { return x_; }
    [[nodiscard]] double posY() const noexcept { return y_; }
    [[nodiscard]] const HardwareProfile& profile() const noexcept { return profile_; }

private:
    double x_ = 0.0;
    double y_ = 0.0;
    HardwareProfile profile_;
};

} // namespace sigint_sim