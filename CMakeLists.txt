# 1.cmake version
cmake_minimum_required(VERSION 3.0.0)

project(tess_engine)

aux_source_directory(src SRC_FILES)

include_directories(include)
add_library(tess_engine SHARED ${SRC_FILES} "./src/tess_engine.def")
add_executable(test "./src/test.cpp")
## for tesseract lib
find_package(Tesseract)
include_directories(${Tesseract_INCLUDE_DIRS})
target_link_libraries(tess_engine Tesseract::libtesseract)
target_link_libraries(test tess_engine)


target_compile_definitions(tess_engine PRIVATE OCR_LIB_EXPORT)