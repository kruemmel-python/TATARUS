# Domänenübergreifende Hypothesen

## Priorisierte Rolle: reset-gekoppelter Post-Spike-Dämpfer

**Mathematische und zeitliche Basis:** Der Kandidat trennt negative von
positiven Zustandsabweichungen. Im implementierten Wrapper wird das Gate im
Zeitschritt nach einem Spike aus der bereits zurückgesetzten Membran berechnet.
Jeder tatsächlich übertragene Spike sieht deshalb denselben normierten Zustand
`-1/3` und dasselbe Gate `0,1283111212878475`.

```text
g_j(t) = clip((1 + tanh(K(z_reset))) / 2, 0.05, 0.95)
I_syn,i += w_ij * spike_j * g_j
```

**Experimentell bestätigt:** Über fünf Seeds sind die wirksame Gatevarianz,
die Spannungs-RMSE gegen `g=0,1283111212878475` und die Spikeabweichung jeweils
exakt null. Der beobachtete Phänotyp ist konstante resetgebundene Abschwächung.

**Widerlegte frühere Hypothese:** fortlaufend zustandsabhängige synaptische
Wirksamkeit im aktuellen Wrapper.

**Beleg:** Die frühere globale Konstantkontrolle `≈0,812` matchte viele
berechnete, aber wirkungslose Gates. Die event-konditionierte Konstante
reproduziert den Kernel bitgenau.

## Kausale Emissionsvariante

`EMISSION_STATE_GATE` speichert das Gate vor dem Reset und verwendet es beim
Folgeschritt. Damit ist die zeitliche Kopplung korrekt. Allerdings liegen alle
pre-reset normierten Spannungen auf dem positiven Kernelplateau:
`g_eff≈0,88934049`, maximale Varianz `7,4×10^-17`. Auch diese Variante ist mit
diesem Feature praktisch konstant. Für echte Dynamik muss ein am Spikezeitpunkt
vorzeichenwechselndes Feature geprüft werden, etwa der signierte synaptische
Nettostrom.

## Drei fachübergreifende Anwendungen

1. **Neurodynamik – bestätigt im synthetischen Wrapper:** reset- bzw.
   refraktärgekoppelte Kurzzeitabschwächung rekurrenter Spikeübertragung.
   Reale synaptische Depression ist damit nicht biologisch validiert.

2. **Robotik/Regelung – Hypothese:** richtungsselektives Kopplungsgate für
   Fehler- oder Geschwindigkeitsabweichungen. Erfolgskriterium sind geringeres
   Überschwingen und schnellere Erholung gegenüber `sign`, `tanh` und
   Hysterese-Baselines.

3. **Signalverarbeitung – Hypothese:** Polaritätsmerkmal für Ereignisströme,
   das Amplitudenausreißer komprimiert. Erfolgskriterium sind bessere
   Rauschrobustheit oder geringere Quantisierungsfehler als ein einfaches
   Vorzeichenbit.

4. **Netzwerkdynamik – überraschende Hypothese:** temporärer
   Kopplungsunterbrecher zwischen Subsystemen. Erfolgskriterium sind kontrollierte
   Desynchronisation und anschließende Wiederkopplung gegenüber binären Gates.

Die Rangfolge, Wrappergleichungen und Falsifikationskriterien sind maschinenlesbar
in `role_affinities.json` abgelegt.
