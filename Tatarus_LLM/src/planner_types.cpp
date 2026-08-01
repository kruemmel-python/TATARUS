#include "tatarus_llm/planner_types.hpp"

#include <stdexcept>

namespace tatarus::llm {

std::string toString(MemoryMode mode) {
    switch (mode) {
        case MemoryMode::Scientific: return "tatarus";
        case MemoryMode::Product: return "hybrid";
        case MemoryMode::Demonstration: return "demo";
    }
    return "tatarus";
}

MemoryMode parseMemoryMode(const std::string& value) {
    if (value == "tatarus" || value == "scientific") return MemoryMode::Scientific;
    if (value == "hybrid" || value == "product") return MemoryMode::Product;
    if (value == "demo" || value == "demonstration") return MemoryMode::Demonstration;
    throw std::runtime_error("Unknown memory mode: " + value);
}

}  // namespace tatarus::llm
