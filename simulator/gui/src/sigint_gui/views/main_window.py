"""Main application window for the SIGINT simulation GUI."""
from __future__ import annotations
import sys
import logging
from pathlib import Path
from typing import Optional

from ..utils.run_logger import RunLogger
from ..controller import SimulatorController
from ..utils.logging_setup import setup_logging
setup_logging(log_file="sim_gui.log", level=logging.INFO)

from datetime import datetime
import pandas as pd

from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QPalette, QColor
from PyQt5.QtWidgets import (
    QAction,
    QApplication,
    QDockWidget,
    QFileDialog,
    QMainWindow,
    QMenu,
    QMessageBox,
    QStatusBar,
    QToolBar,
    QWidget,
    QVBoxLayout,
    QSizePolicy,
    QInputDialog,
)

from sigint_gui import _sigint_sim_core as _core

from ..models import SimulatorConfig
from ..simulator_wrapper import Simulator
from ..utils.output import DEFAULT_OUTPUT_DIR
from .network_graph import NetworkGraphView
from .parameter_panel import ParameterPanel
from .time_series_chart import TimeSeriesChart

logger = logging.getLogger(__name__)

# Fixed bounds for the simulation timer
MAX_STEP_INTERVAL_MS: int = 200
MIN_STEP_INTERVAL_MS: int = 10

def apply_dark_theme(app: QApplication) -> None:
    """Force a dark colour palette on the entire application."""
    app.setStyle("Fusion")
    dark_palette = QPalette()
    dark_palette.setColor(QPalette.Window, QColor(53, 53, 53))
    dark_palette.setColor(QPalette.WindowText, Qt.white)
    dark_palette.setColor(QPalette.Base, QColor(25, 25, 25))
    dark_palette.setColor(QPalette.AlternateBase, QColor(53, 53, 53))
    dark_palette.setColor(QPalette.ToolTipBase, Qt.white)
    dark_palette.setColor(QPalette.ToolTipText, Qt.white)
    dark_palette.setColor(QPalette.Text, Qt.white)
    dark_palette.setColor(QPalette.Button, QColor(53, 53, 53))
    dark_palette.setColor(QPalette.ButtonText, Qt.white)
    dark_palette.setColor(QPalette.BrightText, Qt.red)
    dark_palette.setColor(QPalette.Link, QColor(42, 130, 218))
    dark_palette.setColor(QPalette.Highlight, QColor(42, 130, 218))
    dark_palette.setColor(QPalette.HighlightedText, Qt.black)
    app.setPalette(dark_palette)
