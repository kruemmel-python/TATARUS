#include "tatarus_llm/bounded_command_validator.hpp"
#include "tatarus_llm/cognitive_json.hpp"
#include "tatarus_llm/lmstudio_provider.hpp"
#include "tatarus_llm/openai_provider.hpp"
#include "tatarus_llm/gemini_provider.hpp"
#include "tatarus_llm/episodic_memory.hpp"
#include "tatarus_llm/tatarus_planner_host.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }

void webAssetsTest() {
    const auto web = std::filesystem::current_path() / "web";
    for (const auto& name : {"index.html", "styles.css", "app.js", "favicon.svg"})
        require(std::filesystem::is_regular_file(web / name), "Packaged Web UI asset is missing: " + std::string(name));
    std::ifstream html(web / "index.html", std::ios::binary);
    std::ifstream script(web / "app.js", std::ios::binary);
    std::ifstream styles(web / "styles.css", std::ios::binary);
    const std::string htmlText((std::istreambuf_iterator<char>(html)), {});
    const std::string scriptText((std::istreambuf_iterator<char>(script)), {});
    const std::string styleText((std::istreambuf_iterator<char>(styles)), {});
    require(htmlText.find("Neural Chat") != std::string::npos, "Web UI entry point is not the TATARUS chat");
    require(scriptText.find("/v1/step") != std::string::npos && scriptText.find("user_input") != std::string::npos,
            "Web UI is not wired to the bounded TATARUS step endpoint");
    require(scriptText.find("innerHTML") == std::string::npos, "Web UI must not inject LLM text through innerHTML");
    require(styleText.find("grid-template-rows: auto minmax(0, 1fr) auto") != std::string::npos &&
            styleText.find("overflow-y: scroll") != std::string::npos &&
            styleText.find("height: 100dvh") != std::string::npos,
            "Web UI lost its viewport-bounded independently scrollable panes");
}

class FakeHttp final : public tatarus::llm::HttpClient {
public:
    std::vector<tatarus::llm::HttpResponse> gets;
    std::vector<std::string> posts;
    std::vector<std::string> postUrls;
    tatarus::llm::HttpResponse postResponse{200, R"({"choices":[{"message":{"tool_calls":[{"function":{"name":"submit_cognitive_command","arguments":"{\"attention\":\"text\",\"motor_intent\":2.0,\"intent_strength\":0.75,\"recall_cue\":9,\"recall_strength\":0.5}"}}]}}]})"};
    tatarus::llm::HttpResponse get(const std::string&, const std::map<std::string, std::string>&) override {
        if (gets.empty()) throw std::runtime_error("Unexpected fake GET");
        auto result = gets.front(); gets.erase(gets.begin()); return result;
    }
    tatarus::llm::HttpResponse post(const std::string& url, const std::string& body, const std::map<std::string, std::string>&) override {
        postUrls.push_back(url); posts.push_back(body); return postResponse;
    }
};

class DeterministicProvider final : public tatarus::llm::LlmProvider {
public:
    tatarus::llm::PlannerOutput plan(const tatarus::llm::PlannerInput&) override {
        return {{"text", 0.5, 1.0, 3, 0.5}, "test", "deterministic", 0};
    }
    tatarus::llm::ChatOutput respond(const tatarus::llm::ChatInput& input) override {
        return {"Echo: " + input.userInput, "test", "deterministic", 0, {}};
    }
    std::string providerName() const override { return "test"; }
    std::string currentModel() const override { return "deterministic"; }
};

void jsonTest() {
    const auto parsed = tatarus::json::Value::parse(R"({"a":[1,true,"x"],"unicode":"TATARUS"})");
    require(parsed.at("a").array().size() == 3, "JSON array parse failed");
    require(tatarus::json::Value::parse(parsed.dump()).at("unicode").string() == "TATARUS", "JSON roundtrip failed");
}

