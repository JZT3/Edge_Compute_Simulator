"""Main application window for the SIGINT simulation GUI."""
from __future__ import annotations
import logging
from pathlib import Path
from typing import Optional

from ..utils.logging_setup import setup_logging
setup_logging(log_file="sim_gui.log", level=logging.INFO)


from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtWidgets import (
    QAction,
    QDockWidget,
    QFileDialog,
    QMainWindow,
    QMenu,
    QMessageBox,
    QStatusBar,
    QToolBar,
    QWidget,
    QVBoxLayout,
)

from ..models import SimulatorConfig
from ..simulator_wrapper import Simulator
from ..utils.logging_setup import setup_logging
from .network_graph import NetworkGraphView

logger = logging.getLogger(__name__)

# Fixed bounds for the simulation timer
MAX_STEP_INTERVAL_MS: int = 200
MIN_STEP_INTERVAL_MS: int = 10


class MainWindow(QMainWindow):
    """Top‑level window that owns the simulator, controller, and all views."""

    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("SIGINT Simulator – Game‑Theoretic Orchestrator")
        self.resize(1200, 800)

        # Simulator (created later via "New Scenario")
        self._sim: Optional[Simulator] = None
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._on_tick)
        self._step_interval_ms: int = 100

        # Build UI pieces
        self._create_actions()
        self._create_menu()
        self._create_toolbar()
        self._create_status_bar()
        self._create_central_widget()
        self._create_dock_widgets()

        # Start with a default scenario for convenience
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

    # ------------------------------------------------------------------
    # Central widget (network graph)
    # ------------------------------------------------------------------

    def _create_central_widget(self) -> None:
        self._graph_view = NetworkGraphView()
        central = QWidget()
        layout = QVBoxLayout(central)
        layout.addWidget(self._graph_view)
        layout.setContentsMargins(0, 0, 0, 0)
        self.setCentralWidget(central)

    # ------------------------------------------------------------------
    # Dock widgets (parameter panel, charts – stubs for now)
    # ------------------------------------------------------------------

    def _create_dock_widgets(self) -> None:
        # Parameter panel dock (placeholder)
        param_dock = QDockWidget("Parameters", self)
        param_dock.setWidget(QWidget())  # will be replaced in later phase
        self.addDockWidget(Qt.RightDockWidgetArea, param_dock)

        # Time‑series chart dock (placeholder)
        chart_dock = QDockWidget("Intelligence Over Time", self)
        chart_dock.setWidget(QWidget())  # will be replaced in later phase
        self.addDockWidget(Qt.BottomDockWidgetArea, chart_dock)

    # ------------------------------------------------------------------
    # Scenario management
    # ------------------------------------------------------------------

    def _new_scenario(self) -> None:
        """Create a default simulation and display it."""
        config = SimulatorConfig()
        self._load_simulator(config)

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
        """Replace the current simulator with a new one."""
        if self._timer.isActive():
            self._timer.stop()
        if self._sim is not None:
            self._sim.close()

        assert isinstance(config, SimulatorConfig)
        self._sim = Simulator(config)
        self._refresh_view()
        self._status.showMessage(
            f"Scenario loaded: {len(config.topology)} nodes, "
            f"seed={config.seed}, duration={config.duration:.1f}s"
        )

    def _play(self) -> None:
        assert self._sim is not None, "No simulator loaded"
        self._timer.start(self._step_interval_ms)
        self._status.showMessage("Running…")

    def _pause(self) -> None:
        self._timer.stop()
        self._status.showMessage("Paused")

    def _step(self) -> None:
        assert self._sim is not None, "No simulator loaded"
        self._timer.stop()  # pause if running
        events = self._sim.step(1)
        self._refresh_view()
        self._status.showMessage(
            f"Time: {self._sim.current_time:.2f}s, "
            f"Events: {len(events)}"
        )

    def _reset(self) -> None:
        if self._sim is None:
            return
        self._timer.stop()
        self._sim.reset(self._sim._config.seed)
        self._refresh_view()
        self._status.showMessage("Reset to t=0")

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
        self._graph_view.update_state(nodes, links, self._sim.current_time)

    def _export_graph_png(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self, "Export Graph", "graph.png", "PNG Files (*.png)"
        )
        if path:
            self._graph_view.export_png(path)
            self._status.showMessage(f"Graph exported to {path}")