#include "../include/sim/core/node.hpp"
#include <cassert>

namespace sigint_sim {

// Old constructor – delegates to the new one with defaults
SDRNode::SDRNode(NodeId id, std::string name, ComputeCapability cap,
                 std::vector<Frequency> bands)
    : SDRNode(std::move(id), std::move(name), std::move(cap),
              std::move(bands),
              0.0, 0.0,                // position (0,0)
              HardwareProfile{})        // default profile
{
    assert(!name_.empty() && "Node name must not be empty");
    assert(!bands_.empty() && "Node must support at least one frequency band");
}

// New constructor – full implementation
SDRNode::SDRNode(NodeId id, std::string name, ComputeCapability cap,
                 std::vector<Frequency> bands,
                 double x, double y,
                 HardwareProfile profile)
    : id_(id), name_(std::move(name)), compute_(std::move(cap)),
      bands_(std::move(bands)), x_(x), y_(y), profile_(std::move(profile))
{
    assert(!name_.empty());
    assert(!bands_.empty());
    assert(std::isfinite(x_) && std::isfinite(y_));
}

void SDRNode::applyAction(const Action& action) {
    // Reset mode before applying the new action
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
        // Burst target and phy are stored in the action but resolution happens in simulator.
    }
    // If nothing is set, node remains IDLE.
}

void SDRNode::updateProcessing(double dt) {
    if (mode_ == NodeMode::PROCESS && has_unprocessed_signal_) {
        // Simple placeholder: processing finishes instantly.
        energy_used_ += compute_.fft_ops_per_sec * dt * 1e-6;  // energy in micro-joules
        has_unprocessed_signal_ = false;
        buffer_size_ = 0; // This makes the buffer state consistent: a signal detection sets the buffer, and processing clears it.
    }
}

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
    // neighbour beliefs not set in MVP maybe in game theory addition
    return s;
}

void SDRNode::setSignalDetected(bool detected) noexcept {
    has_unprocessed_signal_ = detected;
    if (detected) buffer_size_ = 1;   // minimal buffer model
    else buffer_size_ = 0;
}

} // namespace sigint_sim