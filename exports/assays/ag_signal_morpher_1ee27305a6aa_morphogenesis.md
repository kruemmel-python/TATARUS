# Algorithmic Morphogenesis Laboratory

This report evaluates an executable mathematical raw form through `genotype + insertion position + wrapper + measured system response`. The assigned source domain is provenance only. A role is retained only when its wrapper changes the system reproducibly relative to its explicit kernel-free neutral ablation.

## Genotype

- Name: `ag_signal_morpher_1ee27305a6aa`
- Formula: `K(x) = ((logabs(sin(cos(sdiv(x,x)))) - tanh(sdiv((sin(sdiv(x,x)) * -0.357064),(logabs(sin(cos(sdiv(x,x)))) - logabs(sin(cos(sdiv(x,x)))))))) - sin((logabs(sin(cos(sdiv(x,x)))) * (sin(cos(sdiv(x,x))) + cos(sin(sdiv(x,x)))))))`
- Source domain: `signal_transform`
- Assigned wrapper: `signal_morpher`
- Scanned range: `[-0.957989, 1.04203]`
- Exact-zero-like: `false`
- Input-dependent: `true`

## Executable insertion-position assays

|Rank|Position|Affected quantity|Effect|Consistency|Energy ratio|Sync Δ|Decision|
|---:|---|---|---:|---:|---:|---:|---|
|1|`update_rate_gate`|rate|1|1|4.00348e+12|377812|`retained_reproducible_effect`|
|2|`additive_state_term`|state|0.975175|0.75|6.32632|0.309739|`retained_reproducible_effect`|
|3|`pre_state_transform`|state|0.971775|1|5.64557|0.207908|`retained_reproducible_effect`|
|4|`post_state_transform`|state|0.942201|1|3.86365|-0.273832|`retained_reproducible_effect`|
|5|`convex_state_blend`|state|0.93886|1|3.77453|0.30459|`retained_reproducible_effect`|
|6|`coupling_gate`|coupling|0.69893|1|2.41978|0.35845|`retained_reproducible_effect`|
|7|`multiplicative_state_gate`|state_validity|0.686069|1|0.0642087|-0.480035|`retained_reproducible_effect`|
|8|`threshold_shift`|threshold|0.243339|0.75|0.998556|0.000992569|`retained_reproducible_effect`|
|9|`exact_zero_reset`|memory_validity|0|0|1|0|`rejected_no_measurable_effect`|

## Retained phenotype hypotheses

- `activation_threshold_modulator`
- `additive_forcing_term`
- `attractor_shaping_branch`
- `convergence_promoter`
- `coupling_interruptor`
- `dynamic_subsystem_isolator`
- `energy_suppressor`
- `sparsity_or_validity_gate`
- `state_collapse_gate`

## Wrapper definitions and ablations

### update_rate_gate

- Position: `update_rate`
- Wrapper: `x[t+1]=x[t]+Q(K(x[t]))*(F(x[t])-x[t])`
- Ablation: replace Q(K) by multiplicative neutral 1
- Trajectory RMSE: `618555`
- Intervention fraction: `0`
- Phenotypes: `convergence_promoter`

### additive_state_term

- Position: `state_increment`
- Wrapper: `x[t+1]=tanh(F(x[t])+0.35*Q(K(x[t])))`
- Ablation: replace Q(K) by additive neutral 0
- Trajectory RMSE: `0.722883`
- Intervention fraction: `0`
- Phenotypes: `additive_forcing_term`

### pre_state_transform

- Position: `input_of_system_map`
- Wrapper: `x[t+1]=Q(K(F(x[t])))`
- Ablation: replace Q(K(z)) by identity z
- Trajectory RMSE: `0.790401`
- Intervention fraction: `0`
- Phenotypes: `convergence_promoter`

### post_state_transform

- Position: `input_before_system_map`
- Wrapper: `x[t+1]=F(Q(K(x[t])))`
- Ablation: replace Q(K(x)) by identity x
- Trajectory RMSE: `0.659563`
- Intervention fraction: `0`
- Phenotypes: none

### convex_state_blend

- Position: `state_mixture`
- Wrapper: `x[t+1]=0.65*F(x[t])+0.35*Q(K(x[t]))`
- Ablation: remove kernel branch and renormalize to F(x[t])
- Trajectory RMSE: `0.657332`
- Intervention fraction: `0`
- Phenotypes: `attractor_shaping_branch`

### coupling_gate

- Position: `coupling_coefficient`
- Wrapper: `x'[t]=F(x[t])+0.25*Q(K(x[t]))*(y[t]-x[t])`
- Ablation: replace Q(K) by coupling neutral 1
- Trajectory RMSE: `0.327529`
- Intervention fraction: `0`
- Phenotypes: `coupling_interruptor` `dynamic_subsystem_isolator`

### multiplicative_state_gate

- Position: `state_gate`
- Wrapper: `x[t+1]=F(x[t])*Q(K(x[t]))`
- Ablation: replace Q(K) by multiplicative neutral 1
- Trajectory RMSE: `0.312788`
- Intervention fraction: `0`
- Phenotypes: `energy_suppressor` `sparsity_or_validity_gate` `state_collapse_gate`

### threshold_shift

- Position: `activation_threshold`
- Wrapper: `x[t+1]=F(x[t]) if |F(x[t])|>0.25+0.15*Q(K(x[t])) else 0`
- Ablation: replace Q(K) by additive neutral 0
- Trajectory RMSE: `0.0472269`
- Intervention fraction: `0.0201823`
- Phenotypes: `activation_threshold_modulator`

### exact_zero_reset

- Position: `reset_condition`
- Wrapper: `x[t+1]=0 if |Q(K(x[t]))|<=1e-12 else F(x[t])`
- Ablation: kernel-absent path never triggers reset
- Trajectory RMSE: `0`
- Intervention fraction: `0`
- Phenotypes: none

## Reproducibility

- Base seed: `38`
- Trials per insertion position: `16`
- Steps per trajectory: `192`
- Effect threshold: `0.05`
- Directional-consistency threshold: `0.75`
- Kernel projection: `Q(K)=tanh(sanitized_runtime_K)`
- Base system: `F(x,d)=tanh(0.82x+0.14sin(1.7x)+d)`

## Boundaries

- Retained roles are executable system-effect hypotheses, not novelty or utility claims.
- The fixed assay worlds are synthetic; retained effects require independent systems and domain-specific baselines.
