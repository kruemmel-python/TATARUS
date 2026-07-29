# Forschungsstufe 3: kausale Vier-Feature-Projektion

## Implementierung

Jeder Spike speichert nun neben Quelle, Zeitpunkt, Amplitude und Gate vier
kausal am Schwellenübertritt berechnete Komponenten:

```text
b   = (I_exc-I_inh) / (|I_exc|+|I_inh|+1e-9)
v   = tanh((V(t)-V(t-1)) / dt / s_v)
o   = tanh((V(t)-theta(t)) / s_o)
r   = 2*exp(-ISI/tau_ISI)-1
```

Für den ersten Spike eines Neurons gilt `r=-1`. Die voreingestellte skalare
Projektion ist:

```text
phi = 0.40*b + 0.25*v + 0.15*o + 0.20*r
s_v = 1 mV/ms
s_o = 1 mV
tau_ISI = 50 ms
```

Alle Gewichte und Skalen sind in Python, C++ und der nativen UI explizite
Parameter. Die UI verwendet dafür die semikolongetrennte Eingabe
`aEI;aV;aO;aISI;sV;sO;tISI`.

## Versuchsaufbau

- fünf Netzwerk-Seeds: `11, 23, 38, 53, 71`
- Dynamikassay: 16 Neuronen, 420 Schritte, STDP aus
- Reihenfolgeerkennung: 12 Samples je Klasse, drei Folds, 120 Schritte
- Komponentenablationen: E/I, Membransteigung, Schwellenüberschuss und ISI
- Kontrollen: jeweilige Event-Konstanten, Originalprojektion als Konstante,
  Vorzeichen, `tanh`, verteilungsgematchtes Zufallsgate, zeitverschoben,
  state-shuffled und deaktiviert

## Ergebnis

| Variante | Accuracy | Gate-Varianz | Gate-Entropie | Assembly-Separation | Spikes/korrekte Entscheidung |
|---|---:|---:|---:|---:|---:|
| Vier-Feature-Kernel | 90,83 % | 0,08758 | 0,6850 bit | 3,1124 | 7,9600 |
| Event-Konstante der Projektion | 88,33 % | 0 | 0 bit | 2,9578 | 10,9533 |
| Verteilungsgematchtes Zufallsgate | 90,83 % | 0,09196 | 0,7093 bit | 2,7051 | 10,6055 |
| Vorzeichenprojektion | 91,67 % | 0,09746 | 0,6883 bit | 3,0634 | 7,7697 |
| `tanh`-Projektion | 90,00 % | 0,07940 | 1,2944 bit | 2,7736 | 9,3850 |
| E/I allein | 84,17 % | 0,06980 | 0,8546 bit | 2,6128 | 11,6383 |
| ISI allein | **95,83 %** | `1,45e-14` | numerisch 1,5317 bit | **3,3918** | **5,2958** |
| ISI-Event-Konstante | **95,83 %** | 0 | 0 bit | **3,3918** | **5,2958** |
| ISI-Verteilungskontrolle | **95,83 %** | `1,33e-14` | 1,4812 bit | **3,3918** | **5,2958** |
| deaktiviert | 81,67 % | 0 | 0 bit | 2,6487 | 14,0144 |

## Interpretation

Die kombinierte Projektion ist eindeutig dynamisch und übertrifft ihre
event-gematchte Konstante in dieser Aufgabe um `2,50` Prozentpunkte. Sie
reduziert zugleich die Spikezahl je korrekter Entscheidung von `10,9533` auf
`7,9600`.

Ein spezifischer Vorteil des generierten Operators ist dennoch nicht belegt:

- das verteilungsgematchte Zufallsgate erreicht dieselbe Accuracy,
- das einfache Vorzeichengate ist leicht besser,
- der scheinbar starke ISI-Kernel ist gegenüber seiner eigenen
  Event-Konstante und Verteilungskontrolle exakt wirkungsgleich.

Die ISI-Variante ist damit kein Nachweis dynamischer Kernelgeometrie, sondern
erneut ein sehr wirksamer nahezu konstanter Dämpfungsphänotyp.

## Entscheidung

`KEEP_AND_RESEARCH`

Die Vier-Feature-Ereignisstruktur bleibt als neue technische Basis erhalten.
Bevor getrennte Synapsenklassen evolviert werden, sollten die
Projektionsgewichte auf Trainingsfolds optimiert und ausschließlich auf
unberührten Seeds beziehungsweise Aufgaben bewertet werden. Die Optimierung
muss Konstant-, Vorzeichen- und Verteilungskontrollen als konkurrierende
Modelle einschließen.
