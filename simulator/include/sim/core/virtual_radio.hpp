#pragma once
#include "radio_interface.hpp"
#include <cstddef>

namespace sigint_sim {

// Forward declarations
class SampleProcessingChannel;

class VirtualRadio : public IRadio {
public:
    // Construct with a reference to the channel and the node's own id.
    // The channel knows how to route samples for each link.
    VirtualRadio(SampleProcessingChannel& channel, int node_id);

    void transmit(const std::vector<std::complex<float>>& samples,
                  double center_freq, double sample_rate) override;

    std::vector<std::complex<float>> receive(double center_freq,
                                             double sample_rate,
                                             double duration_s) override;

    [[nodiscard]] std::string deviceString() const override {
        return "virtual";
    }

private:
    SampleProcessingChannel& channel_;   // the link processor
    int node_id_;                        // which node this radio belongs to
};

} // namespace sigint_sim