void privacyAndMemoryModeTest() {
    agns::CognitiveState state; state.step = 42; state.functionalFingerprint = 99; state.novelty = 0.5;
    state.activeRepresentations.push_back({7, 0.8, 0.2, 12.0});
    const std::string pooled = tatarus::llm::cognitiveStateJson(state).dump();
    for (const std::string forbidden : {"synapse", "weight", "membrane", "eligibility", "topology", "neuron"})
        require(pooled.find(forbidden) == std::string::npos, "Private neural field leaked: " + forbidden);
    tatarus::llm::PlannerInput scientific{"now", state, tatarus::llm::MemoryMode::Scientific, {{"user", "old secret"}}};
    scientific.recalledEpisodes = {{17, "user", "selected neural memory", 0.8, 2}};
    const std::string sciRequest = tatarus::llm::buildOpenAiChatRequest(scientific, "m").dump();
    require(sciRequest.find("old secret") == std::string::npos, "Scientific mode leaked conversation history");
    require(sciRequest.find("selected neural memory") != std::string::npos, "Scientific mode omitted selected TATARUS episode");
    scientific.memoryMode = tatarus::llm::MemoryMode::Product;
    require(tatarus::llm::buildOpenAiChatRequest(scientific, "m").dump().find("old secret") != std::string::npos,
            "Product mode omitted requested history");
    tatarus::llm::ChatInput chat; chat.userInput = "now"; chat.nervousState = state;
    chat.conversation = {{"user", "old secret"}};
    chat.recalledEpisodes = {{17, "user", "selected neural memory", 0.8, 2}};
    require(tatarus::llm::buildChatResponseRequest(chat, "m").dump().find("old secret") == std::string::npos,
             "Scientific language phase leaked conversation history");
    require(tatarus::llm::buildChatResponseRequest(chat, "m").dump().find("selected neural memory") != std::string::npos,
            "Scientific language phase omitted selected TATARUS episode");
    chat.memoryMode = tatarus::llm::MemoryMode::Product;
    require(tatarus::llm::buildChatResponseRequest(chat, "m").dump().find("old secret") != std::string::npos,
            "Product language phase omitted conversation history");
}

void validatorTest() {
    const auto valid = tatarus::json::Value::parse(R"({"attention":"vision","motor_intent":4,"intent_strength":-2,"recall_cue":130,"recall_strength":3})");
    const auto command = tatarus::llm::validateAndBound(tatarus::llm::parseToolArguments(valid));
    require(command.motorIntent == 1.0 && command.intentStrength == 0.0 && command.recallCue == 2 && command.recallStrength == 1.0,
            "Command bounds failed");
    bool rejected = false;
    try { (void)tatarus::llm::parseToolArguments(tatarus::json::Value::parse(R"({"attention":"vision","motor_intent":0,"intent_strength":1,"recall_cue":1,"recall_strength":1,"reward":1})")); }
    catch (...) { rejected = true; }
    require(rejected, "Planner reward injection was accepted");
    const auto neural = tatarus::llm::toNervousCommand(command, {0.37});
    require(std::abs(neural.reward - 0.37) < 1e-12, "Environment reward was not authoritative");
}

agns::CognitiveState memoryState(std::uint64_t step) {
    agns::CognitiveState state; state.step = step; state.functionalFingerprint = 0x123456789abcdef0ULL;
    state.activeRepresentations.push_back({42, 0.9, 0.4, 0.0});
    state.recalledStates.push_back({7, 0.8}); state.recalledStates.push_back({23, -0.25});
    return state;
}

