# Delayed-XOR-Einzelablationen

Eligibility-Zeiten: `50;100;200 ms`

| Variante | Accuracy | Spikes/korrekt | ΔAccuracy zum Vollmodell | ΔKosten zum Vollmodell | p Kosten (Holm konservativ) |
|---|---:|---:|---:|---:|---:|
| full_kernel | 0.892578 | 5.709169 | 0.000000 | 0.000000 | 1.000000 |
| without_dendrite | 0.785807 | 76.905428 | 0.106771 | 71.196259 | 0.000080 |
| without_eligibility | 0.548828 | 9.380449 | 0.343750 | 3.671280 | 0.000072 |
| without_products | 0.475911 | 10.995619 | 0.416667 | 5.286450 | 0.000056 |
| sign_gate | 0.891927 | 5.704541 | 0.000651 | -0.004628 | 1.000000 |

Positive ΔAccuracy/ΔKosten bedeuten einen Vorteil des Vollmodells. Jede Zeile ändert genau einen Mechanismus.
