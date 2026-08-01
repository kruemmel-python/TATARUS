cmake_minimum_required(VERSION 3.21)
project(Tatarus_LLM VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TATARUS_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../research/ag_signal_morpher_1ee27305a6aa/06_temporal_classification/ui_cpp")
set(TATARUS_GENERATED "${CMAKE_CURRENT_SOURCE_DIR}/../exports/generated")

# Keep the dependency-free browser client next to the executable so the native
# localhost gateway can serve it regardless of the caller's working directory.
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/web" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

add_library(tatarus_llm_core
    src/planner_types.cpp
    src/http_client_win.cpp
    src/cognitive_json.cpp
    src/bounded_command_validator.cpp
    src/lmstudio_provider.cpp
    src/openai_provider.cpp
    src/gemini_provider.cpp
    src/interactive_environment.cpp
    src/composite_snapshot.cpp
    src/episodic_memory.cpp
    src/tatarus_planner_host.cpp
    src/demo_server_win.cpp
    "${TATARUS_CORE}/nervous_system.cpp"
    "${TATARUS_CORE}/cognitive_bridge.cpp")
target_include_directories(tatarus_llm_core PUBLIC include "${TATARUS_CORE}" "${TATARUS_GENERATED}")
if(WIN32)
    target_link_libraries(tatarus_llm_core PUBLIC winhttp ws2_32)
endif()
if(MSVC)
    target_compile_options(tatarus_llm_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(tatarus_llm_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(tatarus_llm src/main.cpp)
target_link_libraries(tatarus_llm PRIVATE tatarus_llm_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(tatarus_llm_tests tests/test_main.cpp)
    target_link_libraries(tatarus_llm_tests PRIVATE tatarus_llm_core)
    add_test(NAME tatarus_llm_tests COMMAND tatarus_llm_tests)
endif()
