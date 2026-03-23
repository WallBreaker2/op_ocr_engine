#include "ocr_engine.h"

#include <tesseract/baseapi.h>
#include <iostream>

namespace ocr {

struct OcrEngine::Impl {
    tesseract::TessBaseAPI api;
    bool initialized = false;
};

OcrEngine::OcrEngine() : impl_(std::make_unique<Impl>()) {}

OcrEngine::~OcrEngine() {
    if (impl_ && impl_->initialized) {
        impl_->api.End();
    }
}

const char* OcrEngine::version() {
    return "tesseract-ocr-service 1.0 (Tesseract 5.x)";
}

bool OcrEngine::init(const std::string& datapath, const std::string& language) {
    if (impl_->api.Init(datapath.c_str(), language.c_str())) {
        std::cerr << "[OcrEngine] Failed to initialize Tesseract with datapath="
                  << datapath << " lang=" << language << std::endl;
        return false;
    }
    impl_->initialized = true;
    std::cout << "[OcrEngine] Initialized: datapath=" << datapath
              << " lang=" << language << std::endl;
    return true;
}

std::vector<OcrResult> OcrEngine::recognize(const uint8_t* image, int width, int height, int bpp) {
    std::vector<OcrResult> results;

    if (!impl_->initialized) {
        std::cerr << "[OcrEngine] Engine not initialized" << std::endl;
        return results;
    }

    impl_->api.SetImage(image, width, height, bpp, width * bpp);
    impl_->api.Recognize(nullptr);

    tesseract::ResultIterator* ri = impl_->api.GetIterator();
    if (ri == nullptr) {
        return results;
    }

    tesseract::PageIteratorLevel level = tesseract::RIL_WORD;
    do {
        const char* word = ri->GetUTF8Text(level);
        if (word == nullptr) {
            continue;
        }

        OcrResult r;
        ri->BoundingBox(level, &r.x1, &r.y1, &r.x2, &r.y2);
        r.text = word;
        r.confidence = ri->Confidence(level);

        delete[] word;
        results.push_back(std::move(r));
    } while (ri->Next(level));

    // Clear the image to allow reuse
    impl_->api.Clear();

    return results;
}

} // namespace ocr
