#pragma once
#include <cmath>
#include <complex>
#include <random>
#include <vector>

namespace sigint_sim::phy_math {

// ---------------------------------------------------------------------------
// Physical constants
// ---------------------------------------------------------------------------
inline constexpr double SPEED_OF_LIGHT = 3.0e8;       // m/s
inline constexpr double BOLTZMANN      = 1.380649e-23; // J/K
inline constexpr double ROOM_TEMP_K    = 290.0;        // K

// ---------------------------------------------------------------------------
// Path loss
// ---------------------------------------------------------------------------

/** Friis free‑space path loss (linear gain, ≤ 1). */
inline double friis_path_loss_linear(double freq_Hz, double distance_m) {
    const double lambda = SPEED_OF_LIGHT / freq_Hz;
    const double denom  = 4.0 * M_PI * std::max(distance_m, 0.01);
    const double gain   = lambda / denom;
    return gain * gain;                 // (λ / 4πd)^2
}

// ---------------------------------------------------------------------------
// Noise power
// ---------------------------------------------------------------------------

/** Thermal noise power (W) in the given bandwidth, with receiver noise figure (dB). */
inline double thermal_noise_power_W(double bandwidth_Hz, double noise_figure_dB) {
    const double kTB = BOLTZMANN * ROOM_TEMP_K * bandwidth_Hz;   // W
    const double nf_linear = std::pow(10.0, noise_figure_dB / 10.0);
    return kTB * nf_linear;
}

// ---------------------------------------------------------------------------
// SNR computation
// ---------------------------------------------------------------------------

/**
 * Compute linear SNR from a link’s physical parameters.
 * @param tx_power_dBm        transmitter power (dBm)
 * @param distance_m          distance between nodes (m)
 * @param freq_Hz             centre frequency (Hz)
 * @param bandwidth_Hz        receiver noise bandwidth (Hz)
 * @param noise_figure_dB     receiver noise figure (dB)
 * @param shadowing_std_dB    (optional) log‑normal shadowing sigma; if >0, rng is used
 * @param rng                 random engine for shadowing (ignored if shadowing_std_dB ≤ 0)
 */
inline double snr_linear(double tx_power_dBm, double distance_m,
                         double freq_Hz, double bandwidth_Hz,
                         double noise_figure_dB,
                         double shadowing_std_dB = 0.0,
                         std::mt19937* rng = nullptr) {
    const double tx_power_W = std::pow(10.0, (tx_power_dBm - 30.0) / 10.0);
    const double path_loss  = friis_path_loss_linear(freq_Hz, distance_m);
    double rx_power_W = tx_power_W * path_loss;            // ← no longer const

    if (shadowing_std_dB > 0.0 && rng) {
        std::normal_distribution<double> shadow_dist(0.0, shadowing_std_dB);
        const double shadow_gain_linear = std::pow(10.0, shadow_dist(*rng) / 10.0);
        rx_power_W *= shadow_gain_linear;
    }

    const double noise_W = thermal_noise_power_W(bandwidth_Hz, noise_figure_dB);
    const double snr     = rx_power_W / noise_W;
    return snr > 0.0 ? snr : 1e-12;
}

// ---------------------------------------------------------------------------
// BER models
// ---------------------------------------------------------------------------

/** Theoretical BER for coherent QPSK / 4‑QAM. */
inline double ber_qpsk(double snr_linear) {
    return 0.5 * std::erfc(std::sqrt(snr_linear));
}

/** Generic BER for M‑QAM (approx); placeholder for future use. */
inline double ber_mqam(double snr_linear, int M) {
    // Approximate formula
    double k = std::sqrt(static_cast<double>(M));
    return (4.0 / std::log2(M)) * (1.0 - 1.0/k) * 0.5 * std::erfc(std::sqrt(3.0 * snr_linear / (M - 1.0)));
}

// ---------------------------------------------------------------------------
// Capacity
// ---------------------------------------------------------------------------

/** Shannon capacity in bits/s for AWGN channel. */
inline double shannon_capacity_bps(double bandwidth_Hz, double snr_linear) {
    return bandwidth_Hz * std::log2(1.0 + snr_linear);
}

// ---------------------------------------------------------------------------
// Outage probability
// ---------------------------------------------------------------------------

/**
 * Simplified outage probability for a block‑fading channel.
 * Returns P(SNR < threshold) using an exponential model (Rayleigh). */
inline double outage_probability(double snr_linear, double threshold_linear = 1.0) {
    return 1.0 - std::exp(-threshold_linear / snr_linear);
}

// ---------------------------------------------------------------------------
// Signal processing helpers
// ---------------------------------------------------------------------------

/** Detection probability (simple energy‑detection model). */
inline double detection_probability(double snr_linear, double threshold_factor = 1.0) {
    return 1.0 - std::exp(-snr_linear * threshold_factor);
}

/** Cramér‑Rao lower bound for time‑of‑arrival (TOA) estimation. */
inline double toa_crlb_seconds(double bandwidth_Hz, double snr_linear) {
    const double denom = 2.0 * M_PI * bandwidth_Hz * std::sqrt(2.0 * snr_linear);
    return 1.0 / std::max(denom, 1e-12);
}

/** Resample a vector by a rational ratio using nearest‑neighbour. */
inline std::vector<std::complex<float>> resample_nearest(
    const std::vector<std::complex<float>>& in,
    double input_rate, double output_rate)
{
    const double ratio = output_rate / input_rate;
    const size_t out_size = static_cast<size_t>(in.size() * ratio);
    std::vector<std::complex<float>> out(out_size);
    for (size_t i = 0; i < out_size; ++i) {
        size_t src_idx = static_cast<size_t>(i / ratio);
        if (src_idx >= in.size()) src_idx = in.size() - 1;
        out[i] = in[src_idx];
    }
    return out;
}

/** Apply frequency offset (rotation per sample) in‑place. */
inline void apply_freq_offset(std::vector<std::complex<float>>& samples,
                              double offset_hz, double sample_rate,
                              double start_phase = 0.0) {
    const double dphase = 2.0 * M_PI * offset_hz / sample_rate;
    double phase = start_phase;
    for (auto& s : samples) {
        const auto rotation = std::polar(1.0, phase);
        s = static_cast<std::complex<float>>(std::complex<double>(s) * rotation);
        phase += dphase;
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
    }
}

} // namespace sigint_sim::phy_math