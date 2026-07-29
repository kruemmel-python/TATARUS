# TATARUS

## A Persistent Synthetic Nervous System for Artificial Intelligence

### Ein mathematisch definiertes, biologisch inspiriertes Substrat für kontinuierliche Wahrnehmung, lokales Gedächtnis, adaptive Handlung und strukturelle Selbstreparatur

**Whitepaper · Deutsche Ausgabe · Version 1.0**  
**Softwarestand:** TATARUS 1.0.0  
**Entwickler und Autor:** Ralf Krümmel  
**Datum:** 29. Juli 2026  
**Lizenz:** Apache License 2.0  
**Repository:** <https://github.com/kruemmel-python/TATARUS>

> **TATARUS versucht nicht, ein biologisches Nervensystem materiell zu
> kopieren. Es überträgt ausgewählte funktionale Prinzipien biologischer
> Nervensysteme in ein künstliches mathematisches Substrat, damit eine KI
> nicht nur ein Modell ausführt, sondern einen eigenen fortlaufenden,
> lernenden und handlungswirksamen Innenzustand besitzt.**

Dieses Whitepaper beschreibt Architektur, Mathematik, Implementierung,
experimentelle Evidenz und Grenzen des veröffentlichten Systems. Sämtliche
Leistungsangaben beziehen sich auf die dokumentierten synthetischen
Versuchsdomänen.

<div align="right"><sub>Seite 1 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 02/30 -->

## Publikations- und Anspruchsrahmen

TATARUS ist **keine biologische 1:1-Simulation** eines menschlichen oder
tierischen Nervensystems. Es bildet keine vollständige Anatomie,
Molekularbiologie, Genexpression oder Neurochemie nach. Das Projekt erhebt
keinen Anspruch auf Bewusstsein, Empfindungsfähigkeit, biologische Identität
oder allgemeine Intelligenz.

TATARUS ist zugleich **mehr als eine biologische Metapher**. Neuronen,
Synapsen, Rezeptoren, Dendriten, Eligibility-Spuren, Energie, Homeostase,
Assemblies und Topologie sind numerische Zustände mit expliziten
Übergangsregeln. Sie verändern die weitere Systemdynamik kausal und werden in
Snapshots fortgesetzt.

> **Definition.** Ein synthetisches Nervensystem ist ein dauerhaft
> fortgesetztes mathematisch-algorithmisches System, dessen interne
> neuronale, synaptische, regulatorische und strukturelle Zustände durch
> Erfahrung verändert werden und dadurch zukünftige Wahrnehmung, Erinnerung,
> Planung und Handlung beeinflussen.

Die Software ist unter Apache 2.0 offen veröffentlicht. Die Whitepaper-Aussagen
werden in drei Evidenzklassen getrennt:

| Kennzeichnung | Bedeutung |
|---|---|
| **implementiert** | im veröffentlichten Quellcode ausführbar |
| **bestätigt** | eingefrorenes Kriterium auf getrennten synthetischen Seeds erfüllt |
| **offen** | Hypothese, Transferfrage oder externe Replikation ausstehend |

„Bestätigt“ bedeutet in diesem Dokument niemals biologische Validierung. Eine
formale Neuheits-, Patent- oder vollständige Literaturprüfung ist ebenfalls
nicht Gegenstand dieses Whitepapers.

### Inhalt

1. Positionierung und Forschungsziel — Seiten 3–6  
2. Architektur und Mathematik — Seiten 7–17  
3. Versuchsdesign und Evidenz — Seiten 18–25  
4. Einordnung, Grenzen und Open Science — Seiten 26–28  
5. Forschungsprogramm und Referenzen — Seiten 29–30

<div align="right"><sub>Seite 2 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 03/30 -->

## Abstract

TATARUS ist ein persistentes synthetisches Nervensystem, das als interner,
erfahrungsabhängiger Rechenzustand für künstliche Intelligenz entwickelt
wurde. Anstelle einer anatomischen Rekonstruktion überträgt die Architektur
ausgewählte Organisationsprinzipien biologischer Nervensysteme in
mathematisch definierte, ausführbare Algorithmen.

Das System kombiniert erregende und hemmende Populationen, Soma- und
Dendritenzustände, AMPA-, NMDA-, GABA-A- und GABA-B-Leitwerte, individuelle
Axonverzögerungen, lokale synaptische Spuren, Kurzzeitressourcen,
Neuromodulation, Aktivitäts- und Energiehomeostase, Assemblybildung,
Konsolidierung, kontrollierten Zerfall und strukturelle Reparatur. Eine
beschränkte **Cognitive Bridge** koppelt diesen Zustand an einen höheren
Planungskern, ohne einzelne Neuronen, Synapsen, Gewichte oder
Eligibility-Werte offenzulegen.

Auf eingefrorenen synthetischen Holdout-Aufgaben wurden reizspezifische
Repräsentationen, rohe zeitliche Übergangsstruktur, trace-essential Recall,
erfahrungsabhängige Handlung, mehrskaliges Gedächtnis und
provenienzgestützte Funktionsreparatur bestätigt. Eine vollständige
Skalierungsausführung umfasste 65.536 Neuronen und 2.097.328 aktive Synapsen
mit exakter Snapshot-Restaurierung.

Die Ergebnisse stützen TATARUS als funktionale synthetische
Nervensystemarchitektur innerhalb der dokumentierten Domänen. Sie belegen
weder biologische Gleichwertigkeit noch Bewusstsein, universelle
Weltgeneralisation, allgemeine Intelligenz oder eine allgemeine Überlegenheit
des generierten Operators. Die unabhängige Ausführung des vorbereiteten
Replikationspakets auf zweiter Hardware steht aus.

**Schlüsselwörter:** synthetisches Nervensystem, Spiking Neural Network,
Eligibility Memory, strukturelle Plastizität, Cognitive Bridge,
kontinuierlicher Innenzustand.

<div align="right"><sub>Seite 3 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 04/30 -->

## Executive Summary I – Problem und Ansatz

Viele KI-Anwendungen werden als Abbildung eines Eingabeobjekts auf eine
Ausgabe organisiert. Kontextfenster, rekurrente Zustände und externe Speicher
können diese Abbildung erweitern, sind aber nicht automatisch ein
fortlaufendes, lokal plastisches Nervensystem. TATARUS untersucht eine andere
Architekturfrage:

> Kann eine KI einen eigenen, dauerhaft fortgesetzten Innenzustand besitzen,
> der sich durch Wahrnehmung, Handlung und Konsequenz lokal verändert?

Die konventionelle Kurzform lautet:

```text
Eingabe → Repräsentation/Modell → Ausgabe
```

TATARUS schließt dagegen den kausalen Kreis:

```text
Rohereignis → Nervenzustand → Cognitive Bridge → Planung → Handlung
      ↑                                                    ↓
      └──────────── Umwelt und Konsequenz/Reward ───────────┘
```

