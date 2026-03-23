#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace ocr {

struct OcrResult {
    int x1, y1, x2, y2;
    std::string text;
    float confidence;
};

class OcrEngine {
public:
    OcrEngine();
    ~OcrEngine();

    // Non-copyable
    OcrEngine(const OcrEngine&) = delete;
    OcrEngine& operator=(const OcrEngine&) = delete;

    /// Initialize Tesseract with the given datapath and language.
    /// @param datapath  Path to tessdata directory
    /// @param language  Language code, e.g. "eng", "chi_sim"
    /// @return true on success
    bool init(const std::string& datapath, const std::string& language);

    /// Perform OCR on raw pixel data.
    /// @param image  Pointer to raw pixel buffer (row-major, top-to-bottom)
    /// @param width  Image width in pixels
    /// @param height Image height in pixels
    /// @param bpp    Bytes per pixel (1=gray, 3=RGB, 4=RGBA)
    /// @return Vector of recognized text regions
    std::vector<OcrResult> recognize(const uint8_t* image, int width, int height, int bpp);

    /// Return engine version string.
    static const char* version();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ocr
