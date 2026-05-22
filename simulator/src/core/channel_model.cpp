#include "../include/sim/core/channel_model.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <cassert>

namespace sigint_sim {

void BlockFadingChannel::update(std::vector<LinkState>& links,
                                const std::vector<NodeState>& /*nodes*/,
                                std::mt19937& rng) {
    // Create distributions (lightweight, stack-allocated)
    std::bernoulli_distribution avail_dist(params_.availability);
    std::normal_distribution<double> snr_dist(params_.avg_snr_db, params_.snr_std_db);

    for (auto& link : links) {
        // Determine if the link is available
        const bool available = avail_dist(rng);

        // Generate SNR: if not available, set to a very low value.
        const double snr_db = available ? snr_dist(rng) : -200.0;  // effectively -infinity

        // Link is active if available AND SNR above outage threshold.
        // (A link could be available but suffer a fade that drops it below the threshold;
        //  this models that the fading distribution itself captures deep fades.)
        link.active = available && (snr_db > params_.outage_snr_db);

        link.snr = snr_db;

        // Capacity: Shannon-Hartley C = B * log2(1 + SNR)
        // SNR (linear) = 10^(snr_db/10)
        if (link.active) {
            const double snr_linear = std::pow(10.0, snr_db / 10.0);
            link.capacity_bps = params_.bandwidth * std::log2(1.0 + snr_linear);
        } else {
            link.capacity_bps = 0.0;
        }

        // Outage probability is the complement of the availability probability.
        // In a more detailed model this could be conditionally computed; here it is constant.
        link.outage_prob = 1.0 - params_.availability;
    }
}

#include <cmath>
#include <random>

double DistanceAwareChannel::computeSNR(double distance_m, double noise_figure_dB,
                                        double tx_power_dBm, double freq_MHz,
                                        double bandwidth_Hz,
                                        double shadowing_std_dB, std::mt19937& rng) {
    assert(distance_m > 0.0);
    // Free-space path loss (Friis)
    double path_loss_dB = 20.0 * std::log10(distance_m) + 20.0 * std::log10(freq_MHz) - 27.55;
    // Thermal noise floor: -174 dBm/Hz + 10*log10(bandwidth)
    double noise_floor_dBm = -174.0 + 10.0 * std::log10(bandwidth_Hz);
    // Shadowing
    std::lognormal_distribution<double> shadow_dist(0.0, shadowing_std_dB);
    double shadowing_dB = shadow_dist(rng);
    // SNR = Tx power - path loss - noise figure - thermal noise + shadowing
    double snr_dB = tx_power_dBm - path_loss_dB - noise_figure_dB - noise_floor_dBm + shadowing_dB;
    return snr_dB;
}

void DistanceAwareChannel::update(std::vector<LinkState>& links,
                                   const std::vector<NodeState>& nodes,
                                   std::mt19937& rng) {
    // Build a map from node ID to position + profile (we lack profile in NodeState for now,
    // so we will add a profile to NodeState later; for MVP we use a fixed default.
    // For this code, we assume NodeState has a HardwareProfile field or we pass profiles
    // via the simulator. We'll defer that integration to the simulator side.)
    for (auto& link : links) {
        // Find source and destination nodes
        const NodeState* src = nullptr, *dst = nullptr;
        for (const auto& ns : nodes) {
            if (ns.id == link.from) src = &ns;
            if (ns.id == link.to) dst = &ns;
        }
        if (!src || !dst) {
            link.active = false;
            link.capacity_bps = 0.0;
            continue;
        }
        double dx = src->x - dst->x;
        double dy = src->y - dst->y;
        double dist = std::sqrt(dx*dx + dy*dy);
        if (dist < 0.01) dist = 0.01; // avoid log(0)

        // For Phase 1, we use fixed profiles: we can store a HardwareProfile in NodeState.
        // As a temporary measure, we can read from a member if added; else assume defaults.
        double noise_fig = 10.0;  // placeholder; real profile will come from NodeState
        double tx_power = 10.0;
        double snr = computeSNR(dist, noise_fig, tx_power, params_.frequency_MHz,
                                params_.bandwidth_Hz, params_.shadowing_std_dB, rng);
        link.active = (snr > params_.snr_threshold_dB);
        link.snr = snr;
        if (link.active) {
            double snr_linear = std::pow(10.0, snr/10.0);
            link.capacity_bps = params_.bandwidth_Hz * std::log2(1.0 + snr_linear);
        } else {
            link.capacity_bps = 0.0;
        }
        link.outage_prob = 0.0; // we could compute from CDF, but for MVP set 0
    }
}

} // namespace sigint_sim