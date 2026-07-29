# Mathematische Charakterisierung

## Gemessene Struktur

Der kombinierte lineare, logarithmische und nullfokussierte Scan umfasst
8.045 verschiedene Eingaben. Alle Laufzeitausgaben waren endlich.

| Merkmal | Messwert |
|---|---:|
| beobachtetes Minimum | -0,9579895895 |
| beobachtetes Maximum | 1,1587012683 |
| `K(0)` | 0,1399162834 |
| Gate-Minimum im Scan | 0,1283106114 |
| Gate-Maximum im Scan | 0,9103080908 |
| größte diskret gemessene Absolutsteigung | 3,57064 × 10^11 |
| Nullstellenklammer | `[-10^-12,-10^-13]` |

## Eigenschaften

- **Definitionsbereich:** Durch `sanitize` sind alle in Python konvertierbaren
  Skalare definiert. Nichtendliche Eingaben werden wie `0` behandelt.
- **Wertebereich:** Global analytisch ist der zusammengesetzte Ausdruck
  begrenzt; im geprüften Laufzeitbereich lag er in der obigen Messspanne.
- **Stetigkeit:** Die geschützte Formel ist mathematisch stetig. Numerisch wirkt
  sie um null wie eine sehr steile Stufe.
- **Monotonie:** Global nicht streng monoton. Dominant ist aber der Übergang vom
  negativen zum positiven Plateau.
- **Symmetrie:** Annähernd affine Punktsymmetrie außerhalb der Nullumgebung,
  nicht exakt, weil `A(q)` und der verschachtelte Sinusterm gerade Anteile
  beitragen.
- **Periodizität:** Keine Periodizität in `x`, weil `q(x)` sättigt.
- **Skalierung:** Für `|x| >> 10^-6` ist fast nur das Vorzeichen informativ.
- **Translationsverhalten:** Nicht translationsinvariant; die scharfe
  Entscheidung ist an `x=0` gebunden.
- **Informationskompression:** Große Betragsbereiche kollabieren auf zwei
  Plateaus. Hohe lokale Information bleibt nur nahe null.
- **Konditionierung:** Sehr schlecht nahe null, gutmütig auf den Plateaus.
- **Schutzinterventionen:** Die Division durch `10^-6` ist stets aktiv; im
  durchgeführten Scan waren keine Ausgabe-Clamps oder Nichtendlichkeitsersetzungen
  nötig.

## Iteration

Für die Rekurrenz `x[t+1]=tanh(K(x[t]))` wurden zwei synthetische Attraktoren
beobachtet:

- negative Startwerte konvergieren nach `-0,7433783199`,
- null und positive Startwerte konvergieren nach `+0,7786811014`.

Dies ist ein reproduzierbarer numerischer Befund und kein Beleg für ein reales
biologisches Attraktorsystem.

Die vollständigen Werte stehen in
`characterization.json` und `measurements.csv`.

