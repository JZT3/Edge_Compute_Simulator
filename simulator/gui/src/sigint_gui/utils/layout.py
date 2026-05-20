"""Deterministic force‑directed graph layout for up to ~100 nodes."""

from __future__ import annotations

import math
import random
from typing import Dict, List, Tuple

# Fixed bounds for the layout algorithm
MAX_ITERATIONS: int = 100
MIN_MOVEMENT_THRESHOLD: float = 1e-3
REPULSION_STRENGTH: float = 5000.0
ATTRACTION_STRENGTH: float = 0.01
DAMPING: float = 0.9


def compute_layout(
    node_ids: List[int],
    edges: List[Tuple[int, int]],
    seed: int = 0,
    width: float = 600.0,
    height: float = 600.0,
) -> Dict[int, Tuple[float, float]]:
    """Run a simple force‑directed algorithm and return node positions.

    The algorithm is deterministic (given a seed), uses a fixed number of
    iterations, and operates entirely on Python floats—no external libraries
    needed.

    Args:
        node_ids: List of unique node identifiers.
        edges: List of (source, target) pairs.
        seed: Random seed for reproducible initial positions.
        width, height: Bounding box for positions.

    Returns:
        Mapping from node_id to (x, y) coordinates (centre of the canvas).
    """
    assert len(node_ids) > 0, "Node list must not be empty"
    assert width > 0 and height > 0, "Canvas dimensions must be positive"

    rng = random.Random(seed)

    # Initialise positions randomly within the canvas
    positions: Dict[int, List[float]] = {}
    for nid in node_ids:
        positions[nid] = [
            rng.uniform(width * 0.25, width * 0.75),
            rng.uniform(height * 0.25, height * 0.75),
        ]

    # Build adjacency for fast lookup
    adjacency: Dict[int, List[int]] = {nid: [] for nid in node_ids}
    for u, v in edges:
        adjacency[u].append(v)
        adjacency[v].append(u)

    # Precompute repulsion constant
    k_squared = (width * height) / len(node_ids) if node_ids else 1.0

    # Fixed‑bound iteration loop
    for iteration in range(MAX_ITERATIONS):
        # Displacement vectors for this iteration
        displacements: Dict[int, List[float]] = {
            nid: [0.0, 0.0] for nid in node_ids
        }

        # Repulsive forces (all node pairs)
        node_list = node_ids  # local alias
        n = len(node_list)
        for i in range(n):
            uid = node_list[i]
            for j in range(i + 1, n):
                vid = node_list[j]
                dx = positions[uid][0] - positions[vid][0]
                dy = positions[uid][1] - positions[vid][1]
                distance = math.hypot(dx, dy)
                if distance < 1.0:
                    distance = 1.0  # avoid division by zero
                force = REPULSION_STRENGTH * k_squared / (distance * distance)
                fx = (dx / distance) * force
                fy = (dy / distance) * force
                displacements[uid][0] += fx
                displacements[uid][1] += fy
                displacements[vid][0] -= fx
                displacements[vid][1] -= fy

        # Attractive forces (along edges)
        for u, v in edges:
            dx = positions[u][0] - positions[v][0]
            dy = positions[u][1] - positions[v][1]
            distance = math.hypot(dx, dy)
            if distance < 1.0:
                distance = 1.0
            force = ATTRACTION_STRENGTH * (distance * distance) / k_squared
            fx = (dx / distance) * force
            fy = (dy / distance) * force
            displacements[u][0] -= fx
            displacements[u][1] -= fy
            displacements[v][0] += fx
            displacements[v][1] += fy

        # Apply displacements with damping and boundary constraints
        max_movement = 0.0
        for nid in node_ids:
            dx = displacements[nid][0] * DAMPING
            dy = displacements[nid][1] * DAMPING
            positions[nid][0] = max(50.0, min(width - 50.0, positions[nid][0] + dx))
            positions[nid][1] = max(50.0, min(height - 50.0, positions[nid][1] + dy))
            movement = math.hypot(dx, dy)
            if movement > max_movement:
                max_movement = movement

        # Early exit if the layout has stabilised
        if max_movement < MIN_MOVEMENT_THRESHOLD:
            break

    # Convert to immutable tuples
    return {nid: (pos[0], pos[1]) for nid, pos in positions.items()}