Der Zustand wird zwischen Erfahrungen nicht zurückgesetzt. Ein neuer
Zeitschritt setzt Membranen, Rezeptorleitwerte, Axonqueues, synaptische
Ressourcen, Eligibility, Gewichte, Energie, Assemblies und Topologie fort.
Damit gilt im Normalbetrieb:

$$
\mathcal S_{t+1}\neq \mathcal S_0.
$$

Die zentrale Designhypothese ist:

$$
P(a\mid x_t,\mathcal S_t^{\text{erfahren}})
\neq
P(a\mid x_t,\mathcal S_t^{\text{unerfahren}}),
$$

obwohl der aktuell beobachtbare Reiz \(x_t\) identisch ist. Frühere Erfahrung
soll nicht nur nachgeschlagen werden; sie soll die Rechenbedingungen der
Gegenwart verändern.

<div align="right"><sub>Seite 4 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 05/30 -->

## Executive Summary II – Erreichte Evidenz

Die Veröffentlichung integriert die Forschungsstufen 1 bis 23. Die
Entwicklungsstufen dienen als Evidenzkette, nicht als Produktdefinition.

| Fähigkeit | Ergebnis | Geltungsbereich |
|---|---:|---|
| persistenter C++-Kern | ausführbar und snapshotfähig | synthetischer Simulator |
| konkurrierende Repräsentationen | 8/8 Seeds | eingefrorene Reizfamilie |
| rohe Übergänge und Grenzen | 8/8 Seeds; 77,3438 % | sechs Übergangsklassen |
| trace-essential Recall | 100 % vs. 48,6111 % | 12 Holdout-Netze |
| Funktionsreparatur | 8/8 Seeds | definierter Sensor-Motor-Pfad |
| KI-Nervensystem-Kopplung | 100 % vs. 51,5625 % / 50 % | 8 Lebenslauf-Seeds |
| prozedurale Lebenswelt | 6/8 Einzelkriterien | Weltfamilie und G5 |
| mehrskaliges Gedächtnis | 8/8 Seeds | synthetische Gedächtnistests |
| Skalierung | 65.536 Neuronen | Integrität, nicht Echtzeit |
| externe Replikation | vorbereitet | zweite Hardware ausstehend |

Die stärkste zulässige Gesamtaussage lautet:

> TATARUS vereinigt persistente neurodynamische Zustände, lokale
> Gedächtnisspuren, Selbstregulation, handlungswirksame Kopplung und
> strukturelle Veränderung in einem ausführbaren, kausal testbaren
> synthetischen Nervensystem.

Die Ergebnisse tragen **keine** Aussage, dass der spezielle
Algorithmic-Genesis-Operator allgemein besser als einfache Gates ist. In
mehreren Aufgaben war das Vorzeichengate gleichwertig oder sparsamer; die
Delayed-XOR-Effizienzreplikation war negativ. Diese negativen Befunde bleiben
Teil der veröffentlichten Evidenz.

<div align="right"><sub>Seite 5 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 06/30 -->

## Forschungsziel und Systemkriterien

Das Ziel ist nicht, „ein Gehirn zu kopieren“, sondern einer KI ein
funktionales Nervensystem zu geben. Dafür wurden neun Systemkriterien
festgelegt:

1. **Kontinuität:** kein impliziter Reset zwischen Erfahrungen.
2. **Kausalität:** Zustände werden an der korrekten Ereignisposition gelesen.
3. **Lokalität:** synaptische Erinnerungen gehören zu einer konkreten Kante.
4. **Mehrskaligkeit:** schnelle Dynamik, Spuren, Konsolidierung und Struktur.
5. **Regulation:** Aktivität und Energie bleiben begrenzt.
6. **Verkörperte Kopplung:** Wahrnehmung beeinflusst Handlung und deren Folge
   kehrt als Reiz oder Reward zurück.
7. **Beschränkter Zugriff:** der Planer sieht nur funktionale Poolzustände.
8. **Reproduzierbarkeit:** Seeds, Zustands-Hashes und Snapshots sind prüfbar.
9. **Falsifizierbarkeit:** Mechanismen treten gegen geeignete Kontrollen an.

Diese Kriterien unterscheiden TATARUS sowohl von einem stateless
Eingabe-Ausgabe-Modell als auch von einem bloßen Dateispeicher. Ein externer
Speicher enthält abrufbare Daten. Ein Nervensystemzustand verändert dagegen
die Antwortfunktion selbst:

$$
\pi_{t+1}(a\mid x)
=
\Pi\!\left(x,\mathcal S_{t+1}\right),
\qquad
\mathcal S_{t+1}=F(\mathcal S_t,x_t,a_t,r_t).
$$

Der Zustand ist damit nicht nur Inhalt, sondern Teil des jeweils nächsten
Rechenwegs.

<div align="right"><sub>Seite 6 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 07/30 -->

## Gesamtarchitektur

```mermaid
flowchart LR
    ENV["Prozedurale oder technische Umwelt"]
    SEN["Rohe Sensorereignisse<br/>Bild · Audio · Berührung · Bytes · Interozeption"]
    NS["TATARUS<br/>persistentes synthetisches Nervensystem"]
    BR["Cognitive Bridge<br/>beschränkte funktionale Projektion"]
    PLAN["Höherer Planungs- oder Reasoningkern"]
    ACT["Motorik · Aufmerksamkeit · Lautgebung"]
    CONSEQ["Konsequenz · Reward · Neuheit"]

    ENV --> SEN --> NS --> BR --> PLAN --> ACT --> ENV
    ENV --> CONSEQ --> NS
    PLAN -->|"Intent · Recall-Cue · Aufmerksamkeit"| BR
    BR -->|"Kontextkanal, kein Einzelzellzugriff"| NS
```

Der Simulator besitzt sensorische, exzitatorische, inhibitorische,
Kontext-, motorische und modulatorische Populationen. Rohkanäle werden in
überlappende exzitatorische Mikroassemblies projiziert. Rekurrente Dynamik
transformiert diese Ereignisse; Motorpopulationen erzeugen kontinuierliche
Aktionswerte.

Die Cognitive Bridge ist eine Architekturschranke. Sie liefert aktive
Repräsentationen, gepoolte Recall-Kanäle, Neuheit, Salienz, Energie- und
Aktivitätsbedarf, Vorhersagefehler und Konfidenz. Der Planer darf
Aufmerksamkeitsziel, motorische Absicht, Recall-Cue und Reward zurückgeben.

Die Schichten sind funktional gekoppelt, bleiben aber getrennt testbar. So
können Kontrollen das Nervensystem, Eligibility oder den höheren Planer
gezielt entfernen, ohne die übrige Versuchsanordnung zu verändern.

<div align="right"><sub>Seite 7 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 08/30 -->

## Biologische Inspiration und bewusste Abstraktion

