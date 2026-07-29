# ag_signal_morpher_1ee27305a6aa

## Research interpretation

This artifact is an executable mathematical hypothesis. Its source domain and assigned wrapper describe how it was generated; they do not restrict the formula to that domain or prove that this is its best use. See the accompanying `_hypothesis.md` report for domain-open formula characterization and `_morphogenesis.md` for executable insertion-position assays against neutral kernel-free ablations.

## Purpose

A generated streaming transform that mixes an input signal with the discovered nonlinear kernel and a local memory term.

## Mathematical Kernel

`K(x) = ((logabs(sin(cos(sdiv(x,x)))) - tanh(sdiv((sin(sdiv(x,x)) * -0.357064),(logabs(sin(cos(sdiv(x,x)))) - logabs(sin(cos(sdiv(x,x)))))))) - sin((logabs(sin(cos(sdiv(x,x)))) * (sin(cos(sdiv(x,x))) + cos(sin(sdiv(x,x)))))))`

## Contract

Given a finite signal, the algorithm returns a finite transformed signal of the same length. Each output is sanitized and bounded.

## Pseudocode

```text
state=0; for each sample s: k=K(s+0.25*state); y=0.70*s+0.30*tanh(k); state=0.8*state+0.2*y; emit sanitize(y)
```

## Complexity

Time O(iterations * 17) for iterative modes or O(n * 17) for vector modes; memory O(1) streaming, O(n) only when an output vector is requested.

## Validation

Validate finite outputs, gain bounds, impulse response, step response and spectral distortion against identity and tanh baselines.

## Exported Files

- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\generated\ag_signal_morpher_1ee27305a6aa_kernel.py`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\generated\ag_signal_morpher_1ee27305a6aa_kernel.hpp`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\generated\ag_signal_morpher_1ee27305a6aa_kernel.cl`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\wrappers\ag_signal_morpher_1ee27305a6aa_wrapper.py`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\wrappers\ag_signal_morpher_1ee27305a6aa_support.hpp`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\wrappers\ag_signal_morpher_1ee27305a6aa_kernels.cl`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\assays\ag_signal_morpher_1ee27305a6aa_assay_platform.py`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\assays\test_ag_signal_morpher_1ee27305a6aa_assay_platform.py`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\assays\ag_signal_morpher_1ee27305a6aa_hypothesis.json`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\assays\ag_signal_morpher_1ee27305a6aa_hypothesis.md`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\assays\ag_signal_morpher_1ee27305a6aa_morphogenesis.json`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\assays\ag_signal_morpher_1ee27305a6aa_morphogenesis.md`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\ag_signal_morpher_1ee27305a6aa_manifest.json`
- `C:\Users\ralfk\AppData\Local\Algorithmic Genesis Studio\Runs\20260728_115342_221\exports\ag_signal_morpher_1ee27305a6aa_README.md`