class MainWindow(QMainWindow):
    """Top‑level window that owns the simulator, controller, and all views."""

    def __init__(self) -> None:
        super().__init__()
        self._run_logger: Optional[RunLogger] = None
        self.setWindowTitle("SIGINT Simulator – Game‑Theoretic Orchestrator")
        self.setMinimumHeight(600)
        self.setMinimumWidth(800)
        self.resize(1200, 900)
        self._last_metrics = None
        self._emitters_data: list = []

        # Simulator (created later via "New Scenario")
        self._sim: Optional[Simulator] = None
        self._step_interval_ms: int = 100

        # 1. Create UI pieces that don't depend on the controller
        self._create_actions()
        self._create_menu()
        self._create_toolbar()
        self._create_status_bar()
        self._create_central_widget()
        self._create_dock_widgets()   

        # 2. Set up the controller (before any scenario is loaded)
        self._controller = SimulatorController()
        self._controller.state_updated.connect(self._on_state_updated)
        self._controller.finished.connect(self._on_sim_finished)
        self._controller.error_occurred.connect(self._on_sim_error)
        self._controller.metrics_updated.connect(self._on_metrics_updated)
        self._metrics_history: list[tuple[float, float]] = []
        
        choice, ok = QInputDialog.getItem(
            self, "Select Scenario Source",
            "Choose initial scenario:",
            ["Default (built-in)", "Load from JSON file..."], 0, False)
        
        if not ok:
            sys.exit(0)   # user cancelled

        if choice == "Default (built-in)":
            self._choose_startup_scenario()          # uses factory (create_default_simulator)
        else:
            # Let user pick a JSON file from the scenarios/ folder
            path, _ = QFileDialog.getOpenFileName(
                self, "Open Scenario JSON", "scenarios", "JSON Files (*.json)")
            if path:
                self._start_scenario(path)
            else:
                self._choose_startup_scenario()

    # ------------------------------------------------------------------
    # Actions, menus, toolbar
    # ------------------------------------------------------------------

    def _create_actions(self) -> None:
        self._act_new = QAction("&New Scenario", self)
        self._act_new.triggered.connect(self._choose_startup_scenario)

        self._act_load = QAction("&Load Config…", self)
        self._act_load.triggered.connect(self._load_config)

        self._act_save = QAction("&Save Config…", self)
        self._act_save.triggered.connect(self._save_config)

        self._act_play = QAction("▶ Play", self)
        self._act_play.triggered.connect(self._play)

        self._act_pause = QAction("⏸ Pause", self)
        self._act_pause.triggered.connect(self._pause)

        self._act_step = QAction("⏭ Step", self)
        self._act_step.triggered.connect(self._step)

        self._act_reset = QAction("↺ Reset", self)
        self._act_reset.triggered.connect(self._reset)

        self._act_export_png = QAction("Export Graph as PNG", self)
        self._act_export_png.triggered.connect(self._export_graph_png)

    def _create_menu(self) -> None:
        menu = self.menuBar()
        file_menu: QMenu = menu.addMenu("&File")
        file_menu.addAction(self._act_new)
        file_menu.addAction(self._act_load)
        file_menu.addAction(self._act_save)
        file_menu.addSeparator()
        file_menu.addAction(self._act_export_png)

        sim_menu: QMenu = menu.addMenu("&Simulation")
        sim_menu.addAction(self._act_play)
        sim_menu.addAction(self._act_pause)
        sim_menu.addAction(self._act_step)
        sim_menu.addAction(self._act_reset)
        sim_menu.addAction("Load Scenario", self._load_scenario)
        
        #view menu
        view_menu = self.menuBar().addMenu("&View")
        if hasattr(self, '_param_dock'):
            view_menu.addAction(self._param_dock.toggleViewAction())
        if hasattr(self, '_chart_dock'):
            view_menu.addAction(self._chart_dock.toggleViewAction())

    def _create_toolbar(self) -> None:
        toolbar: QToolBar = self.addToolBar("Simulation")
        toolbar.addAction(self._act_play)
        toolbar.addAction(self._act_pause)
        toolbar.addAction(self._act_step)
        toolbar.addAction(self._act_reset)

    def _create_status_bar(self) -> None:
        self._status = QStatusBar()
        self.setStatusBar(self._status)
        self._status.showMessage("Ready")
        
    def _on_state_updated(self, nodes: list, links: list, sim_time: float) -> None:
        """Refresh the network graph and any other views."""
        self._graph_view.update_state(nodes, links, sim_time)
        
    def _on_metrics_updated(self, metrics: dict, sim_time: float) -> None:
        self._last_metrics = metrics
        logger.debug("DEBUG: metrics received:", metrics)   
        # Update the time‑series chart using sim_time as the X coordinate
        # self._chart_view.add_data_point(sim_time, metrics.get("cumulative_intelligence", 0))
        raw = metrics.get("raw_intelligence", 0.0)
        lpd = metrics.get("lpd_penalty_total", 0.0)
        net = metrics.get("cumulative_intelligence", 0.0)
        attempted = metrics.get("transmissions_attempted", 0)
        succeeded = metrics.get("transmissions_succeeded", 0)
        rate = (succeeded / attempted * 100.0) if attempted > 0 else 0.0

        self._metrics_history.append((sim_time, raw, lpd, net, rate))
        
        logger.debug(f"DEBUG: history length:", len(self._metrics_history))
         
         # Log to file
        if self._run_logger is not None:
            self._run_logger.log(sim_time, raw, lpd, net, rate)

        # Update chart every 5 points
        if len(self._metrics_history) % 5 == 0:
            times, raw_l, lpd_l, net_l, rate_l = zip(*self._metrics_history)
            logger.debug(f"DEBUG: chart update with", len(times), "points")
            self._chart_widget.update_data(list(times), list(raw_l),
                                           list(lpd_l), list(net_l), list(rate_l))

        # Update status bar
        self._status.showMessage(
            f"Time: {sim_time:.2f}s | Raw: {raw:.1f} | Net: {net:.1f} | "
            f"TX: {rate:.0f}% | LPD: {lpd:.1f}"
        )

    def _on_sim_finished(self) -> None:
        self._status.showMessage("Simulation finished")

        if not self._metrics_history:
            return

        # Unpack the 5‑element tuples now stored in history
        times, raw_l, lpd_l, net_l, rate_l = zip(*self._metrics_history)
        total_intel = net_l[-1] if net_l else 0.0
        avg_success = sum(rate_l) / len(rate_l) if rate_l else 0.0

        # Get LPD violations from the last received metrics (stored above)
        lpd_violations = 0
        if self._last_metrics is not None:
            lpd_violations = self._last_metrics.get("lpd_violations", 0)

        msg = (
            f"Simulation finished.\n"
            f"Total Net Mission Value: {total_intel:.1f}\n"
            f"Average TX Success Rate: {avg_success:.0f}%\n"
            f"LPD Violations: {lpd_violations}"
        )

        reply = QMessageBox.information(self, "Run Summary", msg,
                                        QMessageBox.Ok | QMessageBox.Save)

        if reply == QMessageBox.Save:
            import csv
            from pathlib import Path
            path, _ = QFileDialog.getSaveFileName(
                self,
                "Save Run Data",
                str(DEFAULT_OUTPUT_DIR / "run_data.csv"),
                "CSV Files (*.csv)"
            )
            if path:
                with open(path, 'w', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerow(["Time_s", "Raw_Intel", "LPD_Penalty",
                                    "Net_Mission", "TX_Success_Rate"])
                    for t, r, l, n, s in zip(times, raw_l, lpd_l, net_l, rate_l):
                        writer.writerow([t, r, l, n, s])
                self._status.showMessage(f"Data saved to {path}")

    def _on_sim_error(self, msg: str) -> None:
        QMessageBox.critical(self, "Simulation Error", msg)

    # ------------------------------------------------------------------
    # Central widget (network graph)
    # ------------------------------------------------------------------

    def _create_central_widget(self) -> None:
        """Only the network graph lives in the central area."""
        self._graph_view = NetworkGraphView()
        self._graph_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.setCentralWidget(self._graph_view)

    # ------------------------------------------------------------------
    # Dock widgets (parameter panel, charts – stubs for now)
    # ------------------------------------------------------------------

    def _create_dock_widgets(self) -> None:
        # Parameter panel (right side)
        self._param_panel = ParameterPanel()
        self._param_panel.config_changed.connect(self._on_config_changed)
        self._param_dock = QDockWidget("Scenario Parameters", self)
        self._param_dock.setWidget(self._param_panel)
        self._param_dock.setMinimumWidth(300)
        self.addDockWidget(Qt.RightDockWidgetArea, self._param_dock)

        # Time-series chart (bottom)
        self._chart_widget = TimeSeriesChart()
        self._chart_widget.setWindowTitle("Intelligence Over Time")    
        self._chart_dock = QDockWidget("Intelligence Over Time", self)
        self._chart_dock.setWidget(self._chart_widget)
        self._chart_dock.setMinimumHeight(200)
        self.addDockWidget(Qt.BottomDockWidgetArea, self._chart_dock)

        
    def _on_config_changed(self, config: SimulatorConfig, agent_type: str) -> None:
        if self._last_scenario_data is not None:
            self._last_scenario_data["agent_type"] = agent_type
            self._start_scenario(self._last_scenario_data, agent_type)
        else:
            # Fallback for when no scenario has been loaded yet (shouldn't happen)
            self._start_scenario({
                "seed": config.seed, "duration": config.duration,
                "topology_edges": config.topology,
                "availability": config.availability, "avg_snr_db": config.avg_snr_db,
                "node_positions": {}, "node_profiles": {}, "emitters": [],
                "agent_type": agent_type
            })
    # ------------------------------------------------------------------
    # Scenario management
    # ------------------------------------------------------------------

    def _choose_startup_scenario(self) -> None:
        """Called once at application startup to let the user pick a scenario."""
        choice, ok = QInputDialog.getItem(
            self, "Startup Scenario",
            "Choose a scenario to start:",
            ["Default (built-in)", "Load from JSON…"], 0, False)
        if not ok:
            choice = "Default (built-in)"

        if choice == "Default (built-in)":
            # Use a hardcoded minimal scenario (same as before)
            default_data = {
                "seed": 42, "duration": 10.0, "timestep": 0.1,
                "topology_edges": [(0,1),(1,0)],
                "node_positions": {"0": [0.0,0.0], "1": [100.0,0.0]},
                "node_profiles": {},
                "emitters": [],
                "agent_type": "Random"
            }
            self._start_scenario(default_data)
        else:
            path, _ = QFileDialog.getOpenFileName(
                self, "Open Scenario JSON", "scenarios", "JSON Files (*.json)")
            if path:
                self._start_scenario(path)
            else:
                # User cancelled → fall back to default
                default_data = {
                    "seed": 42, "duration": 10.0, "timestep": 0.1,
                    "topology_edges": [(0,1),(1,0)],
                    "node_positions": {"0": [0.0,0.0], "1": [100.0,0.0]},
                    "node_profiles": {},
                    "emitters": [],
                    "agent_type": "Random"
                }
                self._start_scenario(default_data)

    def _load_config(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "Load Config", "", "JSON Files (*.json)"
        )
        if not path:
            return
        try:
            import json
            data = json.loads(Path(path).read_text())
            config = SimulatorConfig(**data)
            self._load_simulator(config)
        except Exception as exc:
            QMessageBox.critical(self, "Error", f"Failed to load config:\n{exc}")

    def _save_config(self) -> None:
        if self._sim is None:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "Save Config", "scenario.json", "JSON Files (*.json)"
        )
        if not path:
            return
        import json
        cfg = self._sim._config
        data = {
            "seed": cfg.seed,
            "duration": cfg.duration,
            "topology": cfg.topology,
            "availability": cfg.availability,
            "avg_snr_db": cfg.avg_snr_db,
            "timestep": cfg.timestep,
        }
        Path(path).write_text(json.dumps(data, indent=2))
        self._status.showMessage(f"Config saved to {path}")

    # ------------------------------------------------------------------
    # Simulator control
    # ------------------------------------------------------------------
    
    def _start_scenario(self, source, agent_type: str = "Random") -> None:
        """Stop any running simulation, create a new one from *data* with *agent_type*,
        and start it.  *data* must contain all scenario keys (seed, duration,
        topology_edges, node_positions, node_profiles, emitters, …)."""
        import json, os, tempfile

        # 1. Resolve source → dict
        if isinstance(source, str):
            # source is a file path
            with open(source) as f:
                data = json.load(f)
            self._last_scenario_path = source
        else:
            data = source

        self._last_scenario_data = data

        # 2. Determine agent type (prefer the dict, else use the argument)
        agent_type = data.get("agent_type", agent_type)

        # 3. Stop old run, create controller if needed
        if self._controller is not None:
            self._controller.stop()
        else:
            self._controller = SimulatorController()
            self._controller.state_updated.connect(self._on_state_updated)
            self._controller.finished.connect(self._on_sim_finished)
            self._controller.error_occurred.connect(self._on_sim_error)

        # 4. Write dict to temp file (the C++ binding reads from disk)
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as tmp:
            json.dump(data, tmp)
            tmp_path = tmp.name
        try:
            cpp_sim = _core.load_scenario_with_agent(tmp_path, agent_type)
        finally:
            os.unlink(tmp_path)

        # 5. Wrap in Python Simulator and hand to controller
        from sigint_gui.simulator_wrapper import Simulator as PySimulator
        config = SimulatorConfig(
            seed=data.get("seed", 42),
            duration=data.get("duration", 10.0),
            topology=data.get("topology_edges", [(0,1),(1,0)]),
            availability=data.get("availability", 0.9),
            avg_snr_db=data.get("avg_snr_db", 20.0))
        py_sim = PySimulator.from_existing(cpp_sim, config)
        self._controller.set_simulator(py_sim)

        # 6. Update emitters on graph
        self._graph_view.reset_view()
        emitters = data.get("emitters", [])
        print(f"Setting {len(emitters)} emitters with positions:")
        for e in emitters:
            print(f"  Emitter {e.get('id')}: ({e.get('x')}, {e.get('y')})")
        self._graph_view.set_emitters(emitters)

        # 7. Update parameter panel
        if hasattr(self, '_param_panel'):
            self._param_panel.load_from_dict(data)

        # 8. Reset visuals
        self._metrics_history.clear()
        self._chart_widget.reset_chart()

        # 9. Show initial state & start
        emitters = data.get("emitters", [])
        self._graph_view.set_emitters(emitters)
        
        nodes = py_sim.get_node_states()
        links = py_sim.get_link_states()
        self._graph_view.update_state(nodes, links, 0.0)
        self._controller.start()
        self._status.showMessage(f"Running: {agent_type}, {len(nodes)} nodes")
        
    def _load_scenario(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Load Scenario", "", "JSON Files (*.json)")
        if not path:
            return
        import json
        data = json.loads(Path(path).read_text())
        self._last_scenario_data = data          # save for later agent changes
        agent_type = data.get("agent_type", "Random")
        self._start_scenario(data, agent_type)

    def _play(self) -> None:
        if self._controller is None:
            return
        self._controller.play()
        self._status.showMessage("Running…")

    def _pause(self) -> None:
        if self._controller is None:
            return
        self._controller.pause()
        self._status.showMessage("Paused")

    def _step(self) -> None:
        if self._controller is None:
            return
        # Step forces a single step; controller will emit state_updated afterwards
        self._controller.step()

    def _reset(self) -> None:
        if self._sim is None or self._controller is None:
            return
        
        # 1. Stop running simulation
        self._controller.stop()
        
        # 2. Reset the native simulator (time, rng)
        seed = self._sim._config.seed if self._sim._config else 42
        self._sim.reset(seed)
        
        # 3. Clear metrics and chart
        self._metrics_history.clear()
        self._chart_widget.reset_chart()

        # 4. Fetch initial state and update graph
        nodes = self._sim.get_node_states()
        links = self._sim.get_link_states()
        self._graph_view.update_state(nodes, links, self._sim.current_time)
        
        # 5. Restart the controller
        self._controller.start()
        self._status.showMessage("Reset to t=0")
        
    def closeEvent(self, event):
        """Stop the simulation thread gracefully before closing."""
        if self._run_logger is not None:
            self._run_logger.close()
            
        if self._controller is not None:
            self._controller.stop()
            self._controller = None
        self._graph_view.clear()
        event.accept()

    # def _on_tick(self) -> None:
    #     """Called by the timer at each interval."""
    #     assert self._sim is not None, "Timer ticked without a simulator"
    #     if self._sim.is_finished:
    #         self._timer.stop()
    #         self._status.showMessage("Simulation finished")
    #         return
    #     events = self._sim.step(1)
    #     self._refresh_view()
    #     self._status.showMessage(
    #         f"Time: {self._sim.current_time:.2f}s"
    #     )

    # ------------------------------------------------------------------
    # View refresh & export
    # ------------------------------------------------------------------

    def _export_graph_png(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self, "Export Graph", "graph.png", "PNG Files (*.png)"
        )
        if path:
            self._graph_view.export_png(path)
            self._status.showMessage(f"Graph exported to {path}")
            
    # def _start_worker(self) -> None:
    #     """Internal: create and start the background worker thread."""
    #     assert self._sim is not None, "No simulator loaded"
    #     self._thread = QThread()
    #     self._worker = _SimulationWorker(self._sim)
    #     self._worker.moveToThread(self._thread)

    #     self._thread.started.connect(self._worker.run_loop)
    #     self._worker.finished.connect(self._thread.quit)
    #     self._worker.finished.connect(self._on_finished)
    #     self._worker.error_occurred.connect(self._on_error)
    #     self._worker.state_ready.connect(self._on_state_ready)
    #     self._thread.finished.connect(self._thread.deleteLater)

    #     self._thread.start()