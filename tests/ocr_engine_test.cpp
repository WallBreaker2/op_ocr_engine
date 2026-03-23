#include "ocr_engine.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

TEST(OcrEngineTest, VersionStringIsExposed) {
    const std::string version = ocr::OcrEngine::version();
    EXPECT_FALSE(version.empty());
    EXPECT_NE(version.find("tesseract"), std::string::npos);
}

TEST(OcrEngineTest, RecognizeWithoutInitReturnsEmptyResults) {
    ocr::OcrEngine engine;

    const int width = 4;
    const int height = 4;
    const int bpp = 1;
    std::vector<uint8_t> image(width * height * bpp, 255);

    const auto results = engine.recognize(image.data(), width, height, bpp);
    EXPECT_TRUE(results.empty());
}