| Biologisches Organisationsprinzip | TATARUS-Abstraktion |
|---|---|
| erregende und hemmende Zellen | Dale-konforme Populationen und Gewichtsvorzeichen |
| Membran- und Dendritendynamik | Soma plus passives Dendritenkompartiment |
| schnelle/langsame Rezeptoren | AMPA, NMDA, GABA-A, GABA-B |
| axonale Laufzeit | individuelle ganzzahlige Ereignisverzögerung |
| kurzzeitige Freisetzungsdynamik | Ressource, Facilitation, Freisetzungswahrscheinlichkeit |
| lokale zeitliche Plastizität | signierte Eligibility pro aktiver Synapse |
| Neuromodulation | Dopamin-/Acetylcholin-ähnliche Regulationszustände |
| Aktivitätsstabilität | Zielratenhomeostase und Schwellendrift |
| Gedächtniskonsolidierung | reward-gebundene Gewichts- und Konsolidierungszustände |
| Zellverbände | kompetitive zeitliche Assembly-Prototypen |
| Umbau und Reparatur | Pruning, Wachstum und Eltern-Provenienz |

```mermaid
flowchart TB
    BIO["Biologisches Prinzip"] --> ABS["funktionale Abstraktion"]
    ABS --> MATH["begrenzte mathematische Zustandsregel"]
    MATH --> CODE["deterministische C++-Ausführung"]
    CODE --> TEST["Kausaltest und Kontrolle"]
```

Bewusst nicht modelliert werden vollständige Molekularbiologie,
Genexpression, Gliazellen in biologischer Detailtiefe, dreidimensionale
Gehirnanatomie, reale Entwicklung, Bewusstsein und biologische Identität.

TATARUS übernimmt somit nicht die materielle Ausführung eines Nervensystems.
Es abstrahiert ausgewählte Funktionsprinzipien so weit, dass sie ausführbar,
abschaltbar, messbar und gegen Kontrollen prüfbar werden.

<div align="right"><sub>Seite 8 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 09/30 -->

## Formales Gesamtmodell

Der vollständige Zustand entwickelt sich diskret mit \(\Delta t=1\,\mathrm{ms}\):

$$
\boxed{
\mathcal S_{t+1}
=
F(\mathcal S_t,X_t,C_t,R_t;\Theta,K,\xi)
}
$$

mit Sensorereignissen \(X_t\), Cognitive-Bridge-Befehlen \(C_t\),
Konsequenzsignalen \(R_t\), Parametern \(\Theta\), generiertem Operator \(K\)
und geseedetem Zufallszustand \(\xi\). Der Zustand kann zerlegt werden als:

$$
\mathcal S_t=
\left(
V_t,D_t,G_t,W_t,E_t,U_t,H_t,A_t,Q_t,P_t,\Xi_t
\right).
$$

| Symbol | Inhalt |
|---|---|
| \(V_t\) | Somapotentiale, Adaptation und Refraktärzustände |
| \(D_t\) | dendritische Potentiale |
| \(G_t\) | Rezeptorleitwerte |
| \(W_t\) | aktive und konsolidierte Gewichte sowie Topologie |
| \(E_t\) | lokale Eligibility-Spuren |
| \(U_t\) | Ressourcen, Facilitation und Nutzungszustände |
| \(H_t\) | Aktivitätshomeostase und Neuromodulation |
| \(A_t\) | Assemblies und Reizphasenakkumulatoren |
| \(Q_t\) | neuronale Energie |
| \(P_t\) | Axonqueues und Verzögerungen |
| \(\Xi_t\) | RNG-, Bridge- und optionale Planerzustände |

Ein V9-Snapshot serialisiert die für exakte Fortsetzung benötigten
Bestandteile. Die Reproduktion verlangt nicht nur gleiche Endmetriken,
sondern identische Zustands-Hashes und anschließende Handlungen.

<div align="right"><sub>Seite 9 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 10/30 -->

## Neuron, Dendrit und Rezeptorströme

TATARUS verwendet einen kontrollierbaren Integrate-and-Fire-Kern. In
vereinfachter Schreibweise:

$$
\tau_D\frac{dD_i}{dt}
=
(V_\mathrm{rest}-D_i)+I_i^\mathrm{syn},
$$

$$
\tau_V\frac{dV_i}{dt}
=
(V_\mathrm{rest}-V_i)
\kappa(D_i-V_i)
I_i^\mathrm{base}
I_i^\mathrm{ext}.
$$

Die synaptische Wirkung ist leitwertbasiert:

$$
I_i^\mathrm{syn}
=
\sum_{r\in\{\mathrm{AMPA,NMDA,GABA_A,GABA_B}\}}
g_{i,r}(E_r-D_i).
$$

Standardwerte des persistenten Kerns sind unter anderem
\(\tau_V=20\,\mathrm{ms}\), \(\tau_D=35\,\mathrm{ms}\),
\(\tau_\mathrm{AMPA}=5\,\mathrm{ms}\),
\(\tau_\mathrm{NMDA}=80\,\mathrm{ms}\),
\(\tau_{\mathrm{GABA_A}}=10\,\mathrm{ms}\) und
\(\tau_{\mathrm{GABA_B}}=120\,\mathrm{ms}\).

Ein Spike entsteht bei

$$
V_i\ge \theta_i+\theta_i^\mathrm{adapt}+\theta_i^\mathrm{homeo}.
$$

Danach folgen Reset, Refraktärzeit und adaptive Schwellenerhöhung. Energie
begrenzt Erregbarkeit und Übertragung zusätzlich. Das Modell soll keine
einzelne biologische Zellklasse imitieren; es stellt eine transparente
Trägerdynamik für lokale Mechanismen bereit.

<div align="right"><sub>Seite 10 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 11/30 -->

## Ereigniskausale synaptische Übertragung

Ein Spike ist ein Ereignis mit Quelle, Emissionszeit, Amplitude und
ereignisgebundenem Gate. Nach der individuellen Verzögerung \(d_{ij}\) wirkt
er auf genau die Zielkante:

$$
\Delta g_{ij,r}(t+d_{ij})
=
w_{ij}\,A_j\,
u_{ij}(t)\,R_{ij}(t)\,
g_K(\phi_j)\,
m_E(e_{ij}).
$$

Dabei sind \(uR\) kurzzeitige Freisetzung, \(g_K\) der generierte
Operatoranteil und \(m_E\) die Eligibility-Modulation. Alle Faktoren werden
begrenzt; nichtendliche Zustände führen zum Testfehler.

Der generierte Operator wird als

$$
g_K(\phi)=
\operatorname{clip}
\left(
\frac{1+\tanh(K(\phi))}{2},
0{,}05,0{,}95
\right)
$$

eingesetzt. Im historischen `RESET_LOCKED`-Wrapper war der wirksame Wert
konstant \(0{,}1283111213\). Erst die Emissionszustands- und
E/I-Projektionsvarianten erzeugten echte Ereignisvarianz.

Wissenschaftlich entscheidend: Eine dynamische Formel ist nicht automatisch
ein nützlicher dynamischer Mechanismus. Timing, Featureprojektion und
Einsetzposition bestimmen den tatsächlich wirksamen Phänotyp. Deshalb
vergleicht TATARUS Originalkernel, event-gematchte Konstante,
Vorzeichengate, Tanh, verteilungsgematchten Zufall, Zeitverschiebung und
Zustands-Shuffle.

