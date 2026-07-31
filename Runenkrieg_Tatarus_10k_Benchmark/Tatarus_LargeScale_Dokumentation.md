# TATARUS LargeScale in Runenkrieg

## Technische und wissenschaftliche Dokumentation

**System:** TATARUS – A Persistent Synthetic Nervous System  
**Android-Zweig:** Runenkrieg: TATARUS LargeScale  
**Version:** 2.0.0-largescale  
**Stand:** 30. Juli 2026  
**Entwickler:** Ralf Krümmel

## 1. Forschungszweck

Die Referenz-App mit 72 Neuronen bleibt als eingefrorene Kontrollfassung
erhalten. LargeScale ist ein getrennt installierbarer Versuch: Er erhöht
Kapazität und sensorische Bandbreite, beseitigt technische
Skalierungsengpässe und trainiert auf zusammenhängenden Partien.

Die zentrale Hypothese lautet:

\[
H_1:\quad Q_{\text{LargeScale}}>Q_{\text{Regelbaseline}}
\]

mit einer vorab definierten Qualitätsfunktion \(Q\), beispielsweise
Holdout-Siegrate und Token-Differenz. Die Implementierung allein bestätigt
diese Hypothese nicht.

## 2. Trennung von Referenz und Experiment

| Eigenschaft | Referenz | LargeScale |
|---|---|---|
| Ordner | `Runenkrieg_Tatarus` | `Runenkrieg_Tatarus_LargeScale` |
| Application-ID | `de.runenkrieg.game` | `de.runenkrieg.game.large` |
| Modellpersistenz | eigener App-Speicher | eigener App-Speicher |
| Installation | Referenz-App | parallel installierbar |

Es findet keine automatische Migration des kleinen Zustands statt. Das
verhindert, dass alte Erfahrungen den LargeScale-Versuch unkontrolliert
beeinflussen.

## 3. Netzwerktopologie

Der Kern besitzt \(N=1024\) Neuronen. Jedes Neuron erzeugt genau 32
gerichtete rekurrente Ausgänge:

\[
S=N\cdot k=1024\cdot32=32768.
\]

Die Population ist funktional gegliedert:

| Indexbereich | Anzahl | Funktion |
|---|---:|---|
| 0–127 | 128 | sensorische Eingangspopulation |
| 128–767 | 640 | exzitatorische Rekurrenz |
| 768–959 | 192 | inhibitorische Rekurrenz |
| 960–991 | 32 | Kontextpopulation |
| 992–1023 | 32 | Motor-/Readoutpopulation |

Präsynaptische Neuronen im inhibitorischen Bereich besitzen ausschließlich
negative Gewichte. Alle übrigen rekurrenten Quellen sind exzitatorisch.
Damit bleibt das Vorzeichen jeder Quelle Dale-konform.

Die initialen Beträge liegen bei:

\[
w_E\in[0{,}9;2{,}5],\qquad
w_I\in[-3{,}6;-1{,}8].
\]

Individuelle Übertragungsverzögerungen werden deterministisch aus dem Seed
im Bereich 1–8 Simulationsschritte gezogen.

## 4. Afferente Verdrahtung

Für jeden Eingangskanal \(c\) existieren acht Projektionen:

\[
\mathcal P(c)=\{n_{c,0},\ldots,n_{c,7}\}.
\]

Die erste Projektion endet am zugehörigen Eingangsneuron. Fünf weitere
enden in der exzitatorischen Population, zwei in der Kontextpopulation.
Damit ergeben sich:

\[
128\cdot8=1024
\]

neuronale Eingangsprojektionen. Die Zielwahl ist seedunabhängig
deterministisch; alle 128 Kanäle verändern nachweislich den Kernzustand.

## 5. Bedeutung der 128 Eingaben

Kontext- und Kandidatenphase nutzen denselben afferenten Raum, aber
unterschiedliche Kodierungen.

### 5.1 Kontextphase

