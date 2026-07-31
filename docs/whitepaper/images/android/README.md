# Android-Abbildungen des TATARUS-Whitepapers

Die Dateien in diesem Ordner sind unveränderte PNG-Screenshots der im
Repository enthaltenen Android-Anwendungen. Sie wurden am 31. Juli 2026 per
Android Debug Bridge direkt auf dem registrierten Testgerät `RMX3853`
(`4c90bfcc`) mit 1.264 × 2.780 Pixeln erfasst. Vor jeder Aufnahme wurde die
jeweilige App beendet und über ihre Launcher-Activity neu gestartet.

| Datei | Anwendung | Paket | dargestellter Zustand |
|---|---|---|---|
| `runenkrieg_tatarus_reference_game.png` | Runenkrieg: TATARUS | `de.runenkrieg.game` | interaktiver Spielzustand der fortlaufend lernenden Referenz-App |
| `runenkrieg_tatarus_reference_lab.png` | Runenkrieg: TATARUS | `de.runenkrieg.game` | Laboransicht mit rein neuronalem Modus und lokalem Erfahrungsstand |
| `runenkrieg_tatarus_largescale_game.png` | Runenkrieg: TATARUS LargeScale | `de.runenkrieg.game.large` | Spielzustand des auf 1.024 Neuronen skalierten Systems |
| `runenkrieg_tatarus_largescale_lab.png` | Runenkrieg: TATARUS LargeScale | `de.runenkrieg.game.large` | Laboransicht mit 1.024 Neuronen, 32.768 Synapsen und 128 Kanälen |
| `runenkrieg_tatarus_winner_status.png` | Runenkrieg: TATARUS Winner | `de.runenkrieg.game.tataruswinner` | eingefrorener Seed 20260732 bei 10.000 Runden und identifiziertem Snapshot |
| `runenkrieg_tensorflow_winner_status.png` | Runenkrieg: eingefrorener TF-Gewinner | `de.runenkrieg.game.tensorflowwinner` | eingefrorener LiteRT-Checkpoint mit 128 Kanälen und deaktivierten Gewichtsänderungen |

## SHA-256

```text
1cfeb266fbb08e016633f378feb9b25049d3a2dc59715ae3b91f8274752cf527  runenkrieg_tatarus_largescale_game.png
5320bcce9cfe4477952ca3fce4cbb8833eb7611677f72dbdd5039743360fb39c  runenkrieg_tatarus_largescale_lab.png
5314f9235a98c83d4086d09e4d8660597c0c6a97f6eb2f47d3075e23c3d54535  runenkrieg_tatarus_reference_game.png
d29b1e7e6af7027ee79ab140a825b22b206c90ad6a8b4d5e2012da65feb7215d  runenkrieg_tatarus_reference_lab.png
3f93636dd1cd7d33d4068f49d760b43a0445df7751f9924e1f55ba4becdcde35  runenkrieg_tatarus_winner_status.png
ba5370b4357a62114f533d7ef6c22cdaef67ebaa4290167627aa2e8f3cdc5ae8  runenkrieg_tensorflow_winner_status.png
```

Die Statusleiste wurde nicht zugeschnitten. Die Bilder enthalten keine
Benachrichtigungsinhalte oder personenbezogenen Daten; sichtbar sind nur die
vom Betriebssystem ausgegebenen Uhrzeit-, Netz- und Akkusymbole.
