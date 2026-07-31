Für **TATARUS** gilt: Du lädst es nicht wie ein LoRA oder GGUF „in“ ChatGPT, Gemini oder ein LM-Studio-Modell. TATARUS bleibt ein eigenständiger persistenter C++-Prozess und wird zum **Nervensystem unterhalb des Sprach- oder Planungskerns**.

Die korrekte Architektur ist:

```text
Umwelt / Benutzer
        ↓
TATARUS-SensorFrame
        ↓
persistentes Nervensystem
        ↓
CognitiveState
        ↓
LLM: ChatGPT, Gemini oder lokales Modell
        ↓
begrenzter CognitiveCommand
        ↓
TATARUS
        ↓
Handlung, neue Erfahrung und Reward
```

Deine vorhandene Bridge ist dafür bereits fast exakt gebaut. Sie liefert Repräsentationen, Recall-Zustände, Neuheit, Salienz, Energiebedarf, Aktivitätsbedarf, Vorhersagefehler und Konfidenz. Zurückgegeben werden Aufmerksamkeit, Intent und Recall-Steuerung. Einzelne Neuronen und Synapsen bleiben verborgen.

# Der wichtigste Architekturentscheid

**TATARUS muss den Lebenslauf besitzen – nicht das LLM.**

Also nicht:

```text
ChatGPT läuft
→ ruft gelegentlich TATARUS als Werkzeug auf
```

sondern:

```text
TATARUS läuft dauerhaft
→ ruft bei Bedarf einen austauschbaren Planungskern auf
```

Dadurch kannst du denselben TATARUS-Zustand nacheinander mit:

* einem LM-Studio-Modell,
* einem OpenAI-Modell,
* Gemini,
* einem eigenen C++-Planer

verwenden, ohne den Nervensystemkern zu verändern.

# Drei Betriebsarten

## 1. Wissenschaftlicher Modus

Das LLM erhält bei jedem Schritt nur:

* aktuelle Benutzereingabe oder Umweltbeobachtung,
* aktuellen gepoolten `CognitiveState`,
* zulässige Befehle.

Es erhält **keinen langen Chatverlauf**.

Damit ist TATARUS der einzige persistente Erfahrungsträger:

```text
LLM-Aufruf 1: zustandslos
LLM-Aufruf 2: zustandslos
LLM-Aufruf 3: zustandslos

TATARUS: läuft durchgehend weiter
```

Das ist die sauberste Fortsetzung der Stufe-19-Versuchsmethodik.

## 2. Produktmodus

Das LLM darf zusätzlich einen normalen Gesprächsverlauf besitzen.

Dann existieren zwei Gedächtnisse:

```text
LLM-Kontext
+
TATARUS-Nervenzustand
```

Das ist für eine Anwendung leistungsfähiger, aber wissenschaftlich nicht mehr geeignet, um zu behaupten, dass nur TATARUS die Erinnerung getragen hat.

## 3. Demonstrationsmodus

Ein Custom GPT oder Gemini-Chat ruft TATARUS als externes Werkzeug auf. Das zeigt die Verbindung anschaulich, bildet aber den permanenten Lebenslauf weniger sauber ab als ein eigener TATARUS-Host.

# Universeller C++-Adapter

Ich würde zwischen TATARUS und den Modellen diese Schicht setzen:

```text
llm/
├── llm_provider.hpp
├── openai_provider.cpp
├── gemini_provider.cpp
├── lmstudio_provider.cpp
├── tatarus_planner_host.cpp
├── cognitive_json.cpp
└── bounded_command_validator.cpp
```

Gemeinsames Interface:

```cpp
struct PlannerInput {
    std::string userInput;
    agns::CognitiveState nervousState;
};

struct PlannerOutput {
    agns::AttentionTarget attention;
    double motorIntent;
    double intentStrength;
    std::uint32_t recallCue;
    double recallStrength;
};

class LlmProvider {
public:
    virtual ~LlmProvider() = default;

    virtual PlannerOutput plan(
        const PlannerInput& input) = 0;
};
```

Damit bleibt nur die HTTP-Kommunikation anbieterspezifisch.

# Ein wichtiger Sicherheitsumbau

Dein aktueller `CognitiveCommand` enthält auch:

```cpp
double reward;
```

Für die Anbindung eines LLMs sollte das Modell **seinen Reward nicht selbst festlegen dürfen**. Sonst kann es sich selbst positive Rückmeldungen geben.