| Kanäle | Inhalt |
|---|---|
| 0–23 | Grundzustand: Belohnung, Karten-, Wetter-, Token-, Helden- und Rundenmerkmale |
| 24–33 | gesamte Häufigkeit der zehn Spielerelemente |
| 34–43 | jüngste Spielerelemente |
| 44–53 | jüngste TATARUS-Elemente |
| 54–63 | Elemente der aktuellen TATARUS-Hand |
| 64–77 | Spielerfähigkeit, one-hot |
| 78–82 | Spielerkartentyp, one-hot |
| 83–89 | sieben Mechaniken der Spielerkarte |
| 90–99 | rohe Element–Wetter-Interaktion |
| 100–103 | vier letzte Rundenausgänge |
| 104–107 | vier letzte Spieler-Stärkewerte |
| 108–111 | vier letzte TATARUS-Stärkewerte |
| 112–126 | Token-, Runden-, Streak-, Neuheits- und Verlaufsmerkmale |
| 127 | Biaskanal |

### 5.2 Kandidatenphase

| Kanäle | Inhalt |
|---|---|
| 0–31 | Basiskodierung der legalen Einzelkarte oder Fusion |
| 32–45 | Kandidatenfähigkeit, one-hot |
| 46–50 | Kandidatentyp, one-hot |
| 51–57 | Mechaniken |
| 58–67 | Spielerelement, one-hot |
| 68–77 | Spielerelement × Kandidatenstärke |
| 78–87 | Kandidatenelement × Wetter |
| 88–97 | Elementverteilung der aktuellen Hand |
| 98–107 | jüngste TATARUS-Elemente |
| 108–117 | jüngste Spielerelemente |
| 118–126 | nichtlineare Stärke, Tokenlage, Runde, Wiederholung und Handrest |
| 127 | Biaskanal |

Der Standardmodus entfernt fertig berechnete Regelvorteile aus dem
neuronalen Entscheidungsscore. Der Hybridmodus darf diese als explizite
Prioren verwenden.

## 6. Neuronendynamik

Soma und passiver Dendrit werden diskret fortgeschrieben:

\[
D_i(t+1)=D_i(t)+
\frac{V_\mathrm{rest}-D_i(t)+I^\mathrm{rec}_i(t)+I^\mathrm{ext}_i(t)}
{\tau_D},
\]

\[
V_i(t+1)=V_i(t)+
\frac{V_\mathrm{rest}-V_i(t)+
\kappa(D_i(t)-V_i(t))+I_0}{\tau_V}.
\]

Ein Spike entsteht bei Überschreiten der adaptiven und homeostatischen
Schwelle, sofern genügend Energie vorhanden ist. Danach werden Spannung,
Adaptation, Ratenzustände und Energie aktualisiert.

## 7. Ereigniskausale Übertragung und lokales Gedächtnis

Für einen Spike der Quelle \(j\) zur Senke \(i\) gilt:

\[
A_{ij}=w_{ij}\,p_{ij}\,R_{ij}\,
g_K(\phi_{ij})\,
\operatorname{clip}(1+\gamma\tanh(e_{ij}),0{,}25,2).
\]

Dabei sind \(p\) Freisetzungswahrscheinlichkeit, \(R\) synaptische
Ressource, \(g_K\) der exportierte Generated Operator und \(e\) die lokale
signierte Eligibility-Spur. Die Spur wird aus Prä-/Post-Reihenfolge
gebildet, exponentiell abgebaut und durch spätere Belohnung in eine
Gewichtsänderung überführt:

\[
e_{ij}(t+1)=e_{ij}(t)e^{-1/\tau_e}+\Delta e_{ij},
\qquad
\Delta w_{ij}=\eta\,d(t)\,e_{ij}(t).
\]

Die Ablationen `Ohne Eligibility`, `Ohne Generated Operator` und `Ohne
Assemblies` schalten jeweils den bezeichneten Mechanismus ab.

## 8. Entscheidungsweg

1. Der aktuelle Spielkontext läuft zehn Schritte durch das Nervensystem.
2. Der resultierende Zustand wird einmal als flacher Snapshot gesichert.
3. Jede legale Einzelkarte und Fusion erhält denselben Ausgangszustand.
4. Der Kandidat läuft sechs Schritte als gegenfaktischer Rollout.
5. 48 neuronale Bridge-Merkmale und 32 Kandidatenmerkmale bilden einen
   80-dimensionalen Aktionsvektor.
6. Ein begrenzter linearer Readout erzeugt den neuronalen Score.
7. Nur der gewählte Kandidatenrollout wird in den persistenten Zustand
   übernommen.
8. Das spätere Rundenergebnis aktualisiert Readout, Eligibility und
   Synapsengewichte.

