# CMake minimum version (FetchContent requires 3.14+)
cmake_minimum_required(VERSION 3.14)

project(ocr_service LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)

# ─── Dependencies ─────────────────────────────────────────────
include(FetchContent)

# cpp-httplib (header-only HTTP server)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.3
)

# nlohmann/json (header-only JSON library)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)

FetchContent_MakeAvailable(httplib json)

# Tesseract OCR
find_package(Tesseract REQUIRED)

# ─── OCR Service Executable ──────────────────────────────────
add_executable(ocr_server
    src/main.cpp
    src/ocr_engine.cpp
    src/ocr_server.cpp
)

target_include_directories(ocr_server PRIVATE src)

target_link_libraries(ocr_server PRIVATE
    Tesseract::libtesseract
    httplib::httplib
    nlohmann_json::nlohmann_json
)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)

    add_executable(ocr_unit_tests
        tests/ocr_engine_test.cpp
        src/ocr_engine.cpp
    )

    target_include_directories(ocr_unit_tests PRIVATE src)

    target_link_libraries(ocr_unit_tests PRIVATE
        Tesseract::libtesseract
        GTest::gtest
        GTest::gtest_main
    )

    add_executable(ocr_integration_tests
        tests/ocr_integration_test.cpp
        src/ocr_engine.cpp
        src/ocr_server.cpp
    )

    target_include_directories(ocr_integration_tests PRIVATE src)

    target_link_libraries(ocr_integration_tests PRIVATE
        Tesseract::libtesseract
        httplib::httplib
        nlohmann_json::nlohmann_json
        GTest::gtest
        GTest::gtest_main
    )

    gtest_discover_tests(ocr_unit_tests)
    gtest_discover_tests(ocr_integration_tests)
endif()

# ─── Legacy DLL target (optional, disabled by default) ───────
option(BUILD_LEGACY_DLL "Build the legacy tess_engine DLL" OFF)
if(BUILD_LEGACY_DLL)
    add_library(tess_engine SHARED src/tess_ocr.cpp src/tess_engine.def)
    target_include_directories(tess_engine PRIVATE include)
    target_link_libraries(tess_engine PRIVATE Tesseract::libtesseract)
    target_compile_definitions(tess_engine PRIVATE OCR_LIB_EXPORT)

    add_executable(test_legacy src/test.cpp)
    target_include_directories(test_legacy PRIVATE include)
    target_link_libraries(test_legacy PRIVATE tess_engine)
endif()
