# Eingefrorene Delayed-XOR-Holdoutbestätigung

Modellhash: `EECE7A502A958561`  
Bestätigungshash: `992F651C727D3C20`

- Kernel-Accuracy: 0.892578
- untere einseitige 95-%-Grenze: 0.876953
- mittlere Verzögerung: 0.871094
- lange Verzögerung: 0.914062
- zuverlässig gelernt: **JA**
- scoped Effizienz repliziert: **NEIN**
- alle Kontrollen übertroffen: **NEIN**

| Kontrolle | ΔAccuracy | untere Grenze | Kostenvorteil | p Holm Kosten |
|---|---:|---:|---:|---:|
| constant | 0.006510 | -0.003255 | 0.049973 | 0.491952 |
| disabled | 0.000000 | -0.009115 | 0.396008 | 0.000085 |
| sign | 0.000651 | -0.002604 | -0.004628 | 0.614213 |
| tanh | -0.003255 | -0.011068 | 0.037617 | 0.491952 |
| random | 0.002604 | -0.005859 | 0.032859 | 0.491952 |

Die Bestätigungs-Seeds wurden erst nach Einfrieren des Modellhashes ausgewertet.
