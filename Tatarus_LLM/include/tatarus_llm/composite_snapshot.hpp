#pragma once

#include "tatarus_llm/interactive_environment.hpp"
#include "tatarus_llm/planner_types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tatarus::llm {

struct HostPersistentState {
    MemoryMode mode = MemoryMode::Scientific;
    std::string lastProvider;
    std::string lastModel;
    std::vector<ConversationTurn> conversation;
    InteractiveEnvironment environment;
};

void saveHostState(const std::filesystem::path& path, const HostPersistentState& state,
                   std::uint64_t nervousHash, std::uint64_t fingerprint);
HostPersistentState loadHostState(const std::filesystem::path& path);

}  // namespace tatarus::llm
