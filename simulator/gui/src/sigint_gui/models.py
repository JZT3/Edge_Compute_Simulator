"""Immutable data models used throughout the GUI."""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum
from typing import Dict, List, Tuple


class NodeMode(IntEnum):
    """Mirrors the C++ NodeMode enum."""
    IDLE = 0
    SCAN = 1
    PROCESS = 2
    TRANSMIT = 3


@dataclass(frozen=True)
class NodeState:
    """Snapshot of a single SDR node’s state."""
    id: int
    name: str
    mode: NodeMode
    buffer_size: int
    energy_used: float


@dataclass(frozen=True)
class LinkState:
    """Snapshot of a communication link between two nodes."""
    id: int
    from_id: int
    to_id: int
    active: bool
    snr: float
    capacity_bps: float


@dataclass(frozen=True)
class Event:
    """An event logged by the simulator."""
    time: float
    node_id: int
    type: str
    params: Dict[str, float] = field(default_factory=dict)


@dataclass(frozen=True)
class SimulatorConfig:
    seed: int = 42
    duration: float = 10.0
    topology: List[Tuple[int, int]] = field(default_factory=lambda: [(0, 1), (1, 0)])
    availability: float = 0.9
    avg_snr_db: float = 20.0
    timestep: float = 0.1

    def topology_nodes(self) -> List[int]:
        """Return all unique node IDs present in the topology edges."""
        ids = set()
        for u, v in self.topology:
            ids.add(u)
            ids.add(v)
        return sorted(ids)
