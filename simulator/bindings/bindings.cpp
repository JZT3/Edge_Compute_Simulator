#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "../include/sim/core/simulator.hpp"
#include "../include/sim/core/channel_model.hpp"
#include "../include/sim/agents/random_agent.hpp"
#include "../include/sim/logging/logger.hpp"
#include "../include/sim/core/scenario_loader.hpp"

namespace py = pybind11;
using namespace sigint_sim;

PYBIND11_MODULE(_sigint_sim_core, m) {
    m.doc() = "SIGINT simulation engine bindings";

    // ---- quick diagnostic ----
    m.def("ping", []() { return "pong"; });

    // ---- factory function (inlined lambda to guarantee export) ----
    m.def(
        "create_default_simulator",
        [](uint64_t seed, double duration, py::list topology_list,
           double availability, double avg_snr_db) {
            Simulator::Config cfg;
            cfg.seed = seed;
            cfg.timestep = 0.1;
            cfg.duration = duration;

            // Convert Python list of (from,to) tuples
            for (auto item : topology_list) {
                auto tup = item.cast<py::tuple>();
                cfg.topology_edges.emplace_back(
                    NodeId{tup[0].cast<int>()},
                    NodeId{tup[1].cast<int>()}
                );
            }

            // Channel
            BlockFadingChannel::Params ch_params;
            ch_params.availability = availability;
            ch_params.avg_snr_db = avg_snr_db;
            ch_params.snr_std_db = 5.0;
            ch_params.outage_snr_db = -10.0;
            ch_params.bandwidth = 10e6;
            cfg.channel = std::make_shared<BlockFadingChannel>(ch_params);

            auto sim = std::make_unique<Simulator>(cfg);

            // Attach random agents with deterministic seeds derived from master seed
            int node_count = 0;
            for (const auto& edge : cfg.topology_edges) {
                node_count = std::max(node_count,
                    std::max(static_cast<int>(edge.first),
                             static_cast<int>(edge.second)));
            }
            node_count += 1;
            for (int i = 0; i < node_count; ++i) {
                auto agent = std::make_unique<RandomAgent>(seed + i * 1000);
                sim->setAgent(NodeId{i}, std::move(agent));
            }

            return sim;
        },
        py::arg("seed"),
        py::arg("duration"),
        py::arg("topology"),
        py::arg("availability") = 0.9,
        py::arg("avg_snr_db") = 20.0,
        "Create a Simulator with two nodes and RandomAgents."
    );

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
    }
