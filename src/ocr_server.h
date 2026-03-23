#pragma once

#include "ocr_engine.h"

#include <string>
#include <memory>
#include <functional>

namespace ocr {

class OcrServer {
public:
    /// Construct the server with a shared OcrEngine reference.
    /// The engine must outlive the server.
    explicit OcrServer(OcrEngine& engine);
    ~OcrServer();

    // Non-copyable
    OcrServer(const OcrServer&) = delete;
    OcrServer& operator=(const OcrServer&) = delete;

    /// Start listening on the given host and port (blocking).
    /// @return true if server started successfully
    bool listen(const std::string& host, int port);

    /// Stop the server gracefully.
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ocr