Besser:

```cpp
struct PlannerCommand {
    AttentionTarget attention;
    double motorIntent;
    double intentStrength;
    std::uint32_t recallCue;
    double recallStrength;
};

struct EnvironmentFeedback {
    double reward;
};
```

Der Host bildet daraus intern den vorhandenen `CognitiveCommand`:

```cpp
agns::CognitiveCommand command;
command.attention      = planner.attention;
command.motorIntent    = clamp(planner.motorIntent, -1.0, 1.0);
command.intentStrength = clamp(planner.intentStrength, 0.0, 1.0);
command.recallCue      = planner.recallCue % 64U;
command.recallStrength = clamp(planner.recallStrength, 0.0, 1.0);
command.reward         = environment.reward;
```

# Laufzeitzyklus

```cpp
while (running) {
    const SensorFrame observation = environment.observe();

    const CognitiveState nervousState =
        bridge.readState();

    PlannerInput plannerInput{
        .userInput = currentUserInput,
        .nervousState = nervousState
    };

    const PlannerOutput plannerCommand =
        llmProvider.plan(plannerInput);

    const double reward =
        environment.previousConsequence();

    CognitiveCommand command =
        validateAndCombine(plannerCommand, reward);

    const CognitiveStep result =
        bridge.step(observation, command);

    environment.apply(result.action);

    saveCompositeSnapshotWhenRequired();
}
```

# Welche Daten das Modell erhält

Aus `CognitiveState` sollte ein begrenztes JSON erzeugt werden:

```json
{
  "step": 91824,
  "representations": [
    {
      "id": 7,
      "activation": 0.83,
      "familiarity": 0.71,
      "age_ms": 420
    }
  ],
  "recall": [
    {
      "channel": 12,
      "strength": 0.64
    }
  ],
  "novelty": 0.18,
  "salience": 0.76,
  "energy_need": 0.22,
  "activity_need": 0.08,
  "prediction_error": 0.31,
  "confidence": 0.69,
  "functional_fingerprint": 872196102
}
```

Das Modell sieht weiterhin keine:

* Membranpotentiale,
* Synapsenlisten,
* Gewichte,
* Eligibility-Einzelwerte,
* interne Topologie.

# LM Studio anbinden

Das ist für dich der einfachste erste praktische Weg, weil alles lokal bleibt.

LM Studio stellt OpenAI-kompatible Endpunkte wie `/v1/responses` und `/v1/chat/completions` bereit. Der Server läuft typischerweise lokal auf Port `1234`; Tool-Aufrufe werden über Funktionsdefinitionen im Request beschrieben. ([LM Studio][1])

Start:

```powershell
lms server start
```

Oder in LM Studio:

```text
Developer
→ Start Server
```

Der TATARUS-Provider sendet dann beispielsweise an:

```text
http://127.0.0.1:1234/v1/chat/completions
```

Request:

```json
{
  "model": "geladenes-modell",
  "messages": [
    {
      "role": "system",
      "content": "You are the bounded planning core of TATARUS. Return only a valid cognitive command."
    },
    {
      "role": "user",
      "content": "Current observation: ... Current nervous state: ..."
    }
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "submit_cognitive_command",
        "description": "Submit a bounded command to TATARUS.",
        "parameters": {
          "type": "object",
          "properties": {
            "attention": {
              "type": "string",
              "enum": [
                "balanced",
                "vision",
                "audio",
                "touch",
                "text",
                "interoception"
              ]
            },
            "motor_intent": {
              "type": "number",
              "minimum": -1,
              "maximum": 1
            },
            "intent_strength": {
              "type": "number",
              "minimum": 0,
              "maximum": 1
            },
            "recall_cue": {
              "type": "integer",
              "minimum": 0,
              "maximum": 63
            },
            "recall_strength": {
              "type": "number",
              "minimum": 0,
              "maximum": 1
            }
          },
          "required": [
            "attention",
            "motor_intent",
            "intent_strength",
            "recall_cue",
            "recall_strength"
          ],
          "additionalProperties": false
        }
      }
    }
  ],
  "tool_choice": "required"
}
```

LM Studio liefert den gewünschten Funktionsaufruf zurück. Dein C++-Host prüft die Argumente und übergibt sie an die Bridge. Das Modell selbst führt die Funktion nicht aus; dein Programm muss den Tool-Call auswerten und ausführen. ([LM Studio][1])

