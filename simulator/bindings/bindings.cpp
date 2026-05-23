#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "../include/sim/core/simulator.hpp"
#include "../include/sim/core/channel_model.hpp"
#include "../include/sim/agents/random_agent.hpp"
#include "../include/sim/logging/logger.hpp"
#include "../include/sim/core/scenario_loader.hpp"
#include "../include/sim/core/sim_config.hpp"

namespace py = pybind11;
using namespace sigint_sim;

// ---- Trampoline for IAgent -----------------------------------------------
class PyIAgent : public IAgent {
public:
    // Inherit the constructor from IAgent
    using IAgent::IAgent;

    // Trampoline for pure virtual method
    Action selectAction(const NodeState& my_state,
                        const std::vector<NodeState>& all_states,
                        const std::vector<LinkState>& links,
                        const EventLog& recent_events) override {
        PYBIND11_OVERRIDE_PURE(
            Action,          // Return type
            IAgent,          // Parent class
            selectAction,    // Function name
            my_state,        // Arguments
            all_states,
            links,
            recent_events
        );
    }

    std::string agentType() const override {
        PYBIND11_OVERRIDE(
            std::string,
            IAgent,
            agentType,
        );
    }
};

PYBIND11_MODULE(_sigint_sim_core, m) {
    m.doc() = "SIGINT simulation engine bindings";

    // ---- quick diagnostic ----
    m.def("ping", []() { return "pong"; });

    m.def("init_logger", [](const std::string& path) {
        sigint_sim::Logger::init(path);
    }, py::arg("log_file"), "Initialise the simulation log file.");

    // ---- factory for quick start (block‑fading, random agents) ----
    m.def(
        "create_default_simulator",
        [](uint64_t seed, double duration, py::list topology_list,
           double availability, double avg_snr_db) {
            Simulator::Config cfg;
            cfg.seed = seed;
            cfg.timestep = 0.1;
            cfg.duration = duration;

            for (auto item : topology_list) {
                auto tup = item.cast<py::tuple>();
                cfg.topology_edges.emplace_back(
                    NodeId{tup[0].cast<int>()},
                    NodeId{tup[1].cast<int>()}
                );
            }

            BlockFadingChannel::Params ch_params;
            ch_params.availability = availability;
            ch_params.avg_snr_db = avg_snr_db;
            ch_params.snr_std_db = 5.0;
            ch_params.outage_snr_db = -10.0;
            ch_params.bandwidth = 10e6;
            cfg.channel = std::make_shared<BlockFadingChannel>(ch_params);

            auto sim = std::make_unique<Simulator>(cfg);

            int node_count = 0;
            for (const auto& edge : cfg.topology_edges) {
                node_count = std::max(node_count,
                    std::max(static_cast<int>(edge.first),
                             static_cast<int>(edge.second)));
            }
            node_count += 1;
            for (int i = 0; i < node_count; ++i) {
                auto agent = std::make_shared<RandomAgent>(seed + i * 1000);
                sim->setAgent(NodeId{i}, std::move(agent));
            }

            std::vector<EmitterDesc> emitters;
            EmitterDesc e;
            e.id = 0;
            e.frequency_Hz   = DEFAULT_EMITTER_FREQ_HZ;
            e.bandwidth_Hz   = DEFAULT_EMITTER_BW_HZ;
            e.priority       = DEFAULT_EMITTER_PRIORITY;
            e.active_start_s = DEFAULT_EMITTER_START_S;
            e.active_end_s   = duration;
            emitters.push_back(e);
            sim->setEmitters(emitters);

            return sim;
        },
        py::arg("seed"),
        py::arg("duration"),
        py::arg("topology"),
        py::arg("availability") = 0.9,
        py::arg("avg_snr_db") = 20.0,
        "Create a Simulator with two nodes and RandomAgents."
    );

    // ---- scenario loader ----
    m.def("load_scenario",
        [](const std::string& json_path) -> std::unique_ptr<Simulator> {
            Scenario sc = sigint_sim::loadScenario(json_path);
            auto sim = std::make_unique<Simulator>(sc.config);
            sim->setEmitters(sc.emitters);
            // Attach random agents
            int node_count = 0;
            for (const auto& edge : sc.config.topology_edges) {
                node_count = std::max(node_count,
                    std::max(static_cast<int>(edge.first),
                            static_cast<int>(edge.second)));
            }
            node_count += 1;
            for (int i = 0; i < node_count; ++i) {
                auto agent = std::make_shared<RandomAgent>(sc.config.seed + i * 1000);
                sim->setAgent(NodeId{i}, agent);
            }
            return sim;
        },
        py::arg("json_path"),
        "Load a scenario JSON file and return a configured Simulator.");
    
    // ---- Trampoline Class Actions ----
    py::class_<Action::Burst>(m, "Burst")
    .def(py::init<>())
    .def_readwrite("target_node_id", &Action::Burst::target_node_id)
    .def_readwrite("phy_mode", &Action::Burst::phy_mode)
    .def_readwrite("power", &Action::Burst::power);

    py::class_<Action>(m, "Action")
        .def(py::init<>())
        .def_readwrite("scan_params", &Action::scan_params)
        .def_readwrite("process_task_ids", &Action::process_task_ids)
        .def_readwrite("burst", &Action::burst)
        .def_readwrite("stay_silent", &Action::stay_silent);

    py::class_<RFParams>(m, "RFParams")
        .def(py::init<>())
        .def_readwrite("center_freq", &RFParams::center_freq)
        .def_readwrite("bandwidth", &RFParams::bandwidth)
        .def_readwrite("gain", &RFParams::gain)
        .def_readwrite("sample_rate", &RFParams::sample_rate);
    
    // IAgent with trampoline
    py::class_<IAgent, PyIAgent, std::shared_ptr<IAgent>>(m, "IAgent")
        .def(py::init<>());

    // RandomAgent – can be instantiated from Python
    py::class_<RandomAgent, IAgent, std::shared_ptr<RandomAgent>>(m, "RandomAgent")
        .def(py::init<uint64_t>(), py::arg("seed"));

    // ---- struct wrappers (read‑only views for Python) ----
    py::class_<NodeState>(m, "NodeState")
        .def_property_readonly("id", [](const NodeState& s) { return static_cast<int>(s.id); })
        .def_readonly("name", &NodeState::name)
        .def_property_readonly("mode", [](const NodeState& s) { return static_cast<int>(s.mode); })
        .def_readonly("buffer_size", &NodeState::buffer_size)
        .def_readonly("energy_used", &NodeState::energy_used);

    py::class_<LinkState>(m, "LinkState")
        .def_property_readonly("id", [](const LinkState& l) { return static_cast<int>(l.id); })
        .def_property_readonly("from_id", [](const LinkState& l) { return static_cast<int>(l.from); })
        .def_property_readonly("to_id", [](const LinkState& l) { return static_cast<int>(l.to); })
        .def_readonly("active", &LinkState::active)
        .def_readonly("snr", &LinkState::snr)
        .def_readonly("capacity_bps", &LinkState::capacity_bps);

    py::class_<Event>(m, "Event")
        .def_readonly("time", &Event::time)
        .def_readonly("node_id", &Event::node_id)
        .def_readonly("type", &Event::type)
        .def_readonly("params", &Event::params);

    // ---- Simulator class (all methods chained) ----
    py::class_<Simulator>(m, "Simulator")
        .def("step", &Simulator::step,
             py::call_guard<py::gil_scoped_release>(),
             "Advance the simulation by one time step.")

        .def("reset", &Simulator::reset,
             py::arg("seed"),
             "Reset the simulation with a new seed.")

        .def("get_node_states", &Simulator::getNodeStates,
             "Return a list of NodeState snapshots.")

        .def("get_link_states", &Simulator::getLinkStates,
             "Return a list of LinkState snapshots.")

        .def("get_event_log", &Simulator::getEventLog,
             "Return the full event log (list of Event).")

        .def_property_readonly("current_time", &Simulator::currentTime)
        .def_property_readonly("is_finished", &Simulator::isFinished)

        .def("set_agent", &Simulator::setAgent,
            py::arg("node_id"), py::arg("agent"))

        .def("get_current_metrics", [](const Simulator& sim) {
            auto m = sim.getCurrentMetrics();
            py::dict d;
            d["cumulative_intelligence"] = m.cumulative_intelligence;
            d["lpd_violations"] = m.lpd_violations;
            d["transmissions_attempted"] = m.transmissions_attempted;
            d["transmissions_succeeded"] = m.transmissions_succeeded;
            d["average_snr"] = m.average_snr;
            d["step_execution_time_us"] = m.step_execution_time_us;
            return d;
        })
        .def("get_metrics_history", [](const Simulator& sim) {
            const auto& hist = sim.getMetricsHistory();
            py::list out;
            for (const auto& m : hist) {
                py::dict d;
                d["cumulative_intelligence"] = m.cumulative_intelligence;
                d["lpd_violations"] = m.lpd_violations;
                d["transmissions_attempted"] = m.transmissions_attempted;
                d["transmissions_succeeded"] = m.transmissions_succeeded;
                d["average_snr"] = m.average_snr;
                d["step_execution_time_us"] = m.step_execution_time_us;
                out.append(d);
            }
            return out;
        });
} //namespace sigint_sim