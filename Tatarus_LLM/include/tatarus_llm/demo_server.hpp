#pragma once

#include "tatarus_llm/tatarus_planner_host.hpp"

#include <filesystem>

namespace tatarus::llm {

// Localhost-only research gateway for Custom GPT/Gemini tool demonstrations.
void runDemoServer(TatarusPlannerHost& host, unsigned short port,
                   const std::filesystem::path& snapshotDirectory, bool autosave,
                   const std::filesystem::path& webRoot = {});

}  // namespace tatarus::llm
