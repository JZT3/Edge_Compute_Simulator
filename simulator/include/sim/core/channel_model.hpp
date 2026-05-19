#pragma once

#include "types.hpp"
#include <memory>
#include <random>
#include <vector>

namespace sigint_sim {

// -------------------------------------------------------------------------
// Abstract channel model interface.
// Each concrete model implements the `update` method that modifies
// link states based on the current node configuration and a
// shared random number generator.
// -------------------------------------------------------------------------
class ChannelModel {
public:
    virtual ~ChannelModel() = default;

    // Update the link states for the current time step.
    // @param links   Mutable view of all link states (updated in place).
    // @param nodes   Current node states (read-only, used e.g. for positions).
    // @param rng     Shared random number generator for deterministic results.
    virtual void update(std::vector<LinkState>& links,
                        const std::vector<NodeState>& nodes,
                        std::mt19937& rng) = 0;

    // Human-readable name for logging / UI.
    [[nodiscard]] virtual std::string name() const = 0;
};

// -------------------------------------------------------------------------
// Block-fading channel: each link independently switches between
// "available" and "outage" with a fixed probability.
// When available, the SNR is drawn from a log-normal distribution.
// Capacity is estimated using the Shannon-Hartley formula.
// -------------------------------------------------------------------------
class BlockFadingChannel : public ChannelModel {
public:
    struct Params {
        double availability = 0.8;      // Probability that a link is not in deep fade
        double avg_snr_db   = 20.0;     // Mean SNR when available (dB)
        double snr_std_db   = 5.0;      // Standard deviation of SNR (dB)
        double outage_snr_db = -10.0;   // SNR below which link is considered unusable
        double bandwidth    = 10e6;     // Representative bandwidth (Hz) for capacity calc
    };

    // Construct with parameters (copies, no allocation).
    explicit BlockFadingChannel(Params p) noexcept : params_(std::move(p)) {}

    void update(std::vector<LinkState>& links,
                const std::vector<NodeState>& nodes,
                std::mt19937& rng) override;

    [[nodiscard]] std::string name() const override { return "BlockFading"; }

private:
    Params params_;
};

} // namespace sigint_sim