Damit sieht kein Kandidat den zuvor getesteten Kandidatenzustand.

## 9. Vollständiges Selbsttraining

Das LargeScale-Training erzeugt keine unabhängigen Einzelrunden mehr. Es
spielt vollständige Partien mit:

- gleichbleibender Historie innerhalb einer Partie,
- Helden und Wetter,
- Tokengewinnen und -verlusten,
- Nachziehen,
- Fusionen,
- mehreren Spielerstrategien (zufällig, stärkste, schwächste und Fusion).

Ein Trainingsauftrag zählt gelöste Runden. Der Modellzustand wird während
des Batches im Speicher fortgeschrieben und am Batchende einmal atomar
persistiert. Dadurch werden unnötige Flash-Schreibvorgänge vermieden.

## 10. Persistenz und Skalierung

Ein Snapshot hält Neuronen- und Synapsenzustände in flachen primitiven
Arrays. Auch das persistierte JSON nutzt sechs flache Synapsenfeld-Arrays.
Das vermeidet 32.768 kleine Snapshot- oder JSON-Unterobjekte pro Kandidat
beziehungsweise Speichervorgang.

Das Gesamtmodell wird als UTF-8-JSON in
`tatarus_large_v1.json.gz` im privaten App-Verzeichnis gespeichert:

1. Schreiben in eine temporäre gzip-Datei,
2. Schließen und vollständiges Flush,
3. atomare Ersetzung der Modelldatei, sofern das Dateisystem dies erlaubt,
4. sichere Ersetzung als Fallback.

Die App fordert keine Internetberechtigung an. Zustand und Messwerte
verlassen das Gerät nicht.

## 11. TATARUS-Labor

Die Oberfläche zeigt:

- reale und selbsttrainierte Beobachtungen getrennt,
- reale Siege, Unentschieden, Niederlagen und Belohnung,
- Neuronen, Synapsen, Kanäle und Eingangsprojektionen,
- geschätzte Größe des dynamischen Zustands,
- neuronale Schritte, Spikes, Feuerrate und Übertragungen,
- mittlere Entscheidungszeit auf dem aktuellen Gerät,
- Gewichts- und Eligibility-Sättigung,
- Assemblyanzahl, Entropie, Separation und Reaktivierungen,
- mittlere, minimale und untere Energieverteilung,
- Ergebnisse der identisch geseedeten Baseline-Evaluation.

Der interaktive Schnelllauf verwendet fünf Partien je Modus. Er dient als
Funktions- und Tendenztest, nicht als Publikationsnachweis.

## 12. Forschungsmodi

| Modus | Zweck |
|---|---|
| Reines TATARUS | neuronaler Score ohne Regelprior |
| Hybrid | neuronaler Score plus Regel- und Erfahrungsprior |
| Nur Regeln | deterministische nichtneuronale Spielbaseline |
| Zufall | untere Zufallsbaseline |
| TATARUS eingefroren | Inferenz ohne Lernen |
| Ohne Eligibility | Beitrag lokaler synaptischer Spuren |
| Ohne Generated Operator | konstantes Gate 0,5 |
| Ohne Assemblies | Beitrag der Repräsentationsprototypen |

Während der Evaluation werden Exploration und Lernen abgeschaltet. Jeder
Modus startet aus demselben Modellcheckpoint und erhält dieselben
Ausgangsseeds. Anschließend wird der exakte Zustand vor der Evaluation
wiederhergestellt.

## 13. Was bestätigt und was offen ist

Technisch bestätigt sind Build, deterministische Topologie, Einfluss aller
128 Kanäle, Snapshot-Invarianz, Dale-Konformität, numerische Stabilität,
Assemblybildung und die kompilierbare Mehrmodus-Evaluation.

Nicht bestätigt ist derzeit:

- strategische Überlegenheit gegenüber der Regelbaseline,
- Vorteil gegenüber der 72-Neuronen-Referenz,
- statistische Signifikanz auf unberührten Seeds,
- bessere Energieeffizienz trotz höherer Kapazität,
- Generalisierung auf andere Spiele.

Diese Aussagen dürfen erst nach dem eingefrorenen Protokoll in
[`LARGESCALE_FORSCHUNGSPROTOKOLL.md`](LARGESCALE_FORSCHUNGSPROTOKOLL.md)
gemacht werden.