void episodicMemoryTest() {
    tatarus::llm::EpisodicMemoryConfig config; config.retrievalThreshold = 0.20;
    tatarus::llm::NeuralEpisodicMemory memory(config);
    const std::string exactContent = "Merke dir exakt den Code VIOLETT-7319 mit UTF-8: ÄÖÜ.";
    memory.encode("user", exactContent, memoryState(10));
    const auto recalled = memory.recall("Welchen Code habe ich dir gegeben?", memoryState(11));
    require(recalled.size() == 1 && recalled.front().content == exactContent,
            "Spike reconstruction did not reproduce the exact UTF-8 episode");
    require(memory.synapseCount() > exactContent.size() && memory.lastRecallSpikeCount() > exactContent.size(),
            "Episodic recall did not traverse synapses and emit decoder spikes");
    require(memory.plasticityUpdateCount() > 0 && memory.lastRecallFailureCount() == 0,
            "Episode was not formed by local unsupervised plasticity");

    auto untrainedConfig = config;
    untrainedConfig.unsupervisedPlasticityEnabled = false;
    tatarus::llm::NeuralEpisodicMemory untrained(untrainedConfig);
    untrained.encode("user", exactContent, memoryState(10));
    require(untrained.synapseCount() == memory.synapseCount() && untrained.plasticityUpdateCount() == 0,
            "Untrained control did not preserve identical reservoir structure");
    require(untrained.reservoirTopologyHash() == memory.reservoirTopologyHash() &&
            untrained.reservoirWeightHash() != memory.reservoirWeightHash(),
            "Episode content was not isolated to the plastic reservoir weights");
    require(untrained.recall("Welchen Code?", memoryState(11)).empty() && untrained.lastRecallFailureCount() == 1,
            "Random untrained reservoir unexpectedly reconstructed the episode");

    auto lesioned = memory;
    require(lesioned.applyWeightLesion(1.0, 99173) > 0, "Synaptic lesion did not alter reservoir weights");
    require(lesioned.recall("Welchen Code?", memoryState(11)).empty() && lesioned.lastRecallFailureCount() == 1,
            "Fully lesioned reservoir retained the learned episode");

    tatarus::llm::NeuralEpisodicMemory sameLengthA(config), sameLengthB(config);
    sameLengthA.encode("user", "AAAA-1111", memoryState(10));
    sameLengthB.encode("user", "BBBB-2222", memoryState(10));
    require(sameLengthA.reservoirTopologyHash() == sameLengthB.reservoirTopologyHash() &&
            sameLengthA.reservoirWeightHash() != sameLengthB.reservoirWeightHash(),
            "Equal-length contents changed topology instead of only learned weights");

    config.mode = tatarus::llm::EpisodicMemoryMode::Disabled;
    tatarus::llm::NeuralEpisodicMemory disabled(config);
    disabled.encode("user", "Code VIOLETT-7319", memoryState(10));
    require(disabled.recall("Welcher Code?", memoryState(11)).empty(), "Disabled episodic memory produced recall");

    config.mode = tatarus::llm::EpisodicMemoryMode::ShuffledAnchors;
    tatarus::llm::NeuralEpisodicMemory shuffled(config);
    shuffled.encode("user", "Merke den Code VIOLETT-7319", memoryState(10));
    require(shuffled.recall("Welcher Code?", memoryState(11)).empty(), "Shuffled neural anchors retained the episode");

    config.mode = tatarus::llm::EpisodicMemoryMode::LexicalOnly;
    tatarus::llm::NeuralEpisodicMemory lexical(config);
    lexical.encode("user", "Merke den Code VIOLETT-7319", memoryState(10));
    require(lexical.recall("Welcher Code?", {}).size() == 1, "Lexical-only control unexpectedly required neural anchors");

    const auto path = std::filesystem::temp_directory_path() / "TatarusLLM_Synaptic_Episodic_Test.bin";
    memory.save(path); tatarus::llm::NeuralEpisodicMemory restored;
    restored.load(path);
    const auto restoredRecall = restored.recall("Welchen Code?", memoryState(12));
    require(restoredRecall.size() == 1 && restoredRecall.front().content == exactContent,
            "Synaptic episodic persistence did not reconstruct exact content");
    {
        std::ifstream binary(path, std::ios::binary);
        const std::string persisted((std::istreambuf_iterator<char>(binary)), {});
        require(persisted.find("VIOLETT-7319") == std::string::npos && persisted.find("Merke dir exakt") == std::string::npos,
                "Synaptic snapshot still contains persisted plaintext");
    }
    const auto damagedPath = std::filesystem::temp_directory_path() / "TatarusLLM_Synaptic_Episodic_Damaged.bin";
    std::filesystem::copy_file(path, damagedPath, std::filesystem::copy_options::overwrite_existing);
    {
        // TSMEMV3 header is 37 bytes; the first episode checksum begins at byte 70.
        std::fstream damaged(damagedPath, std::ios::binary | std::ios::in | std::ios::out);
        damaged.seekg(70);
        char byte = 0;
        damaged.read(&byte, 1);
        byte ^= 0x01;
        damaged.seekp(70);
        damaged.write(&byte, 1);
    }
    tatarus::llm::NeuralEpisodicMemory damaged;
    damaged.load(damagedPath);
    require(damaged.recall("Welchen Code?", memoryState(12)).empty() && damaged.lastRecallFailureCount() == 1,
            "Checksum-damaged synaptic snapshot produced a recalled episode");

    const auto legacyPath = std::filesystem::temp_directory_path() / "TatarusLLM_Episodic_Legacy_Test.json";
    {
        std::ofstream legacy(legacyPath, std::ios::binary | std::ios::trunc);
        legacy << R"({"format":"tatarus-neural-episodic-memory-v1","saved_mode":"anchored","next_id":"2","episodes":[{"id":"1","step":"10","fingerprint":"1311768467463790320","role":"user","content":"Merke Code LEGACY-4401","cue_terms":["code"],"representations":["42"],"recall_anchor":[{"channel":7,"value":0.8}]}]})";
    }
    tatarus::llm::NeuralEpisodicMemory migrated;
    migrated.load(legacyPath);
    const auto migratedRecall = migrated.recall("Welcher Code?", memoryState(12));
    require(migratedRecall.size() == 1 && migratedRecall.front().content.find("LEGACY-4401") != std::string::npos,
            "Legacy plaintext snapshot was not migrated into synapses");
    const auto migratedPath = std::filesystem::temp_directory_path() / "TatarusLLM_Episodic_Migrated.bin";
    migrated.save(migratedPath);
    {
        std::ifstream binary(migratedPath, std::ios::binary);
        const std::string persisted((std::istreambuf_iterator<char>(binary)), {});
        require(persisted.find("LEGACY-4401") == std::string::npos, "Migrated synaptic snapshot retained legacy plaintext");
    }

    const auto corruptPath = std::filesystem::temp_directory_path() / "TatarusLLM_Episodic_Corrupt_Test.json";
    {
        std::ofstream corrupt(corruptPath, std::ios::binary | std::ios::trunc);
        corrupt << R"({"format":"tatarus-neural-episodic-memory-v1","saved_mode":"anchored","next_id":"2","episodes":[{"id":"1","step":"1","fingerprint":"1","role":"system","content":"bad","cue_terms":[],"representations":[],"recall_anchor":[]}]})";
    }
    bool corruptRejected = false;
    try { restored.load(corruptPath); } catch (...) { corruptRejected = true; }
    require(corruptRejected, "Invalid episodic snapshot entry was accepted");
}

