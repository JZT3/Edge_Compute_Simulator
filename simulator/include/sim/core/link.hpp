#pragma once
#include "types.hpp"

namespace sigint_sim {

class Link {
public:
    Link(LinkId id, NodeId from, NodeId to, Frequency band_center);
    void updateFromChannel(double snr, double capacity, double outage_prob, bool active);

    [[nodiscard]] LinkState getState() const noexcept;
    [[nodiscard]] LinkId id() const noexcept { return id_; }
    [[nodiscard]] NodeId from() const noexcept { return from_; }
    [[nodiscard]] NodeId to() const noexcept { return to_; }
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