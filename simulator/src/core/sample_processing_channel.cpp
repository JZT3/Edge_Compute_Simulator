#include "../include/sim/core/sample_processing_channel.hpp"
#include "../include/sim/core/phy_math.hpp"
#include "../include/sim/core/sim_config.hpp"
#include "../include/sim/logging/logger.hpp"
#include <cmath>
#include <random>
#include <cassert>

namespace sigint_sim {

SampleProcessingChannel::SampleProcessingChannel(Params p) : params_(std::move(p)) {}

// ---- Radio‑to‑channel interface ----
void SampleProcessingChannel::pushTxSamples(int src_node,
                                            const std::vector<std::complex<float>>& samples,
                                            double sample_rate) {
    for (auto& [key, buf] : link_buffers_) {
        if (key.first == src_node) {
            buf.tx_samples = samples;
            buf.tx_sample_rate = sample_rate;
        }
    }
}

std::vector<std::complex<float>> SampleProcessingChannel::collectRxSamples(int dst_node) {
    std::vector<std::complex<float>> result;
    for (auto& [key, buf] : link_buffers_) {
        if (key.second == dst_node) {
            result = std::move(buf.rx_samples);
            buf.rx_samples.clear();
            break;      // first match wins – sufficient for MVP
        }
    }
    return result;
}

// ---- Node profile registration ----
void SampleProcessingChannel::setNodeProfile(int node_id, const HardwareProfile& profile) {
    node_profiles_[node_id] = profile;
}

// ---- Per‑link parameter configuration (called each step) ----
void SampleProcessingChannel::setLinkParams(int from, int to, double distance_m,
                                            double center_freq, double tx_rate,
                                            double rx_rate, double freq_offset_hz) {
    auto key = std::make_pair(from, to);
    auto& buf = link_buffers_[key];
    buf.distance_m   = distance_m;
    buf.center_freq  = center_freq;
    buf.tx_rate      = tx_rate;
    buf.rx_rate      = rx_rate;
    buf.freq_offset_hz = freq_offset_hz;
}

// ---- Main update (called by Simulator::step) ----
void SampleProcessingChannel::update(std::vector<LinkState>& links,
                                     const std::vector<NodeState>& /*nodes*/,
                                     std::mt19937& rng) {
    for (auto& ls : links) {
        auto key = std::make_pair(static_cast<int>(ls.from), static_cast<int>(ls.to));
        auto it = link_buffers_.find(key);
        if (it != link_buffers_.end()) {
                processLink(it->second, ls, rng, key.first, static_cast<int>(ls.to));
        } else {
            // No buffer → link down
            ls.active = false;
            ls.snr = -200.0;
            ls.capacity_bps = 0.0;
            ls.outage_prob = 1.0;
        }
    }
}

// ---- Core per‑link processing ----
void SampleProcessingChannel::processLink(LinkBuffer& buf, LinkState& state,
                                          std::mt19937& rng, int src_node_id, int dst_node_id) {
    if (buf.tx_samples.empty()) {
        state.active = false;
        state.snr = -200.0;
        state.capacity_bps = 0.0;
        state.outage_prob = 1.0;
        buf.rx_samples.clear();
        return;
    }

    // Retrieve noise figure for the destination node
    double noise_figure_dB = DEFAULT_NOISE_FIGURE_DB; 
    auto prof_it = node_profiles_.find(dst_node_id);
    if (prof_it != node_profiles_.end()) {
        noise_figure_dB = prof_it->second.noise_figure_dB;
    }

    // 1. Path loss (linear gain)
    const double path_loss = phy_math::friis_path_loss_linear(buf.center_freq, buf.distance_m);

    Logger::get()->info("Link from {} to {}: dist={:.1f}m, path_loss_linear={:.6f}", 
                        src_node_id, dst_node_id, buf.distance_m, path_loss);
    // 2. Noise power (W)
    const double noise_W = phy_math::thermal_noise_power_W(params_.bandwidth_Hz, noise_figure_dB);

    // 3. Apply path loss and AWGN
    std::normal_distribution<double> noise_dist(0.0, std::sqrt(noise_W));
    buf.rx_samples.resize(buf.tx_samples.size());
    double signal_power = 0.0;
    for (size_t i = 0; i < buf.tx_samples.size(); ++i) {
        std::complex<double> s(buf.tx_samples[i].real(), buf.tx_samples[i].imag());
        s *= path_loss;
        s += std::complex<double>(noise_dist(rng), noise_dist(rng));
        buf.rx_samples[i] = static_cast<std::complex<float>>(s);
        signal_power += std::norm(s);
    }
    signal_power /= buf.tx_samples.size();

    // 4. Apply frequency offset (rotation per sample)
    if (std::abs(buf.freq_offset_hz) > 1e-9) {
        phy_math::apply_freq_offset(buf.rx_samples, buf.freq_offset_hz, buf.tx_rate);
    }

    // 5. Sample‑rate conversion (nearest‑neighbour)
    if (std::abs(buf.rx_rate - buf.tx_rate) > 1e-6) {
        buf.rx_samples = phy_math::resample_nearest(buf.rx_samples, buf.tx_rate, buf.rx_rate);
    }

    // 6. Metric computations using phy_math
    double snr_linear = signal_power / noise_W;
    state.snr = 10.0 * std::log10(snr_linear);

    double ber = phy_math::ber_qpsk(snr_linear);
    state.active = (ber < params_.ber_threshold) && (state.snr > params_.snr_threshold_dB);

    state.capacity_bps = state.active ? phy_math::shannon_capacity_bps(buf.rx_rate, snr_linear) : 0.0;
    state.outage_prob = phy_math::outage_probability(snr_linear);

    // Clear TX buffer – processed
    buf.tx_samples.clear();
}

} // namespace sigint_sim