void lmStudioTest() {
    auto http = std::make_shared<FakeHttp>();
    http->gets.push_back({200, R"({"models":[{"type":"llm","key":"model-a","loaded_instances":[{"id":"instance-a"}]}]})"});
    http->gets.push_back({200, R"({"models":[{"type":"llm","key":"model-b","loaded_instances":[{"id":"instance-b"}]}]})"});
    tatarus::llm::LmStudioProvider provider("http://test", http);
    tatarus::llm::PlannerInput input;
    auto first = provider.plan(input); auto second = provider.plan(input);
    require(first.model == "instance-a" && second.model == "instance-b", "Loaded LM Studio model was not refreshed per step");
    require(first.command.motorIntent == 1.0, "LM Studio tool command was not bounded");
    require(http->posts.size() == 2 && http->posts[1].find("instance-b") != std::string::npos, "Discovered model not used in request");

    auto ambiguous = std::make_shared<FakeHttp>();
    ambiguous->gets.push_back({200, R"({"models":[{"type":"llm","key":"a","loaded_instances":[{}]},{"type":"llm","key":"b","loaded_instances":[{}]}]})"});
    bool rejected = false;
    try { (void)tatarus::llm::LmStudioProvider("http://test", ambiguous).discoverLoadedModel(); } catch (...) { rejected = true; }
    require(rejected, "Ambiguous loaded-model state was accepted");

    auto chatHttp = std::make_shared<FakeHttp>();
    chatHttp->gets.push_back({200, R"({"models":[{"type":"llm","key":"chat-model","loaded_instances":[{"id":"chat-instance"}]}]})"});
    chatHttp->postResponse = {200, R"({"choices":[{"message":{"content":"Hallo aus TATARUS."}}]})"};
    tatarus::llm::LmStudioProvider chatProvider("http://test", chatHttp);
    tatarus::llm::ChatInput chatInput; chatInput.userInput = "Hallo";
    const auto chat = chatProvider.respond(chatInput);
    require(chat.text == "Hallo aus TATARUS." && chat.model == "chat-instance", "LM Studio language response failed");
    require(chatHttp->posts.front().find("tools") == std::string::npos, "Display-only language phase exposed command tools");
}

