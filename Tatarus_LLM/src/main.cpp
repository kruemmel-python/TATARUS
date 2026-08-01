#include "tatarus_llm/gemini_provider.hpp"
#include "tatarus_llm/cognitive_json.hpp"
#include "tatarus_llm/demo_server.hpp"
#include "tatarus_llm/lmstudio_provider.hpp"
#include "tatarus_llm/mini_json.hpp"
#include "tatarus_llm/openai_provider.hpp"
#include "tatarus_llm/tatarus_planner_host.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
struct Options {
    std::string provider = "lmstudio";
    std::string baseUrl;
    std::string model;
    tatarus::llm::MemoryMode mode = tatarus::llm::MemoryMode::Scientific;
    std::filesystem::path snapshot = "state/tatarus_llm";
    std::filesystem::path config;
    std::filesystem::path webRoot;
    std::string once;
    std::uint64_t seed = 7001;
    bool load = false;
    bool autosave = true;
    unsigned short demoPort = 0;
    bool diagnostics = true;
    tatarus::llm::EpisodicMemoryConfig episodicMemory;
    bool episodicModeSet = false;
    bool episodicTopKSet = false;
    bool episodicThresholdSet = false;
    bool episodicPlasticitySet = false;
};

std::string environment(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : "";
}

void usage() {
    std::cout << "TATARUS - A Persistent Synthetic Nervous System / LLM Cortex\n\n"
              << "tatarus_llm [--provider lmstudio|openai|gemini] [--memory-owner tatarus|hybrid|demo]\n"
              << "            [--base-url URL] [--model NAME] [--snapshot-dir DIR] [--seed N]\n"
              << "            [--config FILE] [--load] [--no-autosave] [--once TEXT] [--demo-port N] [--web-root DIR] [--chat-only]\n"
              << "            [--episodic-memory anchored|disabled|lexical-only|shuffled-anchors]\n"
              << "            [--episodic-plasticity enabled|disabled] [--memory-top-k N] [--memory-threshold X]\n\n"
              << "LM Studio ignores --model and discovers the currently loaded model every step.\n";
}

Options options(int argc, char** argv) {
    Options result;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next = [&]() -> std::string { if (++i >= argc) throw std::runtime_error("Missing value after " + arg); return argv[i]; };
        if (arg == "--provider") result.provider = next();
        else if (arg == "--base-url") result.baseUrl = next();
        else if (arg == "--model") result.model = next();
        else if (arg == "--memory-owner") result.mode = tatarus::llm::parseMemoryMode(next());
        else if (arg == "--snapshot-dir") result.snapshot = next();
        else if (arg == "--config") result.config = next();
        else if (arg == "--web-root") result.webRoot = next();
        else if (arg == "--seed") result.seed = std::stoull(next());
        else if (arg == "--once") result.once = next();
        else if (arg == "--demo-port") result.demoPort = static_cast<unsigned short>(std::stoul(next()));
        else if (arg == "--chat-only") result.diagnostics = false;
        else if (arg == "--episodic-memory") { result.episodicMemory.mode = tatarus::llm::parseEpisodicMemoryMode(next()); result.episodicModeSet = true; }
        else if (arg == "--memory-top-k") { result.episodicMemory.topK = std::stoull(next()); result.episodicTopKSet = true; }
        else if (arg == "--memory-threshold") { result.episodicMemory.retrievalThreshold = std::stod(next()); result.episodicThresholdSet = true; }
        else if (arg == "--episodic-plasticity") {
            const auto value = next();
            if (value == "enabled" || value == "on") result.episodicMemory.unsupervisedPlasticityEnabled = true;
            else if (value == "disabled" || value == "off") result.episodicMemory.unsupervisedPlasticityEnabled = false;
            else throw std::runtime_error("Unknown episodic plasticity mode: " + value);
            result.episodicPlasticitySet = true;
        }
        else if (arg == "--load") result.load = true;
        else if (arg == "--no-autosave") result.autosave = false;
        else if (arg == "--help" || arg == "-h") { usage(); std::exit(0); }
        else throw std::runtime_error("Unknown option: " + arg);
    }
    return result;
}

