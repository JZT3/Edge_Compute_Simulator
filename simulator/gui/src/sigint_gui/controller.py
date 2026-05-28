"""Thread‑safe controller that runs the simulator and emits GUI updates."""

from __future__ import annotations

import logging
from typing import Optional

from PyQt5.QtCore import QMutex, QMutexLocker, QObject, QThread, pyqtSignal

from .models import LinkState, NodeState, SimulatorConfig
from .simulator_wrapper import Simulator

logger = logging.getLogger(__name__)

# Configuration limits (fixed bounds)
MAX_STEPS_PER_TICK: int = 10
MIN_SPEED_MS: int = 10
MAX_SPEED_MS: int = 1000
DEFAULT_SPEED_MS: int = 100

from sigint_gui._sigint_sim_core import (
    create_default_simulator,
    create_hql_simulator,
    create_gossip_simulator,
    create_dtn_simulator,
)


class _SimulationWorker(QObject):
    """Worker object that runs the simulation loop on a separate thread."""

    # Signal emitted when the worker has finished a batch of steps
    state_ready = pyqtSignal(list, list, float)  # nodes, links, sim_time
    finished = pyqtSignal()
    error_occurred = pyqtSignal(str)
    metrics_ready = pyqtSignal(dict, float)

    def __init__(self, sim: Simulator, parent: Optional[QObject] = None) -> None:
        super().__init__(parent)
        self._sim = sim
        self._running = False
        self._stop_requested = False
        self._mutex = QMutex()
        self._step_interval_ms: int = DEFAULT_SPEED_MS    

    # ------------------------------------------------------------------
    # Public API (called from controller thread, thread‑safe)
    # ------------------------------------------------------------------

    def request_play(self) -> None:
        with QMutexLocker(self._mutex):
            self._running = True

    def request_pause(self) -> None:
        with QMutexLocker(self._mutex):
            self._running = False
            
    def request_stop(self) -> None:
        """Signal the worker to exit its main loop."""
        with QMutexLocker(self._mutex):
            self._stop_requested = True
            self._running = False

    def set_interval(self, ms: int) -> None:
        """Set the delay between simulation steps (clamped)."""
        ms = max(MIN_SPEED_MS, min(MAX_SPEED_MS, ms))
        with QMutexLocker(self._mutex):
            self._step_interval_ms = ms

    def is_running(self) -> bool:
        with QMutexLocker(self._mutex):
            return self._running

    # ------------------------------------------------------------------
    # Main loop (runs in worker thread)
    # ------------------------------------------------------------------

    def run_loop(self) -> None:
        """Infinite loop that processes steps while _running is True."""
        
        assert self._sim is not None, "Simulator must be set before starting loop"
        finished_normally = False
        while True:
            with QMutexLocker(self._mutex):
                if self._stop_requested:
                    break
                running, interval = self._running, self._step_interval_ms

            if not running:
                QThread.msleep(50)
                continue

            if self._sim.is_finished:
                self._emit_state()
                finished_normally = True
                break

            try:
                self._sim.step(1)
                self._emit_state()
            except Exception as exc:
                logger.exception("Simulation step failed")
                self.error_occurred.emit(str(exc))
                break

            QThread.msleep(interval)

        if finished_normally:
            self.finished.emit()

    def run_single_step(self) -> None:
        """Execute a single step (used for manual stepping)."""
        if self._sim.is_finished:
            self.finished.emit()
            return
        try:
            self._sim.step(1)
            self._emit_state()
        except Exception as exc:
            self.error_occurred.emit(str(exc))

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------
    def _emit_state(self) -> None:
        nodes = self._sim.get_node_states()
        links = self._sim.get_link_states()
        self.state_ready.emit(nodes, links, self._sim.current_time)
        
        metrics = self._sim.get_metrics()  # dict
        self.metrics_ready.emit(metrics, self._sim.current_time)

# ---------------------------------------------------------------------------
# SimulatorController – public API for the GUI
# ---------------------------------------------------------------------------

