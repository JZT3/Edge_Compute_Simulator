#include "../include/sim/core/scenario_loader.hpp"
#include "sim/core/sample_processing_channel.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace sigint_sim {

Scenario loadScenario(const std::string& jsonFilePath) {
    std::ifstream ifs(jsonFilePath);
    if (!ifs) throw std::runtime_error("Cannot open scenario file: " + jsonFilePath);
    nlohmann::json j;
    ifs >> j;

    Scenario sc;
    auto& cfg = sc.config;

    cfg.seed = j.at("seed");
    cfg.timestep = j.value("timestep", 0.1);
    cfg.duration = j.at("duration");

    for (const auto& edge : j.at("topology_edges")) {
        cfg.topology_edges.emplace_back(NodeId{edge[0].get<int>()}, NodeId{edge[1].get<int>()});
    }

    std::string channelType = j.value("channel_type", "BlockFading");
    if (channelType == "SampleProcessing") {
        SampleProcessingChannel::Params chp;
        if (j.contains("channel_params")) {
            chp.bandwidth_Hz = j["channel_params"].value("bandwidth_Hz", 10e6);
            chp.snr_threshold_dB = j["channel_params"].value("snr_threshold_dB", 5.0);
            chp.ber_threshold = j["channel_params"].value("ber_threshold", 1e-3);
        }
        cfg.channel = std::make_shared<SampleProcessingChannel>(chp);
    } else {
        // fallback to BlockFading (you can extend)
        throw std::runtime_error("Unsupported channel type: " + channelType);
    }

    if (j.contains("node_profiles")) {
        for (const auto& [key, val] : j["node_profiles"].items()) {
            int id = std::stoi(key);
            HardwareProfile prof;
            std::string dtype = val.value("type", "RTL_SDR");
            if (dtype == "RTL_SDR") prof.type = DeviceType::RTL_SDR;
            else if (dtype == "USRP_B2XX") prof.type = DeviceType::USRP_B2XX;
            else if (dtype == "HACKRF") prof.type = DeviceType::HACKRF;
            else prof.type = DeviceType::VIRTUAL;
            prof.noise_figure_dB = val.value("noise_figure_dB", 10.0);
            prof.tx_power_dBm = val.value("tx_power_dBm", 10.0);
            prof.frequency_accuracy_ppm = val.value("frequency_accuracy_ppm", 1.0);
            prof.fft_gflops_per_sec = val.value("fft_gflops_per_sec", 1.0);
            prof.memory_mib = val.value("memory_mib", 1024.0);
            cfg.node_profiles[id] = prof;
        }
    }

    if (j.contains("node_positions")) {
        for (const auto& [key, val] : j["node_positions"].items()) {
            int id = std::stoi(key);
            cfg.node_positions[id] = {val[0].get<double>(), val[1].get<double>()};
        }
    }

    if (j.contains("emitters")) {
        for (const auto& ej : j["emitters"]) {
            EmitterDesc e;
            e.id = ej.at("id");
            e.frequency_Hz = ej.at("frequency_Hz");
            e.bandwidth_Hz = ej.value("bandwidth_Hz", 1e5);
            e.priority = ej.value("priority", 1);
            e.modulation = ej.value("modulation", "qpsk");
            e.active_start_s = ej.value("active_start_s", 0.0);
            e.active_end_s = ej.value("active_end_s", 5.0);
            sc.emitters.push_back(e);
        }
    }

    return sc;
}

} // namespace sigint_sim