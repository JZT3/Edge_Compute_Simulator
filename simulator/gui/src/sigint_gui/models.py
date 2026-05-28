"""Immutable data models used throughout the GUI."""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum
from typing import Dict, List, Tuple, Optional


class NodeMode(IntEnum):
    """Mirrors the C++ NodeMode enum."""
    IDLE = 0
    SCAN = 1
    PROCESS = 2
    TRANSMIT = 3


@dataclass(frozen=True)
class NodeState:
    id: int
    name: str
    mode: NodeMode
    buffer_size: int
    energy_used: float
    device_type: str = ""
    noise_figure_dB: float = 0.0
    tx_power_dBm: float = 0.0
    frequency_accuracy_ppm: float = 0.0
    fft_gflops_per_sec: float = 0.0
    x: float = 0.0
    y: float = 0.0
    min_freq_hz: float = 50e6
    max_freq_hz: float = 2.5e9
    current_rf: Optional[RFParams] = None   # we’ll add RFParams later if needed


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
