import Lake
open Lake DSL

package «lpd_sigint» where
  -- Settings applied to both it and targets
  leanOptions := #[
    ⟨`pp.unicode.fun, true⟩ -- pretty-prints arrows
  ]

require mathlib from git
  "https://github.com/leanprover-community/mathlib4.git" @ "v4.15.0"

@[default_target]
lean_lib «LpdSigint» where
  -- add library configuration options here
