#pragma once
#include "channel_model.hpp"
#include "hardware_profile.hpp"
#include "sim_config.hpp"
#include <unordered_map>
#include <deque>

namespace sigint_sim {

// Simple hash for std::pair<int,int>
struct PairHash {
    std::size_t operator()(const std::pair<int,int>& p) const {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

class SampleProcessingChannel : public ChannelModel {
public:
    struct Params {
        double frequency_MHz    = DA_FREQUENCY_MHZ;
        double bandwidth_Hz     = DA_BANDWIDTH_HZ;
        double snr_threshold_dB = DA_SNR_THRESHOLD_DB;     // for active flag
        double ber_threshold    = DEFAULT_BER_THRESHOLD;        // for active flag
    };

    explicit SampleProcessingChannel(Params p);

    void update(std::vector<LinkState>& links,
                const std::vector<NodeState>& nodes,
                std::mt19937& rng) override;

    [[nodiscard]] std::string name() const override { return "SampleProcessing"; }

    // ---- Radio-to-channel interface (called by VirtualRadio) ----
    void pushTxSamples(int src_node,
                       const std::vector<std::complex<float>>& samples,
                       double sample_rate);
    std::vector<std::complex<float>> collectRxSamples(int dst_node);

    // Set hardware profiles per node (so channel can use noise figure, etc.)
    void setNodeProfile(int node_id, const HardwareProfile& profile);

    // Optional: allow per‑link parameter overrides (e.g., different freqs)
    void setLinkParams(int from, int to, double distance_m, double center_freq,
                       double tx_rate, double rx_rate, double freq_offset_hz);

private:
    Params params_;

    // Per‑link state: TX buffer, RX buffer, last computed metrics
    struct LinkBuffer {
        std::vector<std::complex<float>> tx_samples;
        double tx_sample_rate = LINK_DEFAULT_TX_SAMPLE_RATE;
        std::vector<std::complex<float>> rx_samples;
        // physical parameters set by setLinkParams before update
        double distance_m     = LINK_DEFAULT_DISTANCE_M;
        double center_freq    = LINK_DEFAULT_CENTER_FREQ_HZ;
        double tx_rate        = LINK_DEFAULT_TX_RATE;
        double rx_rate        = LINK_DEFAULT_RX_RATE;
        double freq_offset_hz = LINK_DEFAULT_FREQ_OFFSET_HZ;
    };

    // Mapping (src,dst) -> LinkBuffer; we'll use a pair hash
    std::unordered_map<std::pair<int,int>, LinkBuffer, PairHash> link_buffers_;

    // Node profiles for noise figure, etc.
    std::unordered_map<int, HardwareProfile> node_profiles_;

    // Helper to compute path loss, noise, and process samples
    void processLink(LinkBuffer& buf, LinkState& state, std::mt19937& rng, int src_node_id, int dst_node_id);
};
} // namespace sigint_sim