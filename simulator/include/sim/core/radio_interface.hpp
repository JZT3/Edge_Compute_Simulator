#pragma once
#include <complex>
#include <vector>
#include <string>

namespace sigint_sim {

class IRadio {
public:
    virtual ~IRadio() = default;

    // Push IQ samples to the air interface.
    virtual void transmit(const std::vector<std::complex<float>>& samples,
                          double center_freq, double sample_rate) = 0;

    // Capture IQ samples from the air interface.
    virtual std::vector<std::complex<float>> receive(double center_freq,
                                                      double sample_rate,
                                                      double duration_s) = 0;

    // Return the SoapySDR device string associated with this radio.
    [[nodiscard]] virtual std::string deviceString() const = 0;
};

} // namespace sigint_sim