#include "../include/sim/core/radio_interface.hpp"
#include <deque>

namespace sigint_sim {

class VirtualRadio : public IRadio {
public:
    explicit VirtualRadio(std::string device_str)
        : device_str_(std::move(device_str)) {}

    void transmit(const std::vector<std::complex<float>>& samples,
                  double center_freq, double sample_rate) override {
        // In Phase 1 this is never called; later we will enqueue samples.
        (void)center_freq; (void)sample_rate;
        tx_buffer_.insert(tx_buffer_.end(), samples.begin(), samples.end());
    }

    std::vector<std::complex<float>> receive(double center_freq,
                                              double sample_rate,
                                              double duration_s) override {
        (void)center_freq; (void)sample_rate; (void)duration_s;
        // Return a copy of the receive buffer (empty for now).
        return rx_buffer_;
    }

    [[nodiscard]] std::string deviceString() const override {
        return device_str_;
    }

private:
    std::string device_str_;
    std::vector<std::complex<float>> tx_buffer_;
    std::vector<std::complex<float>> rx_buffer_;
};

} // namespace sigint_sim