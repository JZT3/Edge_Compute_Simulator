#pragma once

#include "types.hpp"
#include "sim_config.hpp"
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
        double availability  = BF_LINK_AVAILABILITY;      
        double avg_snr_db    = BF_AVG_SNR_DB;     // Mean SNR when available (dB)
        double snr_std_db    = BF_SNR_STD_DB;      // Standard deviation of SNR (dB)
        double outage_snr_db = BF_OUTAGE_SNR_DB ;   // SNR below which link is considered unusable
        double bandwidth     = BF_BANDWIDTH_HZ;     // Representative bandwidth (Hz) for capacity calc
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

class DistanceAwareChannel : public ChannelModel {
public:
    struct Params {
        double frequency_MHz = 2400.0;       // center frequency of links
        double bandwidth_Hz = 10e6;
        double shadowing_std_dB = 3.0;       // lognormal shadowing sigma
        double snr_threshold_dB = 5.0;       // SNR below which link is considered down
    };

    explicit DistanceAwareChannel(Params p) noexcept : params_(p) {}

    void update(std::vector<LinkState>& links,
                const std::vector<NodeState>& nodes,
                std::mt19937& rng) override;

    [[nodiscard]] std::string name() const override { return "DistanceAware"; }

private:
    Params params_;
    static double computeSNR(double distance_m, double noise_figure_dB,
                             double tx_power_dBm, double freq_MHz, double bandwidth_Hz,
                             double shadowing_std_dB, std::mt19937& rng);
};

} // namespace sigint_sim