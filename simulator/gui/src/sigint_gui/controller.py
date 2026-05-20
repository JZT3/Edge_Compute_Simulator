"""Thread‑safe controller that runs the simulator and emits GUI updates."""

from __future__ import annotations

import logging
from typing import Optional

from PyQt5.QtCore import QMutex, QObject, QThread, QTimer, pyqtSignal

from .models import LinkState, NodeState, SimulatorConfig
from .simulator_wrapper import Simulator

logger = logging.getLogger(__name__)

# Configuration limits (fixed bounds)
MAX_STEPS_PER_TICK: int = 10
MIN_SPEED_MS: int = 10
MAX_SPEED_MS: int = 1000
DEFAULT_SPEED_MS: int = 100


class _SimulationWorker(QObject):
    """Worker object that runs the simulation loop on a separate thread."""

    # Signal emitted when the worker has finished a batch of steps
    state_ready = pyqtSignal(list, list, float)  # nodes, links, sim_time
    finished = pyqtSignal()
    error_occurred = pyqtSignal(str)

    def __init__(self, sim: Simulator, parent: Optional[QObject] = None) -> None:
        super().__init__(parent)
        self._sim = sim
        self._running = False
        self._mutex = QMutex()
        self._step_interval_ms: int = DEFAULT_SPEED_MS

    # ------------------------------------------------------------------
    # Public API (called from controller thread, thread‑safe)
    # ------------------------------------------------------------------

    def request_play(self) -> None:
        with QMutex.locker(self._mutex):
            self._running = True

    def request_pause(self) -> None:
        with QMutex.locker(self._mutex):
            self._running = False

    def set_interval(self, ms: int) -> None:
        """Set the delay between simulation steps (clamped)."""
        ms = max(MIN_SPEED_MS, min(MAX_SPEED_MS, ms))
        with QMutex.locker(self._mutex):
            self._step_interval_ms = ms

    def is_running(self) -> bool:
        with QMutex.locker(self._mutex):
            return self._running

    # ------------------------------------------------------------------
    # Main loop (runs in worker thread)
    # ------------------------------------------------------------------

    def run_loop(self) -> None:
        """Infinite loop that processes steps while _running is True."""
        assert self._sim is not None, "Simulator must be set before starting loop"

        while True:
            # Check if we should keep running
            with QMutex.locker(self._mutex):
                running = self._running
                interval = self._step_interval_ms

            if not running:
                # Sleep a bit to avoid busy‑waiting
                QThread.msleep(50)
                continue

            if self._sim.is_finished:
                # Emit final state and stop
                self._emit_state()
                self.finished.emit()
                break

            try:
                # Advance by one timestep; C++ step() releases GIL
                self._sim.step(1)
                self._emit_state()
            except Exception as exc:
                logger.exception("Simulation step failed")
                self.error_occurred.emit(str(exc))
                break

            QThread.msleep(interval)

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

    def __init__(self, parent: Optional[QObject] = None) -> None:
        super().__init__(parent)
        self._sim: Optional[Simulator] = None
        self._worker: Optional[_SimulationWorker] = None
        self._thread: Optional[QThread] = None

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def load_scenario(self, config: SimulatorConfig) -> None:
        """Create a new simulator and prepare the worker thread.

        Precondition: no simulation is currently running.
        """
        assert isinstance(config, SimulatorConfig), "Expected SimulatorConfig"
        assert self._thread is None or not self._thread.isRunning(), (
            "Must stop previous simulation before loading a new one"
        )

        if self._sim is not None:
            self._sim.close()
        self._sim = Simulator(config)

    def start(self) -> None:
        """Start the background thread and begin the simulation loop."""
        assert self._sim is not None, "No simulator loaded"
        assert self._thread is None, "Thread already started"

        self._thread = QThread()
        self._worker = _SimulationWorker(self._sim)

        # Move worker to the new thread
        self._worker.moveToThread(self._thread)

        # Connect signals for cleanup
        self._thread.started.connect(self._worker.run_loop)
        self._worker.finished.connect(self._thread.quit)
        self._worker.finished.connect(self._on_finished)
        self._worker.error_occurred.connect(self._on_error)
        self._worker.state_ready.connect(self._on_state_ready)
        self._thread.finished.connect(self._thread.deleteLater)

        self._thread.start()

    def stop(self) -> None:
        """Stop the simulation and clean up the thread."""
        if self._worker is not None:
            self._worker.request_pause()
        if self._thread is not None:
            self._thread.quit()
            self._thread.wait(2000)  # bounded wait
            self._thread = None
            self._worker = None

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

    # ------------------------------------------------------------------
    # Internal slot handlers
    # ------------------------------------------------------------------

    def _on_state_ready(
        self, nodes: list, links: list, sim_time: float
    ) -> None:
        self.state_updated.emit(nodes, links, sim_time)

    def _on_finished(self) -> None:
        self.finished.emit()
        self.stop()

    def _on_error(self, msg: str) -> None:
        self.error_occurred.emit(msg)
        self.stop()