"""Main application window for the SIGINT simulation GUI."""
from __future__ import annotations
import sys
import logging
from pathlib import Path
from typing import Optional

from ..controller import SimulatorController
from ..utils.logging_setup import setup_logging
setup_logging(log_file="sim_gui.log", level=logging.INFO)


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

from ..models import SimulatorConfig
from ..simulator_wrapper import Simulator
from ..utils.logging_setup import setup_logging
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
        self.setWindowTitle("SIGINT Simulator – Game‑Theoretic Orchestrator")
        self.setMinimumHeight(600)
        self.setMinimumWidth(800)
        self.resize(1200, 900)    

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
            self._new_scenario()          # uses factory (create_default_simulator)
        else:
            # Let user pick a JSON file from the scenarios/ folder
            path, _ = QFileDialog.getOpenFileName(
                self, "Open Scenario JSON", "scenarios", "JSON Files (*.json)")
            if path:
                self._load_json_scenario(path)
            else:
                self._new_scenario()

    # ------------------------------------------------------------------
    # Actions, menus, toolbar
    # ------------------------------------------------------------------

    def _create_actions(self) -> None:
        self._act_new = QAction("&New Scenario", self)
        self._act_new.triggered.connect(self._new_scenario)

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
        
    def _on_metrics_updated(self, metrics: dict) -> None:
        # print("DEBUG: metrics received:", metrics)   

        sim_time = self._sim.current_time if self._sim else 0.0
        mission = metrics.get("cumulative_intelligence", 0.0)
        attempted = metrics.get("transmissions_attempted", 0)
        succeeded = metrics.get("transmissions_succeeded", 0)
        rate = (succeeded / attempted * 100.0) if attempted > 0 else 0.0

        self._metrics_history.append((sim_time, mission, rate))
        
        # print("DEBUG: history length:", len(self._metrics_history))

        # Update chart every 5 points
        if len(self._metrics_history) % 5 == 0:
            times, missions, rates = zip(*self._metrics_history)
            # print("DEBUG: chart update with", len(times), "points")
            self._chart.update_data(list(times), list(missions), list(rates))

        # Update status bar
        self._status.showMessage(
            f"Time: {sim_time:.2f}s, Mission: {mission:.1f}, "
            f"TX Success: {rate:.0f}%, LPD viol: {metrics.get('lpd_violations',0)}"
        )

    def _on_sim_finished(self) -> None:
        self._status.showMessage("Simulation finished")
        
        if not self._metrics_history:
            return
        
        times, missions, rates = zip(*self._metrics_history)
        total_intel = missions[-1] if missions else 0.0
        avg_success = sum(rates) / len(rates) if rates else 0.0
        
        msg = (f"Simulation finished.\n"
            f"Total Mission Value: {total_intel:.1f}\n"
            f"Average TX Success Rate: {avg_success:.0f}%\n"
            f"LPD Violations: {self._sim.get_metrics().get('lpd_violations',0)}")
        
        reply = QMessageBox.information(self, "Run Summary", msg,
                                        QMessageBox.Ok | QMessageBox.Save)
        
        if reply == QMessageBox.Save:
            # Save metrics to CSV
            import csv
            path, _ = QFileDialog.getSaveFileName(
                        self, 
                        "Save Run Data", 
                        str(DEFAULT_OUTPUT_DIR / "run_data.csv"),
                        "CSV Files (*.csv)"
                    )

            if path:
                with open(path, 'w', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerow(["Time_s", "Mission_Value", "TX_Success_Rate"])
                    for t, m, r in self._metrics_history:
                        writer.writerow([t, m, r])
                self._status.showMessage(f"Data saved to {path}")
            pass

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
        self._param_dock = QDockWidget("Scenario Parameters", self)
        self._param_dock.setWidget(self._param_panel)
        self._param_dock.setMinimumWidth(300)
        self.addDockWidget(Qt.RightDockWidgetArea, self._param_dock)

        # Time-series chart (bottom)
        self._chart = TimeSeriesChart("Mission Intelligence")
        self._chart_dock = QDockWidget("Intelligence Over Time", self)
        self._chart_dock.setWidget(self._chart)
        self._chart_dock.setMinimumHeight(200)
        self.addDockWidget(Qt.BottomDockWidgetArea, self._chart_dock)

        
    def _on_config_changed(self, config: SimulatorConfig) -> None:
        self._load_simulator(config)

    # ------------------------------------------------------------------
    # Scenario management
    # ------------------------------------------------------------------

    def _new_scenario(self) -> None:
        # Adjust the path to your actual file
        scenario_path = "scenarios/s1_two_node.json"
        self._controller.load_json(scenario_path)
        self._sim = self._controller.sim
        self._metrics_history.clear()
        # Fetch initial state and display
        nodes = self._sim.get_node_states()
        links = self._sim.get_link_states()
        self._graph_view.update_state(nodes, links, self._sim.current_time)
        self._controller.start()

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

    def _load_simulator(self, config: SimulatorConfig) -> None:
        """Stop previous simulation, load a new config, and start the background thread."""
        if self._controller is not None:
            self._controller.stop()

        self._controller.load_scenario(config)
        self._sim = self._controller.sim
        
        if self._sim is not None:
            nodes = self._sim.get_node_states()
            links = self._sim.get_link_states()
            self._graph_view.update_state(nodes, links, self._sim.current_time)         

        self._metrics_history.clear()      
        self._controller.start()

        # The graph will be refreshed when the first state_ready signal arrives.
        self._status.showMessage(
            f"Scenario loaded: {len(config.topology_nodes())} nodes, "
            f"seed={config.seed}, duration={config.duration:.1f}s"
        )
        
    def _load_json_scenario(self, json_path: str) -> None:
        """Load a scenario from a JSON file and start it."""
        self._controller.load_json(json_path)
        self._sim = self._controller.sim
        self._metrics_history.clear()
        nodes = self._sim.get_node_states()
        links = self._sim.get_link_states()
        self._graph_view.update_state(nodes, links, self._sim.current_time)
        self._controller.start()

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
        self._chart.update_data([], [], [])
        
        # 4. Fetch initial state and update graph
        nodes = self._sim.get_node_states()
        links = self._sim.get_link_states()
        self._graph_view.update_state(nodes, links, self._sim.current_time)
        
        # 5. Restart the controller
        self._controller.start()
        self._status.showMessage("Reset to t=0")
        
    def closeEvent(self, event):
        # Stop the simulation controller and wait for the thread
        if hasattr(self, '_controller') and self._controller is not None:
            self._controller.stop()
            # Give the thread a moment to finish
            QThread.msleep(100)
        event.accept()

    def _on_tick(self) -> None:
        """Called by the timer at each interval."""
        assert self._sim is not None, "Timer ticked without a simulator"
        if self._sim.is_finished:
            self._timer.stop()
            self._status.showMessage("Simulation finished")
            return
        events = self._sim.step(1)
        self._refresh_view()
        self._status.showMessage(
            f"Time: {self._sim.current_time:.2f}s"
        )

    # ------------------------------------------------------------------
    # View refresh & export
    # ------------------------------------------------------------------

    def _refresh_view(self) -> None:
        """Pull state from the simulator and update the graph."""
        assert self._sim is not None, "Cannot refresh without simulator"
        nodes = self._sim.get_node_states()
        links = self._sim.get_link_states()
        print(f"Refresh: {len(nodes)} nodes, {len(links)} links")  # DEBUG
        self._graph_view.update_state(nodes, links, self._sim.current_time)

    def _export_graph_png(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self, "Export Graph", "graph.png", "PNG Files (*.png)"
        )
        if path:
            self._graph_view.export_png(path)
            self._status.showMessage(f"Graph exported to {path}")