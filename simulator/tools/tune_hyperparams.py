import random
import math
import json
import statistics
from _sigint_sim_core import create_hql_simulator

DURATION = 2000
TOPOLOGY = [(0,1),(1,2),(2,3),(3,4),(4,0)]  # 5-node ring
AVAILABILITY = 0.6
AVG_SNR = 25.0
SEEDS = list(range(10))
DELIVERY_WEIGHT = 1.0   # each delivery contributes this to objective

def sample_params():
    p = {}
    p["alpha"] = 10 ** random.uniform(math.log10(0.005), math.log10(0.3))
    beta_ratio = random.uniform(5, 20)
    p["beta"] = p["alpha"] / beta_ratio
    p["gamma"] = random.uniform(0.8, 0.999)
    p["lambda"] = random.uniform(0.5, 0.95)
    p["epsilon_start"] = random.uniform(0.5, 1.0)
    p["epsilon_end"] = random.uniform(0.01, 0.2)
    p["epsilon_decay_steps"] = random.randint(500, 20000)
    p["mu"] = random.uniform(0.1, 2.0)
    p["trace_length"] = random.randint(100, 1000)
    return p

def evaluate_params(params, seeds):
    deliveries = []
    for seed in seeds:
        sim = create_hql_simulator(params, seed, DURATION, TOPOLOGY,
                                   AVAILABILITY, AVG_SNR)
        new_deliveries = sim.run_for_steps(DURATION)
        deliveries.append(new_deliveries)
    mean_del = statistics.mean(deliveries)
    std_del = statistics.stdev(deliveries) if len(deliveries) > 1 else 0.0
    return {
        "mean_reward": mean_del * DELIVERY_WEIGHT,
        "std_reward": std_del * DELIVERY_WEIGHT,
        "divergence_rate": 0.0,   # placeholder
    }

def main():
    random.seed(42)
    results = []
    for i in range(50):
        params = sample_params()
        metrics = evaluate_params(params, SEEDS)
        results.append({"params": params, "metrics": metrics})
        print(f"Trial {i+1:2d}: mean={metrics['mean_reward']:.2f} "
              f"std={metrics['std_reward']:.2f}")
    with open("random_search_results.json", "w") as f:
        json.dump(results, f, indent=2)

if __name__ == "__main__":
    main()