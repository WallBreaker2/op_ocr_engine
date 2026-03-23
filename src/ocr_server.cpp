#include "ocr_server.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <mutex>

// Base64 decoding table
static const unsigned char kBase64Table[256] = {
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64,65,64,64,
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
    64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64
};

static std::vector<uint8_t> base64_decode(const std::string& input) {
    std::vector<uint8_t> out;
    out.reserve(input.size() * 3 / 4);

    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (kBase64Table[c] >= 64) {
            if (c == '=') break;
            continue;
        }
        val = (val << 6) + kBase64Table[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

using json = nlohmann::json;

namespace ocr {

struct OcrServer::Impl {
    OcrEngine& engine;
    httplib::Server svr;
    std::mutex ocr_mutex;  // Tesseract is not thread-safe

    explicit Impl(OcrEngine& eng) : engine(eng) {}

    void setup_routes() {
        // Health check
        svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"({"status":"ok"})", "application/json");
        });

        // Version
        svr.Get("/api/v1/version", [](const httplib::Request&, httplib::Response& res) {
            json j;
            j["version"] = OcrEngine::version();
            res.set_content(j.dump(), "application/json");
        });

        // OCR endpoint
        svr.Post("/api/v1/ocr", [this](const httplib::Request& req, httplib::Response& res) {
            handle_ocr(req, res);
        });
    }

    void handle_ocr(const httplib::Request& req, httplib::Response& res) {
        // Parse JSON body
        json body;
        try {
            body = json::parse(req.body);
        } catch (const json::parse_error& e) {
            json err;
            err["code"] = -1;
            err["error"] = std::string("Invalid JSON: ") + e.what();
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        // Validate required fields
        if (!body.contains("image") || !body.contains("width") ||
            !body.contains("height") || !body.contains("bpp")) {
            json err;
            err["code"] = -1;
            err["error"] = "Missing required fields: image, width, height, bpp";
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        std::string image_b64 = body["image"].get<std::string>();
        int width = body["width"].get<int>();
        int height = body["height"].get<int>();
        int bpp = body["bpp"].get<int>();

        // Validate bpp
        if (bpp != 1 && bpp != 3 && bpp != 4) {
            json err;
            err["code"] = -1;
            err["error"] = "bpp must be 1, 3, or 4";
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        // Decode base64 image
        std::vector<uint8_t> image_data = base64_decode(image_b64);
        size_t expected_size = static_cast<size_t>(width) * height * bpp;
        if (image_data.size() != expected_size) {
            json err;
            err["code"] = -1;
            err["error"] = "Image data size mismatch. Expected " +
                           std::to_string(expected_size) + " bytes, got " +
                           std::to_string(image_data.size());
            res.status = 400;
            res.set_content(err.dump(), "application/json");
            return;
        }

        // Perform OCR (thread-safe via mutex)
        std::vector<OcrResult> results;
        {
            std::lock_guard<std::mutex> lock(ocr_mutex);
            results = engine.recognize(image_data.data(), width, height, bpp);
        }

        // Build response
        json response;
        response["code"] = 0;
        response["results"] = json::array();
        for (const auto& r : results) {
            json item;
            item["text"] = r.text;
            item["bbox"] = {r.x1, r.y1, r.x2, r.y2};
            item["confidence"] = r.confidence;
            response["results"].push_back(item);
        }

        res.set_content(response.dump(), "application/json");
    }
};

OcrServer::OcrServer(OcrEngine& engine)
    : impl_(std::make_unique<Impl>(engine)) {
    impl_->setup_routes();
}

OcrServer::~OcrServer() {
    stop();
}

bool OcrServer::listen(const std::string& host, int port) {
    std::cout << "[OcrServer] Listening on " << host << ":" << port << std::endl;
    return impl_->svr.listen(host, port);
}

void OcrServer::stop() {
    if (impl_) {
        impl_->svr.stop();
    }
}

} // namespace ocr