# OpenAI beziehungsweise ChatGPT anbinden

## OpenAI-Modell über API

Das ist die technisch saubere Variante.

Die OpenAI Responses API kann benutzerdefinierte Function Tools aufrufen. Deine C++-Anwendung sendet den gepoolten TATARUS-Zustand, erhält einen `function_call`, validiert die Argumente und führt den Command lokal aus. ([OpenAI Plattform][2])

Der Ablauf ist derselbe wie bei LM Studio:

```text
OpenAI Responses API
        ↕
OpenAIProvider
        ↕
TATARUS Cognitive Bridge
```

Nur Zieladresse und Authentifizierung ändern sich.

Für Datenschutz und Kosten sendest du ausschließlich den kompakten `CognitiveState`, niemals Snapshot, Topologie oder Synapsenbestand.

## Direkt in der ChatGPT-Oberfläche

Dafür könntest du ein eigenes GPT mit einer **Action** erstellen. GPT Actions verbinden ein GPT über ein OpenAPI-Schema und eine Authentifizierung mit einer externen API. ([OpenAI Help Center][3])

Dazu benötigst du:

```text
TATARUS
→ lokaler oder Server-Wrapper
→ HTTPS-API
→ OpenAPI-Schema
→ Custom GPT Action
```

Das ist für eine öffentliche Demo sinnvoll. Für den wissenschaftlich sauberen persistenten Lebenslauf ist die eigene TATARUS-Anwendung mit OpenAI API besser, weil du dort Kontext, Resetverhalten und Snapshotgrenzen vollständig kontrollierst.

# Gemini anbinden

Gemini besitzt ebenfalls Function Calling. Du definierst die Funktion, Gemini entscheidet über den Aufruf und liefert Funktionsname sowie Parameter; dein C++-Programm führt den Aufruf aus und sendet das Ergebnis anschließend zurück. ([Google AI for Developers][4])

Die gleiche interne Definition kann verwendet werden:

```text
submit_cognitive_command
```

Lediglich die JSON-Verpackung unterscheidet sich vom OpenAI-Format.

Architektur:

```text
Gemini API
    ↕
GeminiProvider
    ↕
gemeinsamer PlannerOutput
    ↕
TATARUS
```

# Meine klare Empfehlung

## Erste Umsetzung

```text
TATARUS
+
LM Studio
+
lokaler OpenAI-kompatibler Tool-Call
```

Vorteile:

* vollständig lokal,
* keine API-Kosten,
* kein Cloudversand,
* leicht debuggbar,
* beliebige lokale Modelle austauschbar,
* passt zu deiner Offline- und C++-Ausrichtung.

## Danach

Füge dieselbe Provider-Schnittstelle für OpenAI und Gemini hinzu:

```text
--provider lmstudio
--provider openai
--provider gemini
```

Beispiel:

```powershell
TatarusAgent.exe ^
  --provider lmstudio ^
  --base-url http://127.0.0.1:1234/v1 ^
  --model qwen2.5-7b-instruct ^
  --memory-owner tatarus
```

# Was dadurch entsteht

Das LLM wird nicht zum Nervensystem umgebaut. Es wird zum austauschbaren höheren Kognitionskern:

```text
TATARUS = Wahrnehmung, Zustand, Gedächtnis, Salienz,
          Regulation, Erfahrung und Handlungskontext

LLM     = Sprache, Abstraktion, Planung und Schlussfolgerung
```

Damit könntest du dasselbe fortlaufende TATARUS-Individuum heute mit einem lokalen Qwen-Modell, morgen mit Gemini und danach mit einem OpenAI-Modell weiterlaufen lassen.

Der persistente Innenzustand bliebe derselbe. Nur der höhere Planungskern würde ausgetauscht.

[1]: https://lmstudio.ai/docs/developer/openai-compat/tools?utm_source=chatgpt.com "Tool Use | LM Studio"
[2]: https://platform.openai.com/docs/quickstart/make-your-first-api-request?utm_source=chatgpt.com "Developer quickstart - OpenAI API"
[3]: https://help.openai.com/en/articles/9442513?utm_source=chatgpt.com "Configuring actions in GPTs | OpenAI Help Center"
[4]: https://ai.google.dev/gemini-api/docs/function-calling?hl=en&utm_source=chatgpt.com "Function calling with the Gemini API  |  Google AI for Developers"