<div align="right"><sub>Seite 11 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 12/30 -->

## Signierte lokale Eligibility-Transfer-Memory

Jede aktive Synapse \(j\rightarrow i\) besitzt eine eigene Spur:

$$
e_{ij}(t+\Delta t)=
\operatorname{clip}
\left[
e_{ij}(t)e^{-\Delta t/\tau_e}
\chi_t
\left(
s_i(t)\,\bar s_j(t)-s_j(t)\,\bar s_i(t)
\right),
-e_{\max},e_{\max}
\right].
$$

\(\bar s\) bezeichnet lokale Spike-Traces. \(\chi_t\) erlaubt das Schreiben
nur bei externem Reiz, Recall, Neuheit oder Reward; während echter Leerzeit
zerfällt die Spur. Die spätere Übertragung wird lokal moduliert:

$$
m_E(e_{ij})=
\operatorname{clip}
\left(1+\gamma_e\tanh(e_{ij}),m_{\min},m_{\max}\right).
$$

```mermaid
sequenceDiagram
    participant Pre as präsynaptisches Neuron
    participant Syn as Synapse j→i
    participant Post as postsynaptisches Neuron
    Pre->>Syn: früher Spike
    Post->>Syn: lokale Reihenfolge
    Syn->>Syn: signierte Spur eᵢⱼ
    Note over Syn: reizfreie Verzögerung: exponentieller Zerfall
    Pre->>Syn: späterer Recall-Spike
    Syn->>Post: durch eᵢⱼ veränderte Übertragung
```

Die Lokalität ist eine Invariante: Nicht vorhandene Verbindungen tragen
keine Spur; ein Synapsen-Shuffle zerstört die Zuordnungskontrolle. `Gain=0`
muss die historische Dynamik exakt wiederherstellen.

<div align="right"><sub>Seite 12 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 13/30 -->

## Kurzzeitplastizität, Konsolidierung und Vergessen

Die synaptische Ressource erholt sich kontinuierlich:

$$
R_{ij}\leftarrow
R_{ij}+(1-R_{ij})\frac{\Delta t}{\tau_\mathrm{rec}},
\qquad
\mathrm{release}_{ij}=u_{ij}R_{ij}.
$$

Bei Übertragung wird Ressource verbraucht und Facilitation verändert. Diese
schnelle Ebene wirkt über Millisekunden bis Sekunden. Die lokale
Eligibility-Spur bildet eine mittlere Zeitskala. Reward-gebundene
Konsolidierung überführt flüchtige Änderungen in stabilere Gewichte:

$$
\Delta w_{ij}
=
\eta\,M_t\,e_{ij},
\qquad
\Delta \bar w_{ij}
=
\eta_c |M_t e_{ij}|(w_{ij}-\bar w_{ij}),
$$

wobei \(M_t\) einen begrenzten neuromodulatorischen Zustand bezeichnet.
Dale-Vorzeichen und Gewichtsgrenzen bleiben erhalten.

```mermaid
flowchart LR
    A["Membran und Leitwert<br/>Millisekunden"] --> B["Ressource und Adaptation<br/>kurz"]
    B --> C["Eligibility<br/>mittel"]
    C --> D["konsolidiertes Gewicht<br/>lang"]
    D --> E["Topologie und Provenienz<br/>strukturell"]
```

Kontrolliertes Vergessen ist kein Löschen auf Befehl, sondern begrenzter
Zerfall ohne erneutes Schreiben. Interferenzschutz wird daran gemessen, ob ein
alter partieller Cue nach einer neuen Erfahrung weiterhin einen passenden
Zustand reaktiviert.

<div align="right"><sub>Seite 13 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 14/30 -->

## Homeostase, Energie und Stabilität

Ein fortlaufendes System darf weder dauerhaft verstummen noch unbeschränkt
eskalieren. TATARUS kombiniert drei Begrenzungsebenen.

Die gefilterte Rate \(r_i\) verschiebt die effektive Schwelle:

$$
\theta_i^\mathrm{homeo}(t+\Delta t)
=
\operatorname{clip}
\left[
\theta_i^\mathrm{homeo}(t)
+\eta_h(r_i-r_i^\star)\Delta t,
-12,12
\right].
$$

Der Energiezustand erholt sich und bezahlt Spike- sowie Übertragungskosten:

$$
q_i(t+\Delta t)=
\operatorname{clip}
\left[
q_i(t)+\rho_q\Delta t
-c_s s_i(t)
-c_\mathrm{tx}n_i^\mathrm{tx}(t),
0,1
\right].
$$

Gewichtsgrenzen, Dale-Konformität und endliche Werte bilden harte
Invarianten. Die Regulation ist nicht mit biologischem Stoffwechsel
gleichzusetzen; sie ist eine funktionale Ressourcenschranke.

Messgrößen sind mittlere Rate, Zielratenabweichung, Energie, Spikezahl,
Übertragungen, Strukturwachstum, Pruning und Endlichkeit. Im
Stufe-16-Endlauf lagen 7.500 fortgesetzte Schritte, 7.439 Spikes,
49.514 Übertragungen, 7,997799 Hz mittlere Rate bei 8,121509 Hz Zielrate und
0,991972 mittlere Energie vor.

<div align="right"><sub>Seite 14 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 15/30 -->

## Rohe Kanäle und kompetitive Assemblybildung

TATARUS enthält keinen zwingenden Tokenizer, kein Vokabular und keine
Embeddingtabelle. Bildereignisse, Audiosamples, Berührung, UTF-8-Bytes,
Temperatur und Interozeption werden topografisch auf überlappende
Mikroassemblies projiziert.

Ein Reizmuster wird als hervorgerufener Zustand relativ zur langsamen
Basislinie erfasst:

$$
\mathbf r^\mathrm{evoked}
=
\mathbf r^\mathrm{fast}
-\mathbf r^\mathrm{slow},
$$

ergänzt um signierte dendritische Abweichungen. Für Prototyp
\(\mathbf p_k\) gilt:

$$
k^\star=\arg\max_k
\frac{\mathbf p_k^\top\mathbf r}
{\|\mathbf p_k\|\,\|\mathbf r\|+\varepsilon}.
$$

Liegt die beste Ähnlichkeit unter der Schwelle, entsteht eine neue Assembly;
sonst wird nur der Gewinner inkrementell angepasst. So konkurrieren
Repräsentationen, anstatt in einem globalen Mittel zu kollabieren.

„Tokenizerfrei“ bedeutet hier ausschließlich: Die Eingabe benötigt keine
vorgegebenen Token-IDs. Es bedeutet nicht, dass TATARUS Sprache semantisch
versteht. Eine interne Einheit ist ein verteilter zeitlicher Nervenzustand,
dessen Bedeutung erst durch wiederholte Übergänge, Konsequenzen und
Auslesung entsteht.