agns::NervousSystemConfig loadConfig(const Options& opts) {
    agns::NervousSystemConfig config; config.seed = opts.seed;
    if (opts.config.empty()) return config;
    std::ifstream input(opts.config, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open config: " + opts.config.string());
    const auto root = tatarus::json::Value::parse(std::string((std::istreambuf_iterator<char>(input)), {}));
    const auto number = [&](const char* key, auto& target) { if (root.contains(key)) target = static_cast<std::remove_reference_t<decltype(target)>>(root.at(key).number()); };
    const auto boolean = [&](const char* key, bool& target) { if (root.contains(key)) target = root.at(key).boolean(); };
    number("sensory_neurons", config.sensoryNeurons); number("excitatory_neurons", config.excitatoryNeurons);
    number("inhibitory_neurons", config.inhibitoryNeurons); number("context_neurons", config.contextNeurons);
    number("motor_neurons", config.motorNeurons); number("modulatory_neurons", config.modulatoryNeurons);
    number("connection_probability", config.connectionProbability); number("dt_ms", config.dtMs);
    number("eligibility_tau_ms", config.eligibilityTauMs); number("learning_rate", config.learningRate);
    number("maximum_assemblies", config.maximumAssemblies);
    boolean("generated_operator_enabled", config.generatedOperatorEnabled);
    boolean("eligibility_memory_enabled", config.eligibilityMemoryEnabled);
    boolean("long_term_plasticity_enabled", config.longTermPlasticityEnabled);
    boolean("structural_plasticity_enabled", config.structuralPlasticityEnabled);
    config.validate(); return config;
}

tatarus::llm::EpisodicMemoryConfig loadEpisodicConfig(const Options& opts) {
    auto config = opts.episodicMemory;
    if (opts.config.empty()) return config;
    std::ifstream input(opts.config, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open config: " + opts.config.string());
    const auto root = tatarus::json::Value::parse(std::string((std::istreambuf_iterator<char>(input)), {}));
    const auto number = [&](const char* key, auto& target) {
        if (root.contains(key)) target = static_cast<std::remove_reference_t<decltype(target)>>(root.at(key).number());
    };
    const auto boolean = [&](const char* key, bool& target) { if (root.contains(key)) target = root.at(key).boolean(); };
    if (!opts.episodicModeSet && root.contains("episodic_memory_mode"))
        config.mode = tatarus::llm::parseEpisodicMemoryMode(root.at("episodic_memory_mode").string());
    number("episodic_maximum_episodes", config.maximumEpisodes);
    number("episodic_maximum_content_bytes", config.maximumContentBytes);
    if (!opts.episodicTopKSet) number("episodic_top_k", config.topK);
    if (!opts.episodicThresholdSet) number("episodic_retrieval_threshold", config.retrievalThreshold);
    number("episodic_lexical_weight", config.lexicalWeight);
    number("episodic_neural_weight", config.neuralWeight);
    number("episodic_recency_weight", config.recencyWeight);
    number("episodic_encoding_synaptic_weight", config.encodingSynapticWeight);
    number("episodic_decoder_spike_threshold", config.decoderSpikeThreshold);
    if (!opts.episodicPlasticitySet)
        boolean("episodic_unsupervised_plasticity_enabled", config.unsupervisedPlasticityEnabled);
    number("episodic_plasticity_epochs", config.plasticityEpochs);
    number("episodic_hebbian_learning_rate", config.hebbianLearningRate);
    number("episodic_recurrent_learning_rate", config.recurrentLearningRate);
    number("episodic_heterosynaptic_depression_rate", config.heterosynapticDepressionRate);
    number("episodic_eligibility_decay", config.eligibilityDecay);
    number("episodic_decoding_margin", config.decodingMargin);
    number("episodic_recurrent_fanout", config.recurrentFanout);
    return config;
}

std::unique_ptr<tatarus::llm::LlmProvider> makeProvider(const Options& opts) {
    using namespace tatarus::llm;
    if (opts.provider == "lmstudio") {
        if (!opts.model.empty()) std::cerr << "Hinweis: --model wird fuer LM Studio ignoriert; das aktuell geladene Modell ist massgeblich.\n";
        return std::make_unique<LmStudioProvider>(opts.baseUrl.empty() ? "http://127.0.0.1:1234" : opts.baseUrl);
    }
    if (opts.provider == "openai") return std::make_unique<OpenAiProvider>(opts.model, environment("OPENAI_API_KEY"), opts.baseUrl.empty() ? "https://api.openai.com/v1" : opts.baseUrl);
    if (opts.provider == "gemini") return std::make_unique<GeminiProvider>(opts.model, environment("GEMINI_API_KEY"), opts.baseUrl.empty() ? "https://generativelanguage.googleapis.com/v1beta" : opts.baseUrl);
    throw std::runtime_error("Unknown provider: " + opts.provider);
}

std::filesystem::path resolveWebRoot(const Options& opts, const char* executable) {
    std::vector<std::filesystem::path> candidates;
    if (!opts.webRoot.empty()) candidates.push_back(opts.webRoot);
    candidates.push_back(std::filesystem::current_path() / "web");
    if (executable != nullptr && *executable != '\0') {
        std::error_code ec;
        const auto absoluteExecutable = std::filesystem::absolute(executable, ec);
        if (!ec) candidates.push_back(absoluteExecutable.parent_path() / "web");
    }
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate / "index.html")) return std::filesystem::canonical(candidate);
    }
    if (!opts.webRoot.empty()) throw std::runtime_error("Web UI not found in: " + opts.webRoot.string());
    return {};
}

