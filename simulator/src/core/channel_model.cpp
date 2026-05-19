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

} // namespace sigint_sim