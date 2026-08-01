# Rückverfolgbarkeit zu `Agents_update.md`

| Forderung | Umsetzung | Verifikation |
|---|---|---|
| TATARUS besitzt den Lebenslauf | `TatarusPlannerHost`, `NeuralEpisodicMemory`, zusammengesetzter Snapshot | Snapshot-Roundtrip mit identischem `stateHash`, CognitiveState und Episodenabruf |
| austauschbarer Planungskern | `LlmProvider` und drei Provider | Provider-Protokolltests |
| wissenschaftlicher Modus ohne lange History | `MemoryMode::Scientific`, Requestfilter | History-Ablationstest |
| semantischer Chatabruf ohne LLM-History | synaptisch rekonstruierte, neuronal verankerte Episoden und Top-k-Retrieval | Live-Prozessneustart mit 0 Conversation-Turns |
| kein persistierter Episodentext | `TSMEMV3` speichert ausschließlich Metadaten, Anker, Gewichte, Eligibility und Synapsen | Binärscan auf Encodingtext und Codes |
| spontane Gewichtsbildung | identische inhaltsfreie Ausgangstopologie, lokale Hebb-/Eligibility-Plastizität und heterosynaptische Depression | Plastizitätszähler und Weight-Hash-Test |
| Rekonstruktion aus Spikes | rekrutierte Assemblies, 24 komplementäre Codekanäle und rekurrente WTA-Übergänge | exakter UTF-8-Roundtrip plus Spikezähler |
| Inhalt nicht in Topologie | gleich lange unterschiedliche Texte erzeugen gleiche Topologie-, aber verschiedene Gewichtshashes | Strukturkausalitätstest |
| Plastizität essentiell | identische Struktur mit abgeschalteter Plastizität bleibt undecodierbar | `PLASTICITY_OFF`-Kontrolle |
| Gewichte essentiell | deterministische vollständige Gewichtsläsion bei unveränderter Topologie | `WEIGHT_LESION`-Kontrolle |
| inhaltliche Pflichtbaseline | `EpisodicMemoryMode::LexicalOnly` rekonstruiert synaptisch, ignoriert Zustandsanker | Abruf- und Persistenztest |
| neuronale Zuordnung ablatieren | `ShuffledAnchors` verändert alle Anker deterministisch | Negativkontrolle und Unit-Test |
| Gedächtnis vollständig neutralisieren | `Disabled` speichert und liefert keine Episoden | Negativkontrolle und Unit-Test |
| Produktmodus mit Doppelgedächtnis | begrenzte, persistierte Conversation-Turns | Requesttest und Snapshotcode |
| Demonstrationsmodus | localhost HTTP-Gateway und OpenAPI 3.1 | realer `POST /v1/step`-Smoke-Test |
| nur gepoolter CognitiveState | `cognitiveStateJson` | Privacy-Allowlist-Test |
| kein neuronaler Detailzugriff des LLM | keine Detailtypen im Providerinterface | Test auf verbotene Feldnamen |
| Planner darf Reward nicht setzen | getrennte Typen `PlannerCommand`/`EnvironmentFeedback` | Reward-Injection-Test |
| Wertebereiche erzwingen | `validateAndBound` | Grenzwerttest |
| LM Studio Tool-Calling | `/v1/chat/completions`, `tool_choice=required` | Live-Test mit geladenem Modell |
| aktuelles geladenes Modell verwenden | Discovery vor jedem `plan()` | Modell-A→B-Faketest und Live-Test |
| OpenAI anbinden | Responses API Function Tool | Protokolltest mit Responses-Fixture |
| Gemini anbinden | `generateContent` Function Calling | Protokolltest mit Gemini-Fixture |
| gleicher TATARUS-Zustand bei Providerwechsel | Snapshot enthält keine Providerbindung | Architektur- und Snapshottest |
| C++ und lokale Ausrichtung | C++20, WinHTTP, Winsock, keine Frameworkabhängigkeit | MSVC-Release-Build |
| natürlichsprachlicher Chat | getrennte `plan()`-/`respond()`-Phasen | Provider-Fixtures und echter LM-Studio-Chat |
| Text darf keine Steuerung umgehen | Sprachrequest ohne Tools, Ausgabe nur als String | Test auf fehlende Tools im Sprachrequest |
| Synapsensnapshot begrenzen und validieren | Größen-, Rollen-, Kanal-, Endlichkeits-, ID-, Topologie- und Prüfsummenprüfung | Unit- und Roundtriptest |
| Legacy migrieren | V1/V2 rekonstruieren, als sensorischen Strom lokal in V3 lernen und V1-Datei löschen | Host- und reale V2→V3-Migration |
| gepoolten Zustand über Neustart erhalten | Bridge-Snapshot V2, V1-Lesekompatibilität | TATARUS-Kernregression und verstärkter Host-Snapshottest |

Die Tabelle dokumentiert Implementierung, nicht wissenschaftliche Bestätigung. Leistungs- und Gedächtnishypothesen müssen gemäß `EXPERIMENT_PROTOCOL.md` auf unberührten Seeds geprüft werden.