void printResult(const tatarus::llm::HostStepResult& result, const tatarus::llm::TatarusPlannerHost& host, bool diagnostics) {
    const auto& c = result.planner.command;
    std::cout << "\nTATARUS: " << result.response.text << "\n";
    if (!result.response.error.empty()) std::cerr << "Sprachfehler: " << result.response.error << '\n';
    if (!diagnostics) return;
    std::cout << "\n[Planung: " << result.planner.provider << " / " << result.planner.model << " / " << result.planner.latencyMs << " ms]\n"
              << "Kommando: attention=" << c.attention << " motor=" << c.motorIntent << " strength=" << c.intentStrength
              << " recall=" << c.recallCue << ":" << c.recallStrength << "\n"
              << "[Sprache: " << result.response.provider << " / " << result.response.model << " / " << result.response.latencyMs << " ms]\n"
              << "Synaptischer Recall: " << result.recalledEpisodes.size() << " Episode(n), "
              << result.reconstructionSpikes << " Rekonstruktionsspikes, "
              << result.reconstructionFailures << " Rekonstruktionsfehler, "
              << result.plasticityUpdates << " lokale Plastizitätsupdates, "
              << result.memorySynapses << " Gedächtnissynapsen\n"
              << "TATARUS: step=" << result.after.step << " fingerprint=" << result.after.functionalFingerprint
              << " confidence=" << result.after.confidence << " reward=" << result.feedback.reward
              << " position=" << host.environment().position() << " target=" << host.environment().target() << "\n";
}
}

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8); SetConsoleCP(CP_UTF8);
#endif
        const Options opts = options(argc, argv);
        const auto episodicConfig = loadEpisodicConfig(opts);
        tatarus::llm::TatarusPlannerHost host(loadConfig(opts), makeProvider(opts), opts.mode,
                                              opts.seed ^ 0x41c64e6dULL, episodicConfig);
        if (opts.load) host.load(opts.snapshot);
        if (opts.demoPort != 0) {
            tatarus::llm::runDemoServer(host, opts.demoPort, opts.snapshot, opts.autosave,
                                        resolveWebRoot(opts, argc > 0 ? argv[0] : nullptr));
            return 0;
        }
        const auto execute = [&](const std::string& text) {
            const auto result = host.step(text); printResult(result, host, opts.diagnostics); if (opts.autosave) host.save(opts.snapshot);
        };
        if (!opts.once.empty()) { execute(opts.once); return 0; }
        std::cout << "TATARUS_LLM bereit. Memory owner: " << tatarus::llm::toString(host.memoryMode())
                  << " | Episodic memory: " << tatarus::llm::toString(episodicConfig.mode)
                  << " | Unsupervised plasticity: " << (episodicConfig.unsupervisedPlasticityEnabled ? "enabled" : "disabled")
                  << "\nBefehle: :state, :memory, :model, :save, :load, :quit\n";
        std::string line;
        while (std::cout << "\n> " && std::getline(std::cin, line)) {
            if (line == ":quit" || line == ":q") break;
            if (line == ":save") { host.save(opts.snapshot); std::cout << "Gespeichert: " << opts.snapshot.string() << '\n'; continue; }
            if (line == ":load") { host.load(opts.snapshot); std::cout << "Geladen: " << opts.snapshot.string() << '\n'; continue; }
            if (line == ":state") { const auto state = host.state(); std::cout << tatarus::llm::cognitiveStateJson(state).dump(2) << '\n'; continue; }
            if (line == ":memory") {
                std::cout << "Episoden=" << host.episodicMemorySize()
                          << " Synapsen=" << host.episodicSynapseCount()
                          << " letzte_Rekonstruktionsspikes=" << host.episodicRecallSpikeCount()
                          << " letzte_Rekonstruktionsfehler=" << host.episodicRecallFailureCount()
                          << " Plastizitaetsupdates=" << host.episodicPlasticityUpdateCount() << '\n';
                continue;
            }
            if (line == ":model") {
                if (auto* lm = dynamic_cast<tatarus::llm::LmStudioProvider*>(&host.provider())) std::cout << lm->discoverLoadedModel() << '\n';
                else std::cout << host.provider().currentModel() << '\n';
                continue;
            }
            if (!line.empty()) execute(line);
        }
        if (opts.autosave) host.save(opts.snapshot);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fehler: " << error.what() << '\n'; return 1;
    }
}
