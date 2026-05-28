#!/usr/bin/env python3
"""Optuna‑based hyperparameter tuning for the HQL agent."""

import json
import math
import statistics
from typing import Dict, List

import multiprocessing
multiprocessing.set_start_method("spawn", force=True)

import optuna
from sigint_gui._sigint_sim_core import create_hql_simulator

SEEDS_FAST = list(range(10))       # for pruning
SEEDS_FINAL = list(range(30))      # for best trial evaluation
TOPOLOGY = [(0,1),(1,2),(2,3),(3,4),(4,0)]
AVAILABILITY = 0.6
AVG_SNR_DB = 25.0
DURATION = 2000

def evaluate_params(params: Dict, seeds: List[int]) -> Dict:
    deliveries = []
    for seed in seeds:
        sim = create_hql_simulator(params, seed, DURATION, TOPOLOGY,
                                   AVAILABILITY, AVG_SNR_DB)
        d = sim.run_for_steps(DURATION)
        deliveries.append(d)
    mean_d = statistics.mean(deliveries)
    std_d  = statistics.stdev(deliveries) if len(deliveries) > 1 else 0.0
    return {"mean": mean_d, "std": std_d}

def objective(trial: optuna.Trial) -> float:
    p = {}
    p["alpha"] = trial.suggest_float("alpha", 0.02, 0.3, log=True)
    beta_ratio = trial.suggest_float("beta_ratio", 10.0, 30.0)
    p["beta"] = p["alpha"] / beta_ratio
    p["gamma"] = trial.suggest_float("gamma", 0.84, 0.96)
    p["lambda"] = trial.suggest_float("lambda", 0.5, 0.90)
    p["epsilon_start"] = trial.suggest_float("epsilon_start", 0.7, 1.0)
    p["epsilon_end"]   = trial.suggest_float("epsilon_end", 0.01, 0.10)
    p["epsilon_decay_steps"] = trial.suggest_int("epsilon_decay_steps", 7000, 20000)
    p["mu"] = trial.suggest_float("mu", 0.3, 1.8)
    p["trace_length"] = trial.suggest_int("trace_length", 300, 900)

    metrics = evaluate_params(p, SEEDS_FAST)
    # Use Lower Confidence Bound (LCB) to penalise variance
    lcb = metrics["mean"] - 2.0 * metrics["std"]
    return lcb

def main():
    study = optuna.create_study(
        direction="maximize",
        pruner=optuna.pruners.MedianPruner(n_startup_trials=10),
        sampler=optuna.samplers.TPESampler(seed=42),
    )
    # n_jobs = number of parallel processes (set to your physical core count)
    study.optimize(objective, n_trials=100, n_jobs=4, show_progress_bar=True)
    
    print("Best trial:")
    print(f"  params: {study.best_params}")
    print(f"  value (LCB): {study.best_value:.2f}")

    # Re‑evaluate best on more seeds
    best = study.best_params
    best["beta"] = best["alpha"] / best.pop("beta_ratio")
    final_metrics = evaluate_params(best, SEEDS_FINAL)
    print(f"  final mean (30 seeds): {final_metrics['mean']:.2f} ± {final_metrics['std']:.2f}")

    # Save all trials
    df = study.trials_dataframe()
    df.to_csv("optuna_results.csv", index=False)
    with open("best_params.json", "w") as f:
        json.dump({"params": best, "final_metrics": final_metrics}, f, indent=2)

if __name__ == "__main__":
    main()