<div align="right"><sub>Seite 15 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 16/30 -->

## Die beschränkte Cognitive Bridge

Der höhere Kern arbeitet nicht direkt auf dem vollständigen Nervenzustand.
Eine beschränkte Projektion erzeugt:

$$
\mathbf c_t=B(\mathcal S_t)
=
\left[
\text{Assemblies},
\text{Recall-Pools},
\text{Neuheit},
\text{Salienz},
\text{Bedarf},
\text{Fehler},
\text{Konfidenz}
\right].
$$

Die Bridge poolt 64 neuronale und 64 recall-gebundene Synapsengruppen.
Einzelne Membranen, Kanten, Gewichte und Eligibility-Werte werden nicht
offengelegt. Formal ist die Projektion verlustbehaftet:

$$
B^{-1}(\mathbf c_t)\neq \mathcal S_t.
$$

Der Planer darf nur einen begrenzten Befehl

$$
\mathbf u_t=
(\text{attention},\text{motor intent},\text{recall cue},\text{reward})
$$

zurückgeben. Dieser wird auf Kontext- und Regulationskanäle abgebildet und
adressiert keine einzelne Synapse.

Damit ist TATARUS weder ein unkontrolliertes Plugin im Planer noch ein
passiver Datenspeicher. Die Bridge schafft eine überprüfbare
Verantwortungsgrenze: Das Nervensystem trägt die lokale Dynamik; der höhere
Kern verarbeitet gepoolte Funktionen. In Ablationen kann jede Seite
unabhängig ersetzt oder entfernt werden.

<div align="right"><sub>Seite 16 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 17/30 -->

## Kontinuierlicher Lebenslauf und kausale Snapshots

Alle Erfahrungen eines Lebenslauf-Seeds werden in demselben Systemzustand
ausgeführt. Das verhindert, dass eine episodische Resetlogik unbeabsichtigt
die eigentliche Gedächtnisleistung übernimmt.

```mermaid
stateDiagram-v2
    [*] --> Erfahrung_A
    Erfahrung_A --> Leerphase
    Leerphase --> Erfahrung_B
    Erfahrung_B --> Regelwechsel
    Regelwechsel --> Recall
    Recall --> Handlung
    Handlung --> Konsequenz
    Konsequenz --> Erfahrung_A: fortlaufender Zustand
```

Ein kompositer Snapshot umfasst:

- den vollständigen V9-Nervensystemzustand,
- RNG und Axonereignisqueues,
- Assemblies und Reizphasenakkumulatoren,
- Bridge-Zustand und Reward-Prädiktion,
- Parameter des höheren Planungskerns.

Exakte Fortsetzung wird stärker geprüft als durch ähnliche Mittelwerte:

$$
\operatorname{Hash}(\mathcal S_{t+n}^{\mathrm{direkt}})
=
\operatorname{Hash}(\mathcal S_{t+n}^{\mathrm{geladen}})
$$

und Aktions-, Recall- sowie Zustandsfolgen müssen übereinstimmen. Dadurch
wird ein Snapshot zum kausalen Fortsetzungspunkt und nicht nur zum Export
einer ungefähren Modellkonfiguration.

<div align="right"><sub>Seite 17 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 18/30 -->

## Experimentelles Design und Pflichtkontrollen

TATARUS trennt Entwicklung, Einfrieren und Bestätigung. Mechanismen und
Parameter werden auf Entwicklungsseeds gewählt; Entscheidungskriterien
werden vor dem Holdout-Lauf festgelegt. Neue Seeds dürfen anschließend nicht
zur Mechanismuswahl zurückfließen.

Für Übertragungsoperatoren werden mindestens folgende Kontrollen verwendet:

| Kontrolle | isolierte Erklärung |
|---|---|
| deaktiviert | Gate überhaupt erforderlich? |
| event-gematchte Konstante | genügt der wirksame Mittelwert? |
| Vorzeichen | genügt Polarität? |
| Tanh | genügt eine Standardnichtlinearität? |
| verteilungsgematchter Zufall | genügt die Verteilung? |
| zeitverschoben | ist das Timing kausal relevant? |
| state-/synapse-shuffled | ist die korrekte Zuordnung relevant? |
| Gain \(=0\) | neutraler Rückfall exakt? |

Für Gedächtnis und Reparatur kommen Ohne-Trace-, Ohne-Nervensystem-,
statischer-Reflex-, Schadens- und Provenienzkontrollen hinzu. Ergebnisse
werden seedweise berichtet; positive Mittelwerte allein ersetzen keine
Einzelkriterien.

Negative Befunde bleiben erhalten. Besonders wichtig sind die bitgenaue
Äquivalenz des historischen Reset-Kernels zur richtigen Konstantkontrolle,
die negative Delayed-XOR-Effizienzreplikation und das Ausstehen externer
Hardware-Replikation.

### Mechanismen- und Quellcodematrix

| Mechanismus | Zweck, Zustand und Ausführungsposition | Invariante/Kontrolle | Evidenz und Grenze | Quelle |
|---|---|---|---|---|
| Generated Polarity and Release Operator | moduliert die präsynaptische Freisetzung aus Eventfeature \(\phi\) | Clip; Konstante, Sign, Tanh, Zufall | dynamisch implementiert; nicht universell überlegen | `bio_core.cpp`, `nervous_system.cpp` |
| Signed Local Eligibility-Transfer Memory | speichert Prä-/Post-Reihenfolge pro aktiver Kante; wirkt bei späterer Übertragung | Lokalität; Gain 0, Shuffle, Shift | trace-essential Aufgabenvorteil; domänenspezifisch | `nervous_system.cpp` |
| Evoked-State Baseline Separation | trennt Reizantwort von langsamer Basislinie | signed state; Ohne-Baseline-Vergleich | stabile synthetische Repräsentationen | `nervous_system.cpp` |
| Competitive Stimulus-Phase Assembly Formation | gewinnt oder erzeugt Prototyp nach Reizphase | Maximalzahl, Ähnlichkeitsschwelle | 8/8 Holdout-Seeds | `nervous_system.cpp` |
| Tokenizer-Free Topographic Raw Projection | projiziert Rohkanal auf überlappende Mikroassembly | feste geseedete Topografie | rohe Übergänge, kein Sprachverständnis | `nervous_system.cpp` |
| Reward-Bound Local Consolidation | überführt modulierte Eligibility in Gewicht und Konsolidierungszustand | Grenzen und Dale-Vorzeichen | Stufe-21-Gedächtnis | `nervous_system.cpp` |
| Controlled Trace Decay and Interference Protection | zerfällt in echter Leerzeit, schützt alten Cue | Schreiben nur bei Salienzereignis | 8/8 Gedächtnis-Seeds | `nervous_system.cpp` |
| Provenance-Guided Axonal Path Reconstruction | ersetzt benutzten zerstörten Pfad mit Eltern-ID | Endpunkte aktiv; eingefrorene Nachprüfung | 8/8 definierte Reparaturen | `nervous_system.cpp`, `representation_research.cpp` |
| Bounded Cognitive-State Bridge | poolt Funktion und beschränkt Top-down-Zugriff | kein Einzelzell-/Gewichtszugriff | 8/8 Lebenslauf-Seeds | `cognitive_bridge.cpp`, `persistent_ai_trial.cpp` |
| Composite Causal Snapshot Continuation | serialisiert Nervensystem, Bridge, RNG und Planer | Hash- und Replay-Identität | lokal exakt; Plattformreplikation offen | `nervous_system.cpp`, `cognitive_bridge.cpp` |

<div align="right"><sub>Seite 18 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 19/30 -->

## Evidenz I – Repräsentationen und rohe Sequenzstruktur

Die Stufe-18-Bestätigung verwendete eingefrorene Kriterien und neue Seeds.
Stabile konkurrierende Repräsentationen bestanden 8/8 Netze. Im Mittel
entstanden 6,125 Assemblies. Ähnliche Reize reaktivierten ihren Zustand mit
einer Kosinusähnlichkeit von 0,907323; nach 10 % Neuronen- und 15 %
Synapsenschaden blieb 0,852185 erhalten.

Für rohe Sequenzen wurden UTF-8-Bytes als Bitereignisse ohne Token-,
Wort- oder Embeddingtabelle eingespeist. Ein nur auf Trainingsepochen
angepasster linearer Readout unterschied sechs Übergangsklassen in
unberührten Epochen mit 77,3438 % mittlerer Accuracy. Die Aktivitätsantwort
an Grenzen war um Faktor 2,884757 stärker als an normalen Übergängen.

Diese Befunde tragen zwei begrenzte Aussagen:

1. Die Assemblydynamik bildet innerhalb der synthetischen Reizfamilie
   unterscheidbare, wiederaktivierbare Zustände.
2. Rohbytes können ohne vorgegebene Token-IDs in eine informative zeitliche
   Übergangsstruktur überführt werden.

Sie tragen nicht die Aussage, dass das System natürliche Sprache versteht
oder selbstständig universelle Symbole entdeckt. Semantische Bindung,
Modalitätstransfer und Langzeitstabilität in offenen Datenströmen bleiben
Forschungsfragen.

<div align="right"><sub>Seite 19 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 20/30 -->

## Evidenz II – Trace-essential internes Gedächtnis

Zwei frühe, energiegleiche Cues tragen XOR-Information. Danach folgen
400 vollständig reizfreie Schritte und für beide Klassen derselbe neutrale
Recall-Cue. Das lineare Readout sieht ausschließlich Spikeänderungen und
Endmembranzustände aus dem letzten Recallfenster; Cue-Features,
Eligibility-Werte und Produktmerkmale sind verborgen.

```mermaid
sequenceDiagram
    participant X as Eingabe
    participant N as TATARUS
    participant R as Readout
    X->>N: Cue A→B oder B→A
    Note over N: 400 ms reizfreie Verzögerung
    X->>N: identischer Recall-Cue
    N->>R: nur spätes Reaktionsfenster
    R->>R: XOR-Entscheidung
```

Mit eingefrorenen Parametern \(\tau_e=800\,\mathrm{ms}\),
Gain \(=10\), Inkrement \(=20\) erreichten zwölf Holdout-Netze 1,000000
Accuracy. Ohne Eligibility waren es 0,486111.

Der Nachweis ist kausal enger als ein Readout mit explizitem Cue-Puffer:
Während der Leerphase muss relevante Information im Nervensystemzustand
überleben und die spätere Recall-Reaktion verändern. Bestätigt ist dieser
Mechanismus in der definierten XOR-Aufgabe, nicht beliebig langes
episodisches Gedächtnis.

<div align="right"><sub>Seite 20 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 21/30 -->

## Evidenz III – Gekoppelte KI bei identischer Gegenwart

Stufe 19 prüft die zentrale Designhypothese in einem durchgängigen
64-Erfahrungen-Lebenslauf. Die Vorgeschichte enthält \(A\rightarrow B\) oder
\(B\rightarrow A\); nach einer reizfreien Phase ist der aktuelle Recall-Reiz
für beide Klassen gleich. Zwischen Lernen und Test wird eine unbekannte rohe
Bytegrammatik eingespielt.

```mermaid
flowchart TB
    H1["Vergangenheit A→B"] --> S1["persistenter Zustand S¹"]
    H2["Vergangenheit B→A"] --> S2["persistenter Zustand S²"]
    X["identischer aktueller Recall"] --> S1
    X --> S2
    S1 --> A1["Handlung links"]
    S2 --> A2["Handlung rechts"]
```

| Variante | mittlere Accuracy |
|---|---:|
| Planer + TATARUS + lokale Eligibility | 1,000000 |
| identische Kopplung ohne Eligibility | 0,515625 |
| höherer Kern ohne Nervensystem | 0,500000 |

Alle 8/8 neuen Seeds bestanden die vorab definierten Kriterien. Die
Aktionsdiversität betrug 1,0 und komposite Snapshot-Replays waren exakt.

Das Experiment bestätigt eine handlungswirksame Kopplung in dieser teilweise
beobachtbaren Aufgabe. Es bestätigt keinen universellen Vorteil gegenüber
allen Gedächtnisarchitekturen und keine allgemeine Grammatikkompetenz.

<div align="right"><sub>Seite 21 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 22/30 -->

## Evidenz IV – Prozedurale Lebenswelt und G5

Die Stufe-20-Welt enthält frei gezogene Objektlagen, Energiebedarf, Gefahr,
mehrere konkurrierende Ziele, verzögerte Konsequenzen und unangekündigte
Regelwechsel. Entscheidungen fallen vor dem Reward; der höhere Kern sieht
nur die Cognitive Bridge.

Über acht neue Seeds wurden gemessen:

| Variante | mittlerer Reward |
|---|---:|
| gekoppeltes TATARUS-System | 310,157089 |
| gleiche Architektur ohne Eligibility | 294,101531 |
| statischer Reflex | 119,488903 |
| eingefrorene G5-Struktur | 363,183060 |

Sechs von acht Seeds erfüllten sämtliche Stufe-20-Einzelkriterien. Der
aggregierte Status lautet `confirmed_on_procedural_holdouts`.

Der Begriff „offene Lebenswelt“ bezeichnet hier eine prozedural generierte,
teilweise beobachtbare synthetische Weltfamilie. Situationen entstehen frei
innerhalb definierter Generatorregeln; die Welt ist nicht physisch real und
nicht unbegrenzt offen. G5 prüft Transfer auf eine eingefrorene neue
Ereignisstruktur derselben Weltfamilie. Daraus folgt weder universelle
Grammatikgeneralisation noch robuste reale Robotik.

<div align="right"><sub>Seite 22 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 23/30 -->

## Evidenz V – Mehrskaliges Gedächtnis

