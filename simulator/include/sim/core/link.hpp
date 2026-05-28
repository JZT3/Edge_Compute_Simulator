#pragma once
#include "types.hpp"
#include <random>

namespace sigint_sim {

class Link {
public:
    Link(LinkId id, NodeId from, NodeId to, Frequency band_center);
    void updateFromChannel(double snr, double capacity, double outage_prob, bool active);

    [[nodiscard]] LinkState getState() const noexcept;
    [[nodiscard]] LinkId id() const noexcept { return id_; }
    [[nodiscard]] NodeId from() const noexcept { return from_; }
    [[nodiscard]] NodeId to() const noexcept { return to_; }

    // Gilbert‑Elliott state
    struct GEState {
        bool good = true;       // current state (true = Good, false = Bad)
        double p_gg = 0.95;     // P(stay Good)
        double p_bb = 0.95;     // P(stay Bad)
    };
    GEState ge_state;

    // Advance the GE chain one step (call once per simulation step)
    void updateGilbertElliott(std::mt19937& rng) {
        std::bernoulli_distribution trans;
        if (ge_state.good) {
            trans.param(std::bernoulli_distribution::param_type(ge_state.p_gg));
            ge_state.good = trans(rng);               // stay Good with prob p_gg
        } else {
            trans.param(std::bernoulli_distribution::param_type(1.0 - ge_state.p_bb));
            ge_state.good = trans(rng);               // recover to Good with prob (1‑p_bb)
        }
    }

    [[nodiscard]] bool isGEActive() const noexcept { return ge_state.good; }
    [[nodiscard]] Frequency band_center() const noexcept { return band_center_; }

private:
    LinkId id_;
    NodeId from_;
    NodeId to_;
    Frequency band_center_;
    double snr_ = 0.0;
    double capacity_bps_ = 0.0;
    double outage_prob_ = 1.0;
    bool active_ = false;
};

} // namespace sigint_sim