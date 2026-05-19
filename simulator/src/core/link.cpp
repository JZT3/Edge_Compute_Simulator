#include "../include/sim/core/link.hpp"
#include <cassert>

namespace sigint_sim {

Link::Link(LinkId id, NodeId from, NodeId to, Frequency band_center)
    : id_(id), from_(from), to_(to), band_center_(band_center)
{
    assert(from != to && "Self-loop links are not allowed");
}

void Link::updateFromChannel(double snr, double capacity, double outage_prob, bool active) {
    snr_ = snr;
    capacity_bps_ = capacity;
    outage_prob_ = outage_prob;
    active_ = active;
}

LinkState Link::getState() const noexcept {
    return LinkState{id_, from_, to_, band_center_, snr_, capacity_bps_, outage_prob_, active_};
}

} // namespace sigint_sim