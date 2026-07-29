cmake_minimum_required(VERSION 3.20)
project(TATARUS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(KERNEL_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../exports/generated")

add_library(ag_bio_engine bio_core.cpp classifier.cpp)
target_include_directories(ag_bio_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${KERNEL_INCLUDE}")

add_executable(TATARUSResearchUI WIN32 main.cpp)
target_link_libraries(TATARUSResearchUI PRIVATE ag_bio_engine user32 gdi32 comdlg32 shell32 comctl32)
target_compile_definitions(TATARUSResearchUI PRIVATE UNICODE _UNICODE)
set_target_properties(TATARUSResearchUI PROPERTIES OUTPUT_NAME "TATARUS_ResearchUI")

add_executable(AGBioNetworkEngineTests engine_tests.cpp)
target_link_libraries(AGBioNetworkEngineTests PRIVATE ag_bio_engine)

add_executable(AGBioNetworkResearch research_cli.cpp)
target_link_libraries(AGBioNetworkResearch PRIVATE ag_bio_engine)

add_executable(AGBioNetworkAcceptance acceptance_tests.cpp)
target_link_libraries(AGBioNetworkAcceptance PRIVATE ag_bio_engine)

add_executable(AGBioNetworkSuperiority superiority_experiment.cpp)
target_link_libraries(AGBioNetworkSuperiority PRIVATE ag_bio_engine)

add_executable(AGBioNetworkDelayedXor delayed_xor_experiment.cpp)
target_link_libraries(AGBioNetworkDelayedXor PRIVATE ag_bio_engine)

add_executable(AGBioNetworkTraceEssential trace_essential_experiment.cpp)
target_link_libraries(AGBioNetworkTraceEssential PRIVATE ag_bio_engine)

add_library(ag_nervous_system nervous_system.cpp cognitive_bridge.cpp)
target_include_directories(ag_nervous_system PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${KERNEL_INCLUDE}")

add_executable(AGNervousSystemTests nervous_system_tests.cpp)
target_link_libraries(AGNervousSystemTests PRIVATE ag_nervous_system)

add_executable(AGNervousSystemLab nervous_system_lab.cpp)
target_link_libraries(AGNervousSystemLab PRIVATE ag_nervous_system)

add_executable(TATARUS WIN32 nervous_system_ui.cpp)
target_link_libraries(TATARUS PRIVATE ag_nervous_system user32 gdi32 comdlg32 shell32)
target_compile_definitions(TATARUS PRIVATE UNICODE _UNICODE)

add_executable(AGNervousSystemOpenClProbe nervous_system_opencl_probe.cpp)

add_executable(AGRepresentationResearch representation_research.cpp)
target_link_libraries(AGRepresentationResearch PRIVATE ag_nervous_system)

add_executable(AGPersistentAITrial persistent_ai_trial.cpp)
target_link_libraries(AGPersistentAITrial PRIVATE ag_nervous_system)

add_executable(AGStage20To23 stage20_23_pipeline.cpp)
target_link_libraries(AGStage20To23 PRIVATE ag_nervous_system)

if(MSVC)
  foreach(target ag_bio_engine ag_nervous_system TATARUSResearchUI AGBioNetworkEngineTests AGBioNetworkResearch AGBioNetworkAcceptance AGBioNetworkSuperiority AGBioNetworkDelayedXor AGBioNetworkTraceEssential AGNervousSystemTests AGNervousSystemLab TATARUS AGNervousSystemOpenClProbe AGRepresentationResearch AGPersistentAITrial AGStage20To23)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  endforeach()
endif()

enable_testing()
add_test(NAME engine_tests COMMAND AGBioNetworkEngineTests)
add_test(
  NAME research_acceptance
  COMMAND AGBioNetworkAcceptance "${CMAKE_CURRENT_BINARY_DIR}/acceptance_results.json")
add_test(
  NAME persistent_nervous_system
  COMMAND AGNervousSystemTests "${CMAKE_CURRENT_BINARY_DIR}/nervous_system_test_output")
add_test(
  NAME opencl_differential
  COMMAND AGNervousSystemOpenClProbe "${CMAKE_CURRENT_BINARY_DIR}/opencl_probe.json")