Stufe 21 trennt vier Gedächtnisfunktionen, anstatt sie in einer einzigen
Accuracy zusammenzufassen:

| Funktion | aggregierter Wert | Kontrolle/Kriterium |
|---|---:|---|
| episodische Einmalspur | 0,281266 | ohne Trace: 0 |
| Konsolidierungsänderung | 87,922375 | reward-gebundene Gewichtsänderung |
| kontrolliertes Vergessen | 99,9955 % | reizfreie Zerfallsphase |
| Retention nach Interferenz | 0,999981 | partieller alter Cue |

Alle 8/8 neuen Seeds bestanden die eingefrorenen Kriterien. Eligibility wird
nur bei externem Reiz, Recall, Neuheit oder Reward geschrieben. Spontane
rekurrente Aktivität darf eine echte Leerphase nicht als neue Erfahrung
maskieren.

```mermaid
flowchart LR
    E["einmaliges Ereignis"] --> T["lokale Spur"]
    T -->|Reward| C["Konsolidierung"]
    T -->|kein Reiz| F["kontrollierter Zerfall"]
    C --> I["Interferenz"]
    I --> R["partieller Cue und Retention"]
```

Die Resultate belegen getrennte Zeit- und Zustandsmechanismen in den
synthetischen Tests. Sie sind noch kein Nachweis autobiografischen
Gedächtnisses, unbegrenzter Konsolidierung oder lebenslangen Lernens.

<div align="right"><sub>Seite 23 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 24/30 -->

## Schaden und provenienzgestützte Reparatur

Die Reparaturprüfung beginnt mit einer messbaren Sensor-Motor-Funktion.
Danach werden genau sechs tatsächlich benutzte Direktleitungen sowie 10 %
interne Neuronen deaktiviert. Die Funktion fällt in allen acht
Holdout-Netzen auf null.

```mermaid
flowchart LR
    B["etablierte Funktion"] --> D["gezielter Pfadschaden"]
    D --> L["gemessener Funktionsverlust"]
    L --> G["begrenztes strukturelles Wachstum"]
    G --> P["Ersatzkante mit Eltern-ID"]
    P --> V["eingefrorene Funktionsprüfung"]
```

Neue Kanten dürfen nur aus zuvor benutzten, konsolidierten und nun inaktiven
Pfaden hervorgehen:

$$
w_{\mathrm{neu}}=\bar w_{\mathrm{parent}},
\qquad
d_{\mathrm{neu}}=\max(1,d_{\mathrm{parent}}-1).
$$

In 8/8 Netzen entstanden sechs Ersatzsynapsen mit Eltern-Provenienz. Nach
Einfrieren von Wachstum und Homeostasedrift wurden im Mittel 111,3726 % des
ursprünglichen Funktionsbetrags mit gleichem Vorzeichen wiedergewonnen.

Bestätigt ist damit eine definierte Form kausaler Pfadrekonstruktion. Nicht
bestätigt sind allgemeine Selbstheilung, Reparatur beliebiger Funktionen
oder biologische Regeneration.

<div align="right"><sub>Seite 24 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 25/30 -->

## Skalierung und technische Tragfähigkeit

Für \(N>2048\) ersetzt ein direkter Sparse-Sampler die quadratische Prüfung
aller Zellpaare. Der erwartete Ausgangsgrad wird geseedet mit eindeutigen
Zielen erzeugt. So bleibt die Initialisierung näher an \(O(Nk)\) als an
\(O(N^2)\).

```mermaid
xychart-beta
    title "Ausgeführte Netzgrößen"
    x-axis [256, 1024, 4096, 16384, 65536]
    y-axis "aktive Synapsen" 0 --> 2100000
    line [4671, 33054, 131248, 524464, 2097328]
```

Der vollständige Release-Lauf umfasste 65.536 Neuronen, 2.097.328 aktive
Synapsen und 40 Schritte. Auf der lokalen 12-Thread-CPU benötigte die
Simulation 2.329,1649 ms. Der Snapshot war 212.426.348 Byte groß und wurde
hashidentisch restauriert. Endlichkeit, Energiegrenzen und Dale-Prinzip
blieben erhalten.

Diese Messung bestätigt Ausführbarkeit und strukturelle Integrität. Der
Echtzeitfaktor betrug bei dieser Größe 0,017174; Echtzeitfähigkeit ist daher
ausdrücklich nicht bestätigt. Nur ein geprüfter Integrationsschritt ist
CPU/OpenCL-differenziell freigegeben; komplexe Plastizitäts- und
Reparaturpfade bleiben CPU-autoritativ.

<div align="right"><sub>Seite 25 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 26/30 -->

## Abgrenzung zu anderen KI-Komponenten

| Kategorie | primäre Rolle | Verhältnis zu TATARUS |
|---|---|---|
| LLM/Reasoningmodell | Sprache, Wissen, abstrakte Planung | möglicher höherer Kern |
| klassisches SNN | spikende Dynamik oder Klassifikation | TATARUS ergänzt Lebenslauf, Gedächtnisebenen, Bridge und Reparatur |
| externer Speicher | persistente Datenablage | kann TATARUS ergänzen, verändert aber nicht automatisch lokale Dynamik |
| Reinforcement-Learning-Agent | Policyoptimierung über Reward | kann über die Bridge handeln |
| Weltmodell | Vorhersage externer Zustände | TATARUS modelliert primär den internen Zustandsapparat |

TATARUS ist kein Ersatz für alle genannten Komponenten. Die Architektur
positioniert sich als **Substrat zwischen kontinuierlicher Umweltkopplung und
higher-level cognition**.

Die Entwicklungsleistung liegt nicht in der Behauptung, dass einzelne
elementare Funktionen neu seien. LIF-Dynamik, STDP, Homeostase,
Kurzzeitplastizität und Assemblies besitzen etablierte Vorläufer. Der
Projektbeitrag ist ihre konkrete, persistente Kopplung mit generiertem
Operator, lokaler Transfer-Memory, beschränkter Bridge, kausalem Snapshot und
provenienzgestützter Reparatur in einer einheitlichen testbaren
Implementierung.

Eine belastbare Prioritäts- oder „erstmals“-Behauptung erfordert eine
gesonderte systematische Literatur- und Patentprüfung.

<div align="right"><sub>Seite 26 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 27/30 -->

## Grenzen, Risiken und verantwortliche Interpretation

Nicht bestätigt sind:

- menschliches oder tierisches Bewusstsein und Emotionen,
- biologische, anatomische oder molekulare Gleichwertigkeit,
- vollständige Gehirnsimulation,
- universelle Welt- oder Grammatikgeneralisation,
- allgemeine Intelligenz,
- universelle Überlegenheit des generierten Kernels,
- produktiver Echtzeitbetrieb der größten Konfiguration,
- unabhängige Replikation auf zweiter Hardware.

