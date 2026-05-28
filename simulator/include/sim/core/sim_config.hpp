#pragma once
#include <cstddef>

namespace sigint_sim {

// ============================================================================
// 1.  Mission / LPD / metrics
// ============================================================================
// Subtracted from cumulative_intelligence for every transmission attempt
inline constexpr double LPD_PENALTY_PER_TX = 0.5;      // dimensionless (mission value)

// ============================================================================
// 2.  Default emitter (used when no scenario JSON is loaded)
// ============================================================================
inline constexpr double DEFAULT_EMITTER_FREQ_HZ  = 2.4e9;   // Hz
inline constexpr double DEFAULT_EMITTER_BW_HZ    = 100e3;   // Hz
inline constexpr int    DEFAULT_EMITTER_PRIORITY = 5;       // mission value per detection
inline constexpr double DEFAULT_EMITTER_START_S  = 0.0;     // seconds

// ============================================================================
// 3.  Synthetic signal injection (simulator internal sensing)
// ============================================================================
inline constexpr int    SYNTHETIC_SIGNAL_SAMPLES = 100;     // number of complex<float> samples
inline constexpr double SYNTHETIC_SIGNAL_SNR_DB  = 15.0;    // dB (linear SNR = 10^(dB/10))

// ============================================================================
// 4.  PHY channel defaults – SampleProcessingChannel & phy_math
// ============================================================================
inline constexpr double DEFAULT_BANDWIDTH_HZ     = 1e6;     // Hz
inline constexpr double DEFAULT_SNR_THRESHOLD_DB = 5.0;     // dB, link active above this
inline constexpr double DEFAULT_BER_THRESHOLD    = 1e-3;    // bit error rate, link active below this

// Path loss / noise
inline constexpr double MIN_DISTANCE_M             = 0.01;  // m, floor for Friis (avoids div by 0)
inline constexpr double THERMAL_NOISE_FLOOR_DBM    = -174.0; // dBm/Hz (kT)
inline constexpr double ROOM_TEMPERATURE_K         = 290.0;  // K

// Outage model
inline constexpr double OUTAGE_SNR_THRESHOLD_LINEAR = 1.0;  // linear SNR for outage calc
inline constexpr double SNR_FLOOR_LINEAR            = 1e-12; // minimum linear SNR to avoid log(0)

// Per‑link buffer defaults (SampleProcessingChannel::LinkBuffer)
inline constexpr double LINK_DEFAULT_TX_SAMPLE_RATE = 1e6;  // Hz, fallback TX rate
inline constexpr double LINK_DEFAULT_DISTANCE_M      = 0.0;  // m
inline constexpr double LINK_DEFAULT_CENTER_FREQ_HZ  = 0.0;  // Hz (set per‑edge later)
inline constexpr double LINK_DEFAULT_TX_RATE         = 1e6;  // Hz
inline constexpr double LINK_DEFAULT_RX_RATE         = 1e6;  // Hz
inline constexpr double LINK_DEFAULT_FREQ_OFFSET_HZ  = 0.0;  // Hz

// ============================================================================
// 5.  Node defaults
// ============================================================================
// Compute capability (default constructor)
inline constexpr double NODE_DEFAULT_FFT_GFLOPS    = 1e9;      // GFLOPS (1 GFLOPS = 1e9 float ops/s)
inline constexpr double NODE_DEFAULT_GPU_TOPS      = 0.0;      // TOPS (0 = no GPU)
inline constexpr size_t NODE_DEFAULT_MEMORY_BYTES  = 1ull*1024*1024*1024; // 1 GiB

// Supported frequency bands (fallback)
inline constexpr double NODE_DEFAULT_BAND_LOW_HZ   = 100e6;    // Hz
inline constexpr double NODE_DEFAULT_BAND_HIGH_HZ  = 2.4e9;   // Hz

// RF parameters when none are set by the agent
inline constexpr double NODE_FALLBACK_CENTER_FREQ_HZ = 2.4e9; // Hz
inline constexpr double NODE_FALLBACK_SAMPLE_RATE_HZ = 1e6;   // Hz

// TX burst generation
inline constexpr int    NODE_TX_PILOT_SAMPLES      = 100;     // complex<float> samples per burst
inline constexpr double NODE_TX_ENERGY_INCREMENT_J = 1e-6;    // J (placeholder energy per TX)

// Processing (fallback energy calculation)
inline constexpr double NODE_PROCESSING_ENERGY_FACTOR = 1e-6; // J per FFT-op (placeholder)

// ============================================================================
// 6.  Agent defaults (RandomAgent)
// ============================================================================
// Scan action parameters
inline constexpr double AGENT_SCAN_FREQ_HZ        = 2.4e9;   // Hz
inline constexpr double AGENT_SCAN_BW_HZ          = 20e6;    // Hz
inline constexpr double AGENT_SCAN_GAIN_DB        = 40.0;    // dB
inline constexpr double AGENT_SCAN_SAMPLE_RATE_HZ = 40e6;    // Hz

// Transmission power used by RandomAgent
inline constexpr double AGENT_TX_POWER_DBM        = 20.0;    // dBm

// ============================================================================
// 7.  Signal processor task model
// ============================================================================
// Complexity factors (multiply bandwidth * duration * fft_gflops_per_sec)
inline constexpr double SP_COMPLEXITY_SCAN    = 0.1;  // relatively cheap
inline constexpr double SP_COMPLEXITY_DETECT  = 1.0;  // baseline
inline constexpr double SP_COMPLEXITY_RAW_IQ  = 0.0;  // just capturing, no compute
inline constexpr double SP_COMPLEXITY_TDOA    = 2.0;  // correlation‑heavy
inline constexpr double SP_COMPLEXITY_FEATURE = 0.5;  // moderate extraction

// Energy model
inline constexpr double SP_JOULES_PER_GFLOP  = 1.0;   // J per GFLOPS (placeholder)

// Output data sizes (bytes) when a signal is present
inline constexpr int SP_OUTPUT_SIZE_SCAN    = 0;   // no output
inline constexpr int SP_OUTPUT_SIZE_DETECT  = 1;   // one classification byte
inline constexpr int SP_OUTPUT_SIZE_FEATURE = 64;  // feature vector

// Detection probability formula parameter
inline constexpr double SP_DETECTION_THRESHOLD_FACTOR = 1.0;

// ============================================================================
// 8.  Hardware profile defaults
// ============================================================================
inline constexpr double DEFAULT_NOISE_FIGURE_DB     = 10.0;   // dB
inline constexpr double DEFAULT_TX_POWER_DBM        = 10.0;   // dBm
inline constexpr double DEFAULT_FREQ_ACCURACY_PPM   = 1.0;    // ppm
inline constexpr double DEFAULT_FFT_GFLOPS_PER_SEC  = 1.0;    // GFLOPS
inline constexpr double DEFAULT_MEMORY_MIB          = 1024.0; // MiB
inline constexpr double DEFAULT_MIN_FREQ_HZ         = 50e6;    // e.g., 50 MHz
inline constexpr double DEFAULT_MAX_FREQ_HZ         = 2.5e9; // e.g., 2.6 GHz

// ============================================================================
// 9.  Block‑fading channel (kept for backward compatibility / quick tests)
// ============================================================================
inline constexpr double BF_LINK_AVAILABILITY = 0.8;    // probability link is not in deep fade
inline constexpr double BF_AVG_SNR_DB        = 20.0;   // dB (log‑normal mean when available)
inline constexpr double BF_SNR_STD_DB        = 5.0;    // dB (log‑normal stddev)
inline constexpr double BF_OUTAGE_SNR_DB     = -10.0;  // dB (SNR below which link is down)
inline constexpr double BF_BANDWIDTH_HZ      = 10e6;   // Hz

// ============================================================================
// 10. Distance‑aware channel (alternative to BlockFading, uses node positions)
// ============================================================================
inline constexpr double DA_FREQUENCY_MHZ      = 2400.0; // MHz
inline constexpr double DA_BANDWIDTH_HZ       = 10e6;   // Hz
inline constexpr double DA_SHADOWING_STD_DB   = 3.0;    // dB
inline constexpr double DA_SNR_THRESHOLD_DB   = 5.0;    // dB

// ============================================================================
// 11. Simulator‑wide defaults
// ============================================================================
inline constexpr double SIM_DEFAULT_TIMESTEP_S = 0.1;   // seconds
inline constexpr double SIM_DEFAULT_DURATION_S = 10.0;  // seconds

// ============================================================================
// 12. Random Agent defaults
// ============================================================================
inline constexpr double RA_PROB_SCAN      = 0.10;
inline constexpr double RA_PROB_PROCESS   = 0.10;
inline constexpr double RA_PROB_TRANSMIT  = 0.70;   // 70% chance
inline constexpr double RA_PROB_IDLE      = 0.10;

// ============================================================================
// 13. Link defaults (used when constructing Link objects)
// ============================================================================
inline constexpr double DEFAULT_LINK_FREQ_HZ = 2.4e9;    // Hz (default representative band)

// ============================================================================
// 14. Logging & output
// ============================================================================
inline constexpr const char* DEFAULT_LOG_PATH = "output/sim_run.log";

} // namespace sigint_sim