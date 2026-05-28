#!/usr/bin/env python3
"""
Random‑search hyperparameter tuning for the Hysteretic Q‑Learner.

Runs 50 trials on a 5‑node ring network, each evaluated on 10 different seeds.
Saves the results to ``random_search_results.json``.
"""

import json
import math
import random
import statistics
import sys
from typing import Dict, List, Tuple

# ---------------------------------------------------------------------------
# Import the compiled binding – make sure the .so is on PYTHONPATH
# (e.g. export PYTHONPATH=build/:$PYTHONPATH from the repo root)
# ---------------------------------------------------------------------------

from pathlib import Path
    
_so_dir = Path(__file__).resolve().parents[2] / "gui" / "src" / "sigint_gui"
if str(_so_dir) not in sys.path:
    sys.path.insert(0, str(_so_dir))

try:
    from sigint_gui import _sigint_sim_core as core
except ImportError:
    print("Could not import _sigint_sim_core.  Is the build complete?")
    print("Try: export PYTHONPATH=build/:$PYTHONPATH")
    sys.exit(1)

# ---------------------------------------------------------------------------
# Fixed scenario parameters
# ---------------------------------------------------------------------------
SEEDS = list(range(10))                     # 10 evaluation seeds per trial
TOPOLOGY = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 0)]   # 5‑node ring
AVAILABILITY = 0.6                          # link duty cycle
AVG_SNR_DB = 25.0                           # average signal‑to‑noise ratio
DURATION = 2000                             # simulation steps

# ---------------------------------------------------------------------------
# Hyper‑parameter prior ranges
# ---------------------------------------------------------------------------
PARAM_RANGES = {
    "alpha":               (0.005, 0.3),     # log‑uniform
    "beta_ratio":          (5.0, 20.0),      # beta = alpha / beta_ratio
    "gamma":               (0.8, 0.999),
    "lambda":              (0.5, 0.95),
    "epsilon_start":       (0.5, 1.0),
    "epsilon_end":         (0.01, 0.2),
    "epsilon_decay_steps": (500, 20000),     # integer
    "mu":                  (0.1, 2.0),
    "trace_length":        (100, 1000),      # integer
}

# ---------------------------------------------------------------------------
# Helper: sample one hyperparameter set
# ---------------------------------------------------------------------------
def sample_params() -> Dict:
    p = {}
    # alpha – log‑uniform
    lo, hi = PARAM_RANGES["alpha"]
    p["alpha"] = 10 ** random.uniform(math.log10(lo), math.log10(hi))

    # beta = alpha / beta_ratio
    beta_ratio = random.uniform(*PARAM_RANGES["beta_ratio"])
    p["beta"] = p["alpha"] / beta_ratio

    p["gamma"]   = random.uniform(*PARAM_RANGES["gamma"])
    p["lambda"]  = random.uniform(*PARAM_RANGES["lambda"])
    p["epsilon_start"] = random.uniform(*PARAM_RANGES["epsilon_start"])
    p["epsilon_end"]   = random.uniform(*PARAM_RANGES["epsilon_end"])
    p["epsilon_decay_steps"] = random.randint(*PARAM_RANGES["epsilon_decay_steps"])
    p["mu"] = random.uniform(*PARAM_RANGES["mu"])
    p["trace_length"] = random.randint(*PARAM_RANGES["trace_length"])
    return p

# ---------------------------------------------------------------------------
# Evaluation of one parameter set over multiple seeds
# ---------------------------------------------------------------------------
def evaluate_params(params: Dict, seeds: List[int]) -> Dict:
    deliveries = []
    for seed in seeds:
        sim = core.create_hql_simulator(
            params,             # dict passed directly to C++
            seed,
            DURATION,
            TOPOLOGY,
            AVAILABILITY,
            AVG_SNR_DB,
        )
        # Run for the whole duration and return the number of new deliveries
        new_deliveries = sim.run_for_steps(DURATION)
        deliveries.append(new_deliveries)

    mean_d = statistics.mean(deliveries)
    std_d  = statistics.stdev(deliveries) if len(deliveries) > 1 else 0.0
    return {
        "mean_deliveries": mean_d,
        "std_deliveries":  std_d,
        "raw_values":      deliveries,
    }

# ---------------------------------------------------------------------------
# Main random‑search loop
# ---------------------------------------------------------------------------
def main() -> None:
    random.seed(42)               # reproducible sampling of hyper‑params
    results: List[Dict] = []
    print(f"{'Trial':<6} {'mean':<8} {'std':<8}")
    print("-" * 22)

    for i in range(50):
        params  = sample_params()
        metrics = evaluate_params(params, SEEDS)
        results.append({"params": params, "metrics": metrics})
        print(f"{i+1:<6} {metrics['mean_deliveries']:<8.2f} {metrics['std_deliveries']:<8.2f}")

    # Save for later analysis
    out_path = "random_search_results.json"
    with open(out_path, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to {out_path}")

if __name__ == "__main__":
    main()
    
"""
Best performing numbers based on 50 search
NARROWED_RANGES = {
    "alpha":               (0.02, 0.3),       # log‑uniform
    "beta_ratio":          (10.0, 30.0),      # slightly stronger hysteresis
    "gamma":               (0.84, 0.96),      # moderate discount
    "lambda":              (0.5, 0.90),        # avoid extreme λ
    "epsilon_decay_steps": (7000, 20000),     # slow decay
    "mu":                  (0.3, 1.8),
}
"""