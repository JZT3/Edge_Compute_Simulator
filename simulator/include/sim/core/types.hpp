#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
// #include <variant>
#include <limits>

namespace sigint_sim {

// Physical units
using TimePoint = double;
using Frequency = double;       // Hz
using Power = double;           // dBm
using DataRate = double;        // bps
using Energy = double;          // Joules

// Strong ID types (avoids mixing ints)
enum class NodeId : int {};
enum class LinkId : int {};
enum class EmitterId : int {};

// Mode
enum class NodeMode : uint8_t { IDLE, SCAN, PROCESS, TRANSMIT };

// RF parameters
struct RFParams {
    Frequency center_freq = 0.0;
    double bandwidth = 0.0;        // Hz
    double gain = 0.0;             // dB
    double sample_rate = 0.0;      // samples/s
};

// Compute capability vector
struct ComputeCapability {
    double fft_ops_per_sec = 0.0;
    double gpu_tops = 0.0;
    size_t memory_bytes = 0;
};

// Action representation
struct Action {
    std::optional<RFParams> scan_params;          // if set, node scans
    std::vector<int> process_task_ids;             // IDs of signals to process locally
    struct Burst {
        int target_node_id = -1;
        int phy_mode = 0;       // index into MCS table
        Power power = 0.0;
        std::vector<int> payload_ids;
    };
    std::optional<Burst> burst;  // if set, node transmits
    bool stay_silent = true;     // redundancy: if nothing set, silent

    [[nodiscard]] bool is_silent() const noexcept {
        return !scan_params && process_task_ids.empty() && !burst;
    }
};

// Snapshot of node state (all values, no references)
struct NodeState {
    NodeId id;
    std::string name;
    NodeMode mode = NodeMode::IDLE;
    RFParams current_rf;
    ComputeCapability compute;
    int buffer_size = 0;
    double energy_used = 0.0;
    std::unordered_map<int, double> neighbor_availability_belief; // node_id -> probability
    double tx_power_dbm = 20.0;   // per‑node quality (higher → longer range)
    double x = 0.0;
    double y = 0.0;
};

// Link state snapshot
struct LinkState {
    LinkId id;
    NodeId from;
    NodeId to;
    Frequency band_center;       // representative frequency
    double snr = -std::numeric_limits<double>::infinity();
    double capacity_bps = 0.0;
    double outage_prob = 1.0;    // probability of failure this step
    bool active = false;
};

// Event log entry (variant later, for now simple struct)
struct Event {
    TimePoint time;
    int node_id = -1;   // or NodeId
    std::string type;
    std::unordered_map<std::string, double> params;
};

using EventLog = std::vector<Event>;

} // namespace sigint_sim