class SimulatorController(QObject):
    """Manages the simulator lifecycle and its background thread.

    Signals:
        state_updated(nodes, links, sim_time) – emitted after each state change.
        finished() – emitted when the simulation reaches its duration.
        error_occurred(msg) – emitted when a fatal error occurs.
    """

    state_updated = pyqtSignal(list, list, float)
    finished = pyqtSignal()
    error_occurred = pyqtSignal(str)
    metrics_updated = pyqtSignal(dict, float)

    def __init__(self, parent: Optional[QObject] = None) -> None:
        super().__init__(parent)
        self._sim: Optional[Simulator] = None
        self._worker: Optional[_SimulationWorker] = None
        self._thread: Optional[QThread] = None

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    @property
    def sim(self):
        return self._sim

    def load_scenario(self, config: SimulatorConfig, agent_type: str = "Random") -> None:
        """Stop any running simulation, create a new simulator with the given agent type,
        and prepare the worker thread (but do not start it yet)."""
        # 1. Stop previous run
        self.stop()

        # 2. Build the C++ simulator using the chosen agent factory
        kwargs = dict(
            seed=config.seed,
            duration=config.duration,
            topology=config.topology,
            availability=config.availability,
            avg_snr_db=config.avg_snr_db,
        )
        if agent_type == "HQL":
            # hql_config defaults are handled inside the binding when dict is empty
            cpp_sim = create_hql_simulator(**kwargs)
        elif agent_type == "Gossip":
            cpp_sim = create_gossip_simulator(**kwargs)
        elif agent_type == "DTN":
            cpp_sim = create_dtn_simulator(**kwargs)
        else:  # Random
            cpp_sim = create_default_simulator(**kwargs)

        # 3. Wrap the C++ object in our Python Simulator (which adds type hints, context manager, etc.)
        #    We assume a class method `from_existing` or we can directly create a Simulator
        #    that takes ownership of the C++ object.
        self._sim = Simulator.from_existing(cpp_sim, config)
        
    def load_json(self, json_path: str) -> None:
        """Load a JSON scenario and prepare the simulation (does not start)."""
        self.stop()
        self._sim = Simulator.from_json(json_path)

    def start(self) -> None:
        """Start the background thread and begin the simulation loop."""
        assert self._sim is not None, "No simulator loaded"
        assert self._thread is None, "Thread already started"

        self._thread = QThread()
        self._worker = _SimulationWorker(self._sim)
        self._worker.moveToThread(self._thread)

        # Connect signals for cleanup
        self._thread.started.connect(self._worker.run_loop)
        self._worker.finished.connect(self._thread.quit)
        self._worker.finished.connect(self._on_finished)
        self._worker.error_occurred.connect(self._on_error)
        self._worker.state_ready.connect(self._on_state_ready)
        self._thread.finished.connect(self._thread.deleteLater)
        
        self._worker.metrics_ready.connect(self.metrics_updated)   # relay

        self._thread.start()

    def stop(self) -> None:
        """Pause and clean up the background thread.  Safe to call multiple times."""
        if self._worker is not None:
            self._worker.request_stop()   
            self._worker = None
            
        if self._thread is not None:
            if self._thread.isRunning():
                self._thread.quit()
                self._thread.wait(3000)
            self._thread = None

    # ------------------------------------------------------------------
    # Simulation control (delegated to worker)
    # ------------------------------------------------------------------

    def play(self) -> None:
        """Resume continuous stepping."""
        if self._worker is not None:
            self._worker.request_play()

    def pause(self) -> None:
        """Pause continuous stepping."""
        if self._worker is not None:
            self._worker.request_pause()

    def step(self) -> None:
        """Execute a single simulation step (pauses continuous play)."""
        if self._worker is not None:
            self._worker.request_pause()
            self._worker.run_single_step()

    def reset(self, seed: int) -> None:
        """Reset the simulator with a new seed."""
        assert self._sim is not None, "No simulator loaded"
        assert seed >= 0, f"Seed must be non‑negative, got {seed}"
        self._sim.reset(seed)
        # Emit the initial state so the GUI refreshes
        nodes = self._sim.get_node_states()
        links = self._sim.get_link_states()
        self.state_updated.emit(nodes, links, self._sim.current_time)

    def set_speed(self, ms: int) -> None:
        """Set the delay between steps in milliseconds (clamped)."""
        if self._worker is not None:
            self._worker.set_interval(ms)
            
    def set_simulator(self, sim: Simulator) -> None:
        """Replace the current simulator (must be called when stopped)."""
        self._sim = sim
        self._worker = None
        self._thread = None

    # ------------------------------------------------------------------
    # Internal slot handlers
    # ------------------------------------------------------------------

    def _on_state_ready(
        self, nodes: list, links: list, sim_time: float
    ) -> None:
        self.state_updated.emit(nodes, links, sim_time)

    def _on_finished(self) -> None:
        self.stop()
        self.finished.emit()


    def _on_error(self, msg: str) -> None:
        self.error_occurred.emit(msg)
        self.stop()
        
    def request_stop(self) -> None:
        """Tell the worker to exit its main loop at the next iteration."""
        with QMutexLocker(self._mutex):
            self._stop_requested = True
            self._running = False # also stop stepping