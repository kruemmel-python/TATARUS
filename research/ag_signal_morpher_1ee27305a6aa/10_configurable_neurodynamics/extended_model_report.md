# Forschungsstufe 4: konfigurierbares neurodynamisches System

## Ziel

Die zuvor fest codierten Modellannahmen wurden zu expliziten
Forschungsparametern. Zusätzlich wurden die in der UI-Dokumentation als
fehlend ausgewiesenen Mechanismen in die native C++-Engine integriert.

## Implementierte Mechanismen

- alle Membran-, Schwellen-, Refraktär- und Adaptationsparameter editierbar,
- E/I-Anteil, Anfangsgewichte und STDP-Parameter editierbar,
- Stimulus- und Readoutparameter editierbar,
- individuelle seed-deterministische Axonverzögerung je Verbindung,
- strombasierte Referenzsynapse,
- leitwertbasierte AMPA-/GABA-Synapse mit Umkehrpotentialen,
- passives Dendritenkompartiment je Neuron,
- getrennte Operatoren für `EE`, `EI`, `IE`, `II`,
- Mehrseed-Vergleich,
- zweiseitiger gepaarter Sign-Flip-Permutationstest,
- Projektionsoptimierung mit getrennten Trainings-Seeds und Holdout-Seed.

## Erweiterter UI-Standard

```text
dt/tauM/tauS                 = 1/20/5 ms
Ruhe/Reset/Schwelle          = -65/-70/-50 mV
Axonverzögerung              = 1...5 ms
Synapsenmodell               = AMPA/GABA
AMPA/GABA-Umkehrpotential    = 0/-75 mV
Leitwertskalierung E/I       = 0.02/0.02
Dendrit                      = aktiv
Dendrit tau/Kopplung         = 30 ms / 0.20
externer Dendriteninput      = 0
Basisstrom                   = 15
Emissionsfeature             = Vier-Feature-Projektion
Klassenoperatoren            = aktiviert
EE/EI/IE/II                  = kernel/kernel/kernel/kernel
```

## Nativer Referenzlauf

Ein MSVC-Referenzlauf mit Seed `38`, 16 Neuronen, 24 Samples je Klasse und
vier Folds ergab:

| Gate | Accuracy | Feuerrate | Assembly-Separation | Spikes/korrekte Entscheidung |
|---|---:|---:|---:|---:|
| Originalkernel | 97,92 % | 3,700 Hz | 3,4777 | 7,2553 |
| Event-Konstante | 97,92 % | 3,754 Hz | 3,5082 | 7,3617 |
| deaktiviert | 97,92 % | 4,080 Hz | 3,5042 | 8,0000 |
| Vorzeichen | 97,92 % | 3,668 Hz | 3,4682 | 7,1915 |
| `tanh` | 100,00 % | 3,754 Hz | 3,5090 | 7,2083 |
| Verteilungszufall | 97,92 % | 3,700 Hz | 3,4748 | 7,2553 |

Dieser einzelne Seed dient nur als Funktions- und Stabilitätsnachweis. Er
belegt keinen einzigartigen Kernelvorteil; die einfache `tanh`-Kontrolle ist
hier stärker.

## Wissenschaftliche Einordnung

Das System verhält sich als rekurrentes, verzögertes, adaptives,
ereignisgetriebenes Netzwerk mit:

- Erregung und Hemmung,
- leitwertabhängiger treibender Kraft,
- dendritischer zeitlicher Integration,
- axonaler Leitungslaufzeit,
- Refraktär- und Adaptationsdynamik,
- lokaler Plastizität,
- populationsspezifischer synaptischer Modulation.

Dies sind wesentliche Funktionsprinzipien von Nervensystemen. Das Modell ist
dennoch kein validierter digitaler Organismus. Es fehlen unter anderem aktive
Dendritenäste, mehrere Rezeptorkinetiken, Neuromodulation, Glia,
strukturelle Plastizität, Sensorik und Aktorik.

## Rückfallpfad

Die frühere Referenz bleibt erreichbar:

```text
Axonverzögerung = 1;1
Synapsenmodell  = Strombasiert
Dendrit         = aus
Klassenoperatoren = aus
Basisstrom      = 12.5
```

Damit können neue Mechanismen einzeln gegen die historische Architektur
abgetragen werden.

## Entscheidung

`KEEP_AND_RESEARCH`
