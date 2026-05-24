import Mathlib.Data.Real.Basic
import Mathlib.Data.Fin.Basic
import Mathlib.Algebra.BigOperators.Group.Finset
import Mathlib.Tactic.Ring
import Mathlib.Tactic.Linarith

-- Fix the parameters at the top level so Lean doesn't have to guess them
variable (N num_neighbors num_freqs num_powers : Nat)

structure AgentAction where
  is_transmit : Bool
  neighbor_idx : Fin (max 1 num_neighbors) -- max 1 ensures it's always Inhabited
  freq_bin : Fin (max 1 num_freqs)
  power_bin : Fin (max 1 num_powers)

def ActionProfile := Fin N → AgentAction num_neighbors num_freqs num_powers

structure GameConfig where
  omega : Real
  gamma_loc : Real
  mu : Real

def joint_delivery_value (cfg : GameConfig) (a : ActionProfile N num_neighbors num_freqs num_powers) : Real :=
  ∑ i : Fin N, (if (a i).is_transmit then cfg.omega * cfg.gamma_loc else 0)

def penalty_i (cfg : GameConfig) (a : ActionProfile N num_neighbors num_freqs num_powers) (i : Fin N) : Real :=
  if (a i).is_transmit then cfg.mu else 0

def utility_i_coupled (cfg : GameConfig) (a : ActionProfile N num_neighbors num_freqs num_powers) (i : Fin N) : Real :=
  joint_delivery_value N num_neighbors num_freqs num_powers cfg a - penalty_i N num_neighbors num_freqs num_powers cfg a i

open scoped BigOperators
def global_potential_coupled (cfg : GameConfig) (a : ActionProfile N num_neighbors num_freqs num_powers) : Real :=
  joint_delivery_value N num_neighbors num_freqs num_powers cfg a - ∑ i : Fin N, penalty_i N num_neighbors num_freqs num_powers cfg a i

def update_profile (a : ActionProfile N num_neighbors num_freqs num_powers) (i : Fin N)
  (new_ai : AgentAction num_neighbors num_freqs num_powers) :
  ActionProfile N num_neighbors num_freqs num_powers :=
  fun j => if j = i then new_ai else a j

-- Theorem 1: Exact Potential Game Identity
theorem exact_potential_game_identity
  (cfg : GameConfig) (a : ActionProfile N num_neighbors num_freqs num_powers)
  (i : Fin N) (new_ai : AgentAction num_neighbors num_freqs num_powers) :
  utility_i_coupled N num_neighbors num_freqs num_powers cfg (update_profile N num_neighbors num_freqs num_powers a i new_ai) i - utility_i_coupled N num_neighbors num_freqs num_powers cfg a i =
  global_potential_coupled N num_neighbors num_freqs num_powers cfg (update_profile N num_neighbors num_freqs num_powers a i new_ai) - global_potential_coupled N num_neighbors num_freqs num_powers cfg a := by

  unfold utility_i_coupled global_potential_coupled

  have H_penalty_sum :
    (∑ j : Fin N, penalty_i N num_neighbors num_freqs num_powers cfg (update_profile N num_neighbors num_freqs num_powers a i new_ai) j) - (∑ j : Fin N, penalty_i N num_neighbors num_freqs num_powers cfg a j) =
    penalty_i N num_neighbors num_freqs num_powers cfg (update_profile N num_neighbors num_freqs num_powers a i new_ai) i - penalty_i N num_neighbors num_freqs num_powers cfg a i := by
    rw [← Finset.sum_sub_distrib]
    apply Finset.sum_eq_single i
    · intro j _ hj
      unfold penalty_i update_profile
      simp only [if_neg hj]
      ring
    · intro hi
      exact False.elim (hi (Finset.mem_univ i))
  linarith

-- Theorem 2: Nash Equilibrium Implication
theorem max_potential_is_nash
  (cfg : GameConfig) (a_opt : ActionProfile N num_neighbors num_freqs num_powers) :
  (∀ a, global_potential_coupled N num_neighbors num_freqs num_powers cfg a ≤ global_potential_coupled N num_neighbors num_freqs num_powers cfg a_opt) →
  (∀ i : Fin N, ∀ a_i' : AgentAction num_neighbors num_freqs num_powers,
    utility_i_coupled N num_neighbors num_freqs num_powers cfg a_opt i ≥ utility_i_coupled N num_neighbors num_freqs num_powers cfg (update_profile N num_neighbors num_freqs num_powers a_opt i a_i') i) := by

  intro h_max i a_i'
  have h_id := exact_potential_game_identity N num_neighbors num_freqs num_powers cfg a_opt i a_i'
  linarith [h_max (update_profile N num_neighbors num_freqs num_powers a_opt i a_i')]