void cloudProviderProtocolTest() {
    auto openaiHttp = std::make_shared<FakeHttp>();
    openaiHttp->postResponse = {200, R"({"output":[{"type":"function_call","call_id":"c1","name":"submit_cognitive_command","arguments":"{\"attention\":\"balanced\",\"motor_intent\":0.2,\"intent_strength\":0.3,\"recall_cue\":4,\"recall_strength\":0.5}"}]})"};
    tatarus::llm::OpenAiProvider openai("test-model", "test-key", "https://example.test/v1", openaiHttp);
    const auto openaiResult = openai.plan({});
    require(openaiResult.command.recallCue == 4 && openaiHttp->postUrls.front().ends_with("/responses"), "OpenAI Responses protocol failed");
    require(openaiHttp->posts.front().find("parallel_tool_calls") != std::string::npos && openaiHttp->posts.front().find("\"store\":false") != std::string::npos,
            "OpenAI scientific request lacks isolation controls");
    openaiHttp->postResponse = {200, R"({"output":[{"type":"message","content":[{"type":"output_text","text":"OpenAI chat text"}]}]})"};
    tatarus::llm::ChatInput openaiChat; openaiChat.userInput = "Hallo";
    require(openai.respond(openaiChat).text == "OpenAI chat text", "OpenAI language response failed");

    auto geminiHttp = std::make_shared<FakeHttp>();
    geminiHttp->postResponse = {200, R"({"candidates":[{"content":{"parts":[{"functionCall":{"name":"submit_cognitive_command","args":{"attention":"audio","motor_intent":-0.2,"intent_strength":0.6,"recall_cue":5,"recall_strength":0.7}}}]}}]})"};
    tatarus::llm::GeminiProvider gemini("gemini-test", "test-key", "https://example.test/v1beta", geminiHttp);
    const auto geminiResult = gemini.plan({});
    require(geminiResult.command.attention == "audio" && geminiHttp->postUrls.front().find(":generateContent?key=") != std::string::npos,
            "Gemini function-calling protocol failed");
    geminiHttp->postResponse = {200, R"({"candidates":[{"content":{"parts":[{"text":"Gemini chat text"}]}}]})"};
    tatarus::llm::ChatInput geminiChat; geminiChat.userInput = "Hallo";
    require(gemini.respond(geminiChat).text == "Gemini chat text", "Gemini language response failed");
}

