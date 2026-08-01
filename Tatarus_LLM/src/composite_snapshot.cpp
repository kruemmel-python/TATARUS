#include "tatarus_llm/composite_snapshot.hpp"

#include <fstream>
#include <stdexcept>

namespace tatarus::llm {

void saveHostState(const std::filesystem::path& path, const HostPersistentState& state,
                   std::uint64_t nervousHash, std::uint64_t fingerprint) {
    json::Value::Array conversation;
    for (const auto& turn : state.conversation)
        conversation.emplace_back(json::Value::Object{{"role", turn.role}, {"content", turn.content}});
    const json::Value root(json::Value::Object{
        {"format", "tatarus-llm-host-v1"}, {"memory_owner", toString(state.mode)},
        {"last_provider", state.lastProvider}, {"last_model", state.lastModel},
        {"nervous_state_hash", std::to_string(nervousHash)}, {"functional_fingerprint", std::to_string(fingerprint)},
        {"environment", state.environment.toJson()}, {"conversation", std::move(conversation)}});
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Cannot write host snapshot: " + path.string());
    stream << root.dump(2) << '\n';
}

HostPersistentState loadHostState(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot open host snapshot: " + path.string());
    const std::string source((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    const json::Value root = json::Value::parse(source);
    if (json::requiredString(root, "format") != "tatarus-llm-host-v1") throw std::runtime_error("Unsupported host snapshot format");
    HostPersistentState state;
    state.mode = parseMemoryMode(json::requiredString(root, "memory_owner"));
    state.lastProvider = json::requiredString(root, "last_provider"); state.lastModel = json::requiredString(root, "last_model");
    state.environment.fromJson(root.at("environment"));
    for (const auto& turn : root.at("conversation").array())
        state.conversation.push_back({json::requiredString(turn, "role"), json::requiredString(turn, "content")});
    return state;
}

}  // namespace tatarus::llm