Weitere technische Risiken sind Parameterempfindlichkeit, versteckte
Korrelationen synthetischer Aufgaben, Skalierungskosten, mögliche
Interferenz langer Lebensläufe und schwer interpretierbare Assemblydrift.
Ein persistenter Zustand kann außerdem unerwünschte Erfahrungen bewahren.
Daher sind Reset, Snapshotversionierung, Zustandsaudit, Ressourcenlimits und
Mechanismusablationen sicherheitsrelevant.

TATARUS darf nicht als empfindungsfähiges Wesen dargestellt werden. Begriffe
wie Reward, Neuheit, Energie oder Dopamin bezeichnen funktionale numerische
Variablen, keine bewiesenen subjektiven Zustände.

Die korrekte öffentliche Formulierung ist:

> TATARUS ist ein experimentelles, persistentes synthetisches Nervensystem
> für KI, dessen dokumentierte Funktionen in begrenzten synthetischen
> Aufgaben kausal geprüft wurden.

<div align="right"><sub>Seite 27 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 28/30 -->

## Open Science und unabhängige Replikation

Version 1.0.0 ist unter Apache 2.0 veröffentlicht. Quellcode, UI-Dokumentation,
Versuchspläne, Rohmetriken, negative Resultate und eingefrorene Berichte sind
im Repository enthalten. Große binäre Snapshots werden wegen ihrer Größe
nicht in Git versioniert, können aber durch die Läufe neu erzeugt werden.

Das Stufe-23-Paket enthält:

- 24 extern verwendbare Seeds,
- Clean-Build- und Gesamtlaufskript,
- erwartete Referenzwerte,
- Hardware- und Compilerprotokoll,
- Hashmanifest.

```mermaid
flowchart LR
    C["Clean Checkout"] --> B["Build auf Rechner 2"]
    B --> S["unabhängige Seeds"]
    S --> R["Stufen 18–23 ausführen"]
    R --> H["Hashes, Hardware, Rohwerte"]
    H --> V["Vergleich ohne Parameteranpassung"]
```

Die lokale Paketwiederholung bestand, ist aber keine unabhängige Replikation.
Dieser Status bleibt `package_ready_external_run_pending`, bis ein zweiter
Rechner mit dokumentierter Hardware und frischem Build die Kriterien
reproduziert.

Empfohlene Zitierung steht in `CITATION.cff`. Reproduktionsabweichungen sollen
als Ergebnisse veröffentlicht und nicht durch nachträgliche Parameterwahl
verdeckt werden.

<div align="right"><sub>Seite 28 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 29/30 -->

## Forschungsprogramm nach Version 1.0

Die nächste Evidenzstufe sollte Reichweite statt weiterer
Mechanismenakkumulation priorisieren:

1. **Externe Replikation:** Clean-Build auf anderer CPU/GPU und
   Betriebssystemkonfiguration.
2. **G6-Transfer:** strukturell neue Weltfamilie ohne nachträgliche
   Mechanismus- oder Parameterwahl.
3. **Langzeitversuche:** deutlich längere Lebensläufe mit Messung von
   Assemblydrift, Vergessen und Interferenz.
4. **Offene Rohströme:** kontinuierliche Audio-, Bild- und Byteereignisse mit
   vorab definierten Grenz- und Vorhersagekriterien.
5. **Reparaturbreite:** mehrere verlorene Funktionen, konkurrierende
   Ersatzpfade und Kosten der Wiederherstellung.
6. **Beschleunigung:** differenziell getestete OpenCL-Pfade für Plastizität,
   Assembly und Struktur.
7. **Vergleichssysteme:** externe SNN-, Reservoir-, RL- und Memory-Baselines
   bei gleichem Informationszugang und Rechenbudget.

Eine geeignete Mehrzielgröße ist:

$$
J=
\mathrm{TaskScore}
-\lambda_s\,\mathrm{Spikekosten}
-\lambda_e\,\mathrm{Interferenz}
+\lambda_r\,\mathrm{Robustheit}
+\lambda_x\,\mathrm{Reproduzierbarkeit}.
$$

Der nächste Fortschritt ist nicht die größte positive Zahl, sondern eine
engere kausale Behauptung, die auf unberührten Daten und externer Hardware
besteht.

<div align="right"><sub>Seite 29 von 30</sub></div>
<div style="page-break-after: always;"></div>

<!-- PAGE 30/30 -->

## Schlussfolgerung und Referenzen

TATARUS zeigt, dass ein künstlicher Agent einen persistierenden,
mehrskaligen und handlungswirksamen Nervenzustand als eigenes
Rechensubstrat besitzen kann. Der veröffentlichte Stand verbindet
neurodynamische Zustände, lokale synaptische Erinnerung, Regulation,
Assemblybildung, beschränkte Planungskopplung, Closed Loop und strukturelle
Reparatur in einem deterministisch prüfbaren C++-System.

Der wissenschaftliche Wert liegt ebenso in den Grenzen: Die spezielle
Kernelgeometrie ist nicht universell überlegen, die größte Konfiguration ist
nicht echtzeitfähig, die Welten sind synthetisch und die externe Replikation
steht aus. Genau diese Trennung macht die Architektur überprüfbar.

### Primäre TATARUS-Artefakte

- [Projektübersicht](README.md)
- [UI-Dokumentation](UI_DOKUMENTATION.md)
- [Persistenter Kern](research/ag_signal_morpher_1ee27305a6aa/16_persistent_nervous_system/README.md)
- [Stufen 17/18](research/ag_signal_morpher_1ee27305a6aa/17_autonomous_representation/README.md)
- [Cognitive Bridge](research/ag_signal_morpher_1ee27305a6aa/19_persistent_ai_bridge/README.md)
- [Stufen 20–23](research/ag_signal_morpher_1ee27305a6aa/20_23_validation/README.md)

### Fachlicher Kontext

1. Gerstner, Kistler, Naud, Paninski:
   [*Neuronal Dynamics*](https://neuronaldynamics.epfl.ch/online/).
   Cambridge University Press, 2014.
2. Dayan, Abbott:
   [*Theoretical Neuroscience*](https://mitpress.mit.edu/9780262041997/theoretical-neuroscience/).
   MIT Press, 2001.
3. Maass, Natschläger, Markram:
   [“Real-Time Computing Without Stable States: A New Framework for Neural
   Computation Based on Perturbations”](https://doi.org/10.1162/089976602760407955).
   *Neural Computation* 14(11), 2002.
4. Bi, Poo:
   [“Synaptic Modifications in Cultured Hippocampal Neurons: Dependence on
   Spike Timing, Synaptic Strength, and Postsynaptic Cell
   Type”](https://pubmed.ncbi.nlm.nih.gov/9852584/).
   *J. Neurosci.* 18(24), 1998.
5. Frémaux, Gerstner:
   [“Neuromodulated Spike-Timing-Dependent Plasticity, and Theory of
   Three-Factor Learning Rules”](https://doi.org/10.3389/fncir.2015.00085).
   *Front. Neural Circuits* 9, 2016.

<div align="right"><sub>Seite 30 von 30</sub></div>