void snapshotTest() {
    agns::NervousSystemConfig config; config.seed = 1234; config.structuralPlasticityEnabled = false;
    const auto root = std::filesystem::temp_directory_path() / "TatarusLLM_Snapshot_Test_1234";
    std::filesystem::remove_all(root);
    tatarus::llm::TatarusPlannerHost source(config, std::make_unique<DeterministicProvider>());
    (void)source.step("memory pulse"); (void)source.step("recall");
    require(source.step("visible reply").response.text == "Echo: visible reply", "Host did not return language response");
    source.save(root); const auto expectedHash = source.stateHash(); const auto expectedFingerprint = source.state().functionalFingerprint;
    require(std::filesystem::exists(root / "synaptic_memory.bin") && !std::filesystem::exists(root / "episodic_memory.json"),
            "Host snapshot did not exclusively persist synaptic episodic memory");
    {
        std::ifstream binary(root / "synaptic_memory.bin", std::ios::binary);
        const std::string persisted((std::istreambuf_iterator<char>(binary)), {});
        require(persisted.find("memory pulse") == std::string::npos && persisted.find("visible reply") == std::string::npos,
                "Host synaptic snapshot leaked conversation plaintext");
    }
    const auto expectedPosition = source.environment().position();
    tatarus::llm::TatarusPlannerHost restored(config, std::make_unique<DeterministicProvider>());
    restored.load(root);
    require(restored.stateHash() == expectedHash, "Nervous system snapshot hash changed");
    require(restored.state().functionalFingerprint == expectedFingerprint && !restored.state().recalledStates.empty(),
            "Cognitive bridge snapshot did not restore the pooled neural state");
    require(std::abs(restored.environment().position() - expectedPosition) < 1e-12, "Environment snapshot changed");
    const auto restartRecall = restored.step("What was the memory pulse?");
    require(!restartRecall.recalledEpisodes.empty() && restartRecall.recalledEpisodes.front().content.find("memory pulse") != std::string::npos,
            "Host did not reconstruct the remembered episode from synapses after restart");

    const auto legacyHostRoot = std::filesystem::temp_directory_path() / "TatarusLLM_Legacy_Host_Migration_1234";
    std::filesystem::remove_all(legacyHostRoot);
    std::filesystem::create_directories(legacyHostRoot);
    for (const auto& name : {"nervous_system.agns", "cognitive_bridge.bin", "host_state.json"})
        std::filesystem::copy_file(root / name, legacyHostRoot / name, std::filesystem::copy_options::overwrite_existing);
    {
        std::ofstream legacy(legacyHostRoot / "episodic_memory.json", std::ios::binary | std::ios::trunc);
        legacy << R"({"format":"tatarus-neural-episodic-memory-v1","saved_mode":"anchored","next_id":"2","episodes":[{"id":"1","step":"3","fingerprint":")"
               << expectedFingerprint
               << R"(","role":"user","content":"Host legacy secret MIGRATE-8821","cue_terms":["legacy"],"representations":[],"recall_anchor":[]}]})";
    }
    tatarus::llm::TatarusPlannerHost legacyHost(config, std::make_unique<DeterministicProvider>());
    legacyHost.load(legacyHostRoot);
    legacyHost.save(legacyHostRoot);
    require(std::filesystem::exists(legacyHostRoot / "synaptic_memory.bin") &&
            !std::filesystem::exists(legacyHostRoot / "episodic_memory.json"),
            "Host did not remove the legacy plaintext snapshot after synaptic migration");
    {
        std::ifstream binary(legacyHostRoot / "synaptic_memory.bin", std::ios::binary);
        const std::string persisted((std::istreambuf_iterator<char>(binary)), {});
        require(persisted.find("MIGRATE-8821") == std::string::npos, "Host migration left plaintext in synaptic storage");
    }

    tatarus::llm::TatarusPlannerHost product(config, std::make_unique<DeterministicProvider>(), tatarus::llm::MemoryMode::Product);
    (void)product.step("visible product turn");
    const auto productRoot = root / "product"; product.save(productRoot);
    std::ifstream input(productRoot / "host_state.json", std::ios::binary);
    const auto hostState = tatarus::json::Value::parse(std::string((std::istreambuf_iterator<char>(input)), {}));
    const auto& conversation = hostState.at("conversation").array();
    require(conversation.size() == 2 && conversation[1].at("content").string() == "Echo: visible product turn",
            "Product snapshot did not persist visible language response");
    require(hostState.dump().find("motor_intent") == std::string::npos, "Internal planner command leaked into product chat history");
}
}

int main() {
    try {
        webAssetsTest(); jsonTest(); privacyAndMemoryModeTest(); validatorTest(); episodicMemoryTest(); lmStudioTest(); cloudProviderProtocolTest(); snapshotTest();
        std::cout << "All Tatarus_LLM tests passed.\n"; return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n'; return 1;
    }
}
