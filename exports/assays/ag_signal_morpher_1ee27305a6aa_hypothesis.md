# Algorithmic Genesis Hypothesis Laboratory

## Interpretation

This report treats the generated expression as an executable mathematical hypothesis. Its source domain is provenance, not a restriction and not evidence of best fit. The scans below identify falsifiable research directions; they do not prove novelty, causality, superiority or deployment readiness.

## Artifact

- Name: `ag_signal_morpher_1ee27305a6aa`
- Formula: `K(x) = ((logabs(sin(cos(sdiv(x,x)))) - tanh(sdiv((sin(sdiv(x,x)) * -0.357064),(logabs(sin(cos(sdiv(x,x)))) - logabs(sin(cos(sdiv(x,x)))))))) - sin((logabs(sin(cos(sdiv(x,x)))) * (sin(cos(sdiv(x,x))) + cos(sin(sdiv(x,x)))))))`
- Source domain: `signal_transform`
- Generator-assigned kind: `signal_morpher`
- Characterization status: `empirically_characterized`
- Novelty status: `unassessed`
- Domain-fit status: `not_assumed`

## Evaluation layers

|Layer|Finite ratio|Interventions|Interpretation|
|---|---:|---:|---|
|Intrinsic raw AST|0.999024|0|Formula semantics without protected operators or sanitization; boundedness: `not_established`|
|Protected operators|1|16512|Protected division/logarithm/exponential semantics only|
|Sanitized runtime|1|0|Input, intermediate and output containment; recurrence additionally uses `tanh`|

Runtime interventions: non-finite replacements `0`, input clamps `0`, intermediate clamps `0`, output clamps `0`.

## Affine symmetry and radial shell geometry

- Measured symmetry center: `(0, 0.139916)`
- Centered odd score: `0.90219`
- Affine point-symmetry score: `0.90219`
- Maximum residual in `K(x)+K(-x)-2K(0)`: `0.195812`
- Shell descriptor: `no_clear_shell_geometry`
- Radial even score of `|K(x)-K(0)|`: `0`
- Outer log correlation / slope: `-0.656175` / `-2.83787e-09`
- Reference radii: `0`
- Response-peak radii: `1.32754e-06`

## Candidate functional roles

|Rank|Role|Affinity|Evidence|
|---:|---|---:|---|
|1|`centered_odd_transform`|0.902|K(x)-K(0) is antisymmetric around the measured center|
|2|`affine_point_symmetric_transform`|0.902|K(x)+K(-x)=2*K(0) within the reported numeric residual|
|3|`numerically_safe_nonlinear_transform`|0.807|bounded-after-runtime execution; this score includes protected operators and sanitization|
|4|`distribution_separator`|0.512|direction-neutral AUC weighted by standardized effect size|
|5|`tail_or_transition_pressure`|0.057|response contrast between central observations and rare tails|
|6|`even_symmetry_invariant`|0.053|agreement of K(x) and K(-x)|
|7|`iterated_dynamics_map`|0.050|attractor diversity, convergence and Lyapunov-like response under x[t+1]=tanh(K(x[t]))|
|8|`oscillatory_feature_map`|0.015|turning-point and zero-crossing density across scanned scales|
|9|`scale_selective_sensor`|0.000|behavior changes when the same normalized inputs are rescaled|
|10|`signed_logarithmic_shell_transform`|0.000|centered sign response, radial shell references and logarithmic outer growth|
|11|`dual_reference_radial_separator`|0.000|multiple low-response radii in |K(x)-K(0)||

## Domain-neutral separation probes

|Probe|Contrast|Best feature|Input scale|AUC|Raw-input ablation|Gain|95% CI|Effect|
|---|---|---|---:|---:|---:|---:|---:|---:|
|tail_separation|central normal observations vs rare symmetric tails|`signed_output`|0.100|0.762|0.999|-0.237|0.754–0.770|0.061|
|location_shift|zero-centered observations vs shifted location|`distance_from_K(0)`|0.000|0.861|0.861|-0.000|0.854–0.867|1.222|
|variance_expansion|reference variance vs expanded variance|`signed_output`|1.000|0.636|0.760|-0.124|0.622–0.649|0.082|
|skew_transition|symmetric observations vs centered skew distribution|`absolute_output`|1.000|0.597|0.552|0.045|0.581–0.612|0.301|
|quantization_transition|continuous values vs quantized values|`absolute_output`|0.000|0.567|0.507|0.060|0.559–0.575|0.530|
|boundary_layer|central interval vs boundary-layer observations|`signed_output`|1000000.000|0.760|1.000|-0.240|0.750–0.769|0.070|

AUC separation is direction-neutral: `0.5` means no scalar threshold separation, `1.0` means complete separation in the scanned synthetic contrast.

## Scale-space geometry

|Scale|Range|Std.dev.|Variation|Max slope|Local sensitivity concentration|Zero crossings|Extrema|Centered odd|Scale similarity|
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
|0.000|2.019|1.000|2.038|512000222.306|25716.443|0.001|0.001|0.990|1.000|
|0.000|2.111|1.000|2.255|5135119.139|19305.892|0.001|0.003|0.929|1.000|
|0.010|2.033|1.000|2.066|54523.978|1796328.125|0.001|0.001|0.904|1.000|
|0.100|2.004|1.000|2.008|5601.698|18432835.037|0.001|0.001|0.902|1.000|
|1.000|2.000|1.000|2.001|561.929|184884775.318|0.001|0.001|0.902|1.000|
|10.000|2.000|1.000|2.000|56.211|1849354189.465|0.001|0.001|0.902|1.000|
|100.000|2.000|1.000|2.000|5.621|18434029148.166|0.001|0.001|0.902|1.000|
|10000.000|2.000|1.000|2.000|0.056|54554485023.768|0.001|0.001|0.902|1.000|
|1000000.000|2.000|1.000|2.000|0.001|562126081.247|0.001|0.001|0.902|1.000|

## Iterated dynamics probe

- Recurrence: `x[t+1]=tanh(K(x[t]))`
- Convergence fraction: `1.000`
- Period-two fraction: `0.000`
- Lyapunov-like mean: `-14.412`
- Distinct terminal attractors: `2`

## Boundaries and warnings

- The source domain 'signal_transform' is provenance only; this report does not assume that it is the formula's best application domain.
- A narrow high-gradient boundary layer was observed. Dense precision and perturbation tests are required around protected operator transitions.
- Protected operators improve finiteness relative to the raw AST; safety-layer behavior must not be attributed to the intrinsic formula alone.
- Separability scans generate research hypotheses, not proof of novelty, causality or deployment readiness.

## Falsifiable next experiments

- Does the distance_from_K(0) response near input scale 0.0001 retain its location_shift separation on independent real measurements?
- Does the formula improve AUC beyond its best raw-input ablation by more than -2.67029e-05 on a preregistered external contrast?
- Can the leading role 'centered_odd_transform' outperform an ablated wrapper in a preregistered experiment?
- Which physical units and normalization map real observations into the informative input scales?
- Are the observed effects stable across seeds, acquisition systems, drift and numeric backends?

## Reproducibility

- Seed: `38`
- Samples per scale: `1025`
- Trials per separation probe: `16`
- Probe AUC: direction-neutral Mann–Whitney rank statistic
- Confidence interval: normal approximation over independent trial AUC values
- Every winning probe records its class distributions, parameters, scale index, trial seeds, raw-input ablation and a 32-bin pooled histogram in the JSON report.
