#include "ocr_engine.h"
#include "ocr_server.h"

#include <iostream>
#include <string>
#include <csignal>
#include <memory>

static std::unique_ptr<ocr::OcrServer> g_server;

static void signal_handler(int sig) {
    std::cout << "\n[main] Received signal " << sig << ", shutting down..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --datapath <path>   Path to tessdata directory (required)\n"
              << "  --lang <language>   OCR language, e.g. eng, chi_sim (default: eng)\n"
              << "  --port <port>       HTTP listen port (default: 8080)\n"
              << "  --host <host>       HTTP listen host (default: 0.0.0.0)\n"
              << "  --help              Show this help message\n";
}

int main(int argc, char* argv[]) {
    std::string datapath;
    std::string lang = "eng";
    std::string host = "0.0.0.0";
    int port = 8080;

    // Simple argument parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--datapath" && i + 1 < argc) {
            datapath = argv[++i];
        } else if (arg == "--lang" && i + 1 < argc) {
            lang = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    if (datapath.empty()) {
        std::cerr << "Error: --datapath is required\n" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    // Initialize OCR engine
    ocr::OcrEngine engine;
    if (!engine.init(datapath, lang)) {
        std::cerr << "Failed to initialize OCR engine" << std::endl;
        return 1;
    }

    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Create and start HTTP server
    g_server = std::make_unique<ocr::OcrServer>(engine);

    std::cout << "=== OCR HTTP Service ===" << std::endl;
    std::cout << "Version: " << ocr::OcrEngine::version() << std::endl;
    std::cout << "Endpoints:" << std::endl;
    std::cout << "  GET  /health          - Health check" << std::endl;
    std::cout << "  GET  /api/v1/version  - Engine version" << std::endl;
    std::cout << "  POST /api/v1/ocr      - Perform OCR" << std::endl;
    std::cout << "========================" << std::endl;

    if (!g_server->listen(host, port)) {
        std::cerr << "Failed to start server on " << host << ":" << port << std::endl;
        return 1;
    }

    return 0;
}
