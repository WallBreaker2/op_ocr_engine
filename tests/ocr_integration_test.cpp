#include "ocr_engine.h"
#include "ocr_server.h"

#include <gtest/gtest.h>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using json = nlohmann::json;

std::string get_tessdata_path() {
    char* from_env = nullptr;
    size_t len = 0;
    if (_dupenv_s(&from_env, &len, "OCR_TESSDATA_PATH") == 0 && from_env != nullptr) {
        std::string value(from_env);
        free(from_env);
        if (!value.empty()) {
            return value;
        }
    }

    const std::filesystem::path default_path =
        std::filesystem::current_path().parent_path() / "tessdata";
    return default_path.string();
}

std::string get_language() {
    char* from_env = nullptr;
    size_t len = 0;
    if (_dupenv_s(&from_env, &len, "OCR_LANG") == 0 && from_env != nullptr) {
        std::string value(from_env);
        free(from_env);
        if (!value.empty()) {
            return value;
        }
    }
    return "chi_sim";
}

bool wait_until_healthy(const std::string& host, int port, int timeout_ms = 5000) {
    httplib::Client client(host, port);
    client.set_connection_timeout(1, 0);
    client.set_read_timeout(1, 0);

    const auto start = std::chrono::steady_clock::now();
    while (true) {
        if (const auto res = client.Get("/health")) {
            if (res->status == 200) {
                return true;
            }
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() >= timeout_ms) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

class OcrServerIntegrationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        datapath_ = get_tessdata_path();
        language_ = get_language();

        const std::filesystem::path model = std::filesystem::path(datapath_) / (language_ + ".traineddata");
        if (!std::filesystem::exists(model)) {
            GTEST_SKIP() << "Missing model: " << model.string()
                         << " (set OCR_TESSDATA_PATH / OCR_LANG to override)";
        }

        engine_ = std::make_unique<ocr::OcrEngine>();
        if (!engine_->init(datapath_, language_)) {
            GTEST_SKIP() << "Failed to init OcrEngine with datapath=" << datapath_
                         << " lang=" << language_;
        }

        server_ = std::make_unique<ocr::OcrServer>(*engine_);

        for (int p = 19080; p < 19100; ++p) {
            port_ = p;
            listen_returned_ = false;
            listen_ok_ = false;

            server_thread_ = std::thread([] {
                listen_ok_ = server_->listen(host_, port_);
                listen_returned_ = true;
            });

            if (wait_until_healthy(host_, port_)) {
                return;
            }

            server_->stop();
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
        }

        GTEST_FAIL() << "Failed to start test server on ports 19080-19099";
    }

    static void TearDownTestSuite() {
        if (server_) {
            server_->stop();
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        server_.reset();
        engine_.reset();
    }

    static httplib::Client new_client() {
        httplib::Client client(host_, port_);
        client.set_connection_timeout(2, 0);
        client.set_read_timeout(2, 0);
        return client;
    }

    static inline std::unique_ptr<ocr::OcrEngine> engine_;
    static inline std::unique_ptr<ocr::OcrServer> server_;
    static inline std::thread server_thread_;
    static inline std::atomic<bool> listen_returned_{false};
    static inline std::atomic<bool> listen_ok_{false};
    static inline std::string datapath_;
    static inline std::string language_;
    static inline constexpr const char* host_ = "127.0.0.1";
    static inline int port_ = 19080;
};

TEST_F(OcrServerIntegrationTest, HealthEndpointWorks) {
    auto client = new_client();
    const auto res = client.Get("/health");
    ASSERT_TRUE(res) << "Request failed";
    EXPECT_EQ(res->status, 200);

    const auto body = json::parse(res->body);
    EXPECT_EQ(body.value("status", ""), "ok");
}

TEST_F(OcrServerIntegrationTest, VersionEndpointWorks) {
    auto client = new_client();
    const auto res = client.Get("/api/v1/version");
    ASSERT_TRUE(res) << "Request failed";
    EXPECT_EQ(res->status, 200);

    const auto body = json::parse(res->body);
    ASSERT_TRUE(body.contains("version"));
    EXPECT_FALSE(body["version"].get<std::string>().empty());
}

TEST_F(OcrServerIntegrationTest, OcrEndpointAcceptsValidPayload) {
    auto client = new_client();

    std::vector<uint8_t> image_data(16 * 16, 255);
    std::string image_b64;
    image_b64.reserve(32 * 32);
    static constexpr char kEncodeTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (size_t i = 0; i < image_data.size(); i += 3) {
        int v = image_data[i];
        v = (i + 1 < image_data.size()) ? (v << 8) + image_data[i + 1] : (v << 8);
        v = (i + 2 < image_data.size()) ? (v << 8) + image_data[i + 2] : (v << 8);

        image_b64.push_back(kEncodeTable[(v >> 18) & 0x3F]);
        image_b64.push_back(kEncodeTable[(v >> 12) & 0x3F]);
        image_b64.push_back(i + 1 < image_data.size() ? kEncodeTable[(v >> 6) & 0x3F] : '=');
        image_b64.push_back(i + 2 < image_data.size() ? kEncodeTable[v & 0x3F] : '=');
    }

    json req;
    req["image"] = image_b64;
    req["width"] = 16;
    req["height"] = 16;
    req["bpp"] = 1;

    const auto res = client.Post("/api/v1/ocr", req.dump(), "application/json");
    ASSERT_TRUE(res) << "Request failed";
    EXPECT_EQ(res->status, 200);

    const auto body = json::parse(res->body);
    EXPECT_EQ(body.value("code", -1), 0);
    ASSERT_TRUE(body.contains("results"));
    EXPECT_TRUE(body["results"].is_array());
}

} // namespace
