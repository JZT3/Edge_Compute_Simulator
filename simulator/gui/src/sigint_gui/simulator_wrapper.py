"""Pythonic wrapper around the native _sigint_sim_core extension."""

from __future__ import annotations

import logging
from types import TracebackType
from typing import Any, Dict, List, Optional, Type

from .models import (
    Event,
    LinkState,
    NodeMode,
    NodeState,
    SimulatorConfig,
)

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Internal helpers – convert C++ structs to frozen dataclasses
# ---------------------------------------------------------------------------

def _node_state_from_cpp(raw: Any) -> NodeState:
    """Convert a _sigint_sim_core.NodeState to a Python NodeState."""
    assert raw is not None, "Raw NodeState must not be None"
    mode_int: int = getattr(raw, "mode", 0)
    mode = NodeMode(mode_int)
    result = NodeState(
        id=getattr(raw, "id", -1),
        name=getattr(raw, "name", ""),
        mode=mode,
        buffer_size=getattr(raw, "buffer_size", 0),
        energy_used=getattr(raw, "energy_used", 0.0),
    )
    assert result.id >= 0, f"Invalid node id: {result.id}"
    return result


def _link_state_from_cpp(raw: Any) -> LinkState:
    """Convert a _sigint_sim_core.LinkState to a Python LinkState."""
    assert raw is not None, "Raw LinkState must not be None"
    result = LinkState(
        id=getattr(raw, "id", -1),
        from_id=getattr(raw, "from_id", -1),
        to_id=getattr(raw, "to_id", -1),
        active=bool(getattr(raw, "active", False)),
        snr=float(getattr(raw, "snr", -200.0)),
        capacity_bps=float(getattr(raw, "capacity_bps", 0.0)),
    )
    assert result.id >= 0, f"Invalid link id: {result.id}"
    return result


def _event_from_cpp(raw: Any) -> Event:
    """Convert a _sigint_sim_core.Event to a Python Event."""
    assert raw is not None, "Raw Event must not be None"
    params: Dict[str, float] = dict(getattr(raw, "params", {}))
    result = Event(
        time=float(getattr(raw, "time", 0.0)),
        node_id=int(getattr(raw, "node_id", -1)),
        type=str(getattr(raw, "type", "")),
        params=params,
    )
    assert result.type, "Event type must not be empty"
    return result


# ---------------------------------------------------------------------------
# Main simulator wrapper
# ---------------------------------------------------------------------------

class Simulator:
    """High‑level simulator interface.

    Manages the lifecycle of a native C++ Simulator, provides type‑safe
    Python state snapshots, and supports context manager usage.
    """

    def __init__(self, config: SimulatorConfig) -> None:
        """Initialise the simulation.

        Args:
            config: Validated configuration.

        Raises:
            RuntimeError: If the native simulator cannot be created.
        """
        assert isinstance(config, SimulatorConfig), (
            "config must be a SimulatorConfig instance"
        )
        self._config = config
        self._native: Any = None
        self._event_log: List[Event] = []

        try:
            from sigint_gui import _sigint_sim_core as _core
        except ImportError as exc:
            raise RuntimeError(
                "Could not import _sigint_sim_core from sigint_gui. "
                "Did you build the bindings? "
                "Ensure the .so file is inside gui/src/sigint_gui/ and "
                "you have run `pip install -e .` from the gui/ folder."
            ) from exc

        # Use the factory to create a complete simulator
        self._native = _core.create_default_simulator(
            seed=self._config.seed,
            duration=self._config.duration,
            topology=self._config.topology,
            availability=self._config.availability,
            avg_snr_db=self._config.avg_snr_db,
        )
        assert self._native is not None, "Native simulator creation failed"

    def __enter__(self) -> "Simulator":
        return self

    def __exit__(
        self,
        exc_type: Optional[Type[BaseException]],
        exc_val: Optional[BaseException],
        exc_tb: Optional[TracebackType],
    ) -> None:
        self.close()

    def close(self) -> None:
        """Release native resources. Idempotent."""
        self._native = None
        self._event_log.clear()

    # ------------------------------------------------------------------
    # Simulation control
    # ------------------------------------------------------------------

    def step(self, steps: int = 1) -> List[Event]:
        """Advance the simulation by *steps* timesteps.

        Args:
            steps: Positive number of steps to execute.

        Returns:
            New events generated during these steps.
        """
        assert steps > 0, f"steps must be positive, got {steps}"
        assert self._native is not None, "Simulator has been closed"

        new_events: List[Event] = []
        for _ in range(steps):
            if self.is_finished:
                break
            # C++ step() releases the GIL – safe to call from threads.
            self._native.step()
            # Append new events from the C++ log (we track the difference).
            new_events.extend(self._collect_new_events())
        self._event_log.extend(new_events)
        return new_events

    def reset(self, seed: int) -> None:
        """Reset the simulator with a new seed.

        Args:
            seed: New random seed for deterministic replay.
        """
        assert self._native is not None, "Simulator has been closed"
        assert isinstance(seed, int) and seed >= 0, (
            f"Seed must be a non‑negative integer, got {seed}"
        )
        self._native.reset(seed)
        self._event_log.clear()
        logger.info("Simulator reset with seed %d", seed)

    # ------------------------------------------------------------------
    # State access
    # ------------------------------------------------------------------

    @property
    def current_time(self) -> float:
        """Current simulation time in seconds."""
        assert self._native is not None, "Simulator has been closed"
        return float(self._native.current_time)

    @property
    def is_finished(self) -> bool:
        """Whether the simulation has reached its duration."""
        assert self._native is not None, "Simulator has been closed"
        return bool(self._native.is_finished)

    def get_node_states(self) -> List[NodeState]:
        """Return frozen snapshots of all nodes."""
        assert self._native is not None, "Simulator has been closed"
        raw_nodes = self._native.get_node_states()
        result = [_node_state_from_cpp(n) for n in raw_nodes]
        assert all(isinstance(n, NodeState) for n in result), (
            "All converted objects must be NodeState"
        )
        return result

    def get_link_states(self) -> List[LinkState]:
        """Return frozen snapshots of all links."""
        assert self._native is not None, "Simulator has been closed"
        raw_links = self._native.get_link_states()
        result = [_link_state_from_cpp(l) for l in raw_links]
        assert all(isinstance(l, LinkState) for l in result), (
            "All converted objects must be LinkState"
        )
        return result

    def get_event_log(self) -> List[Event]:
        """Return a copy of all events collected so far."""
        return list(self._event_log)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _collect_new_events(self) -> List[Event]:
        """Query the native event log and convert only the new ones."""
        assert self._native is not None, "Simulator has been closed"
        raw_events = self._native.get_event_log()
        # We only convert the slice since the last known index.
        new_raw = raw_events[len(self._event_log):]
        return [_event_from_cpp(e) for e in new_raw]