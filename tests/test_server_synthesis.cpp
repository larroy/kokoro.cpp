// End-to-end HTTP checks against the real model.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "http_routes.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <memory>
#include <algorithm>
#include <thread>

namespace {
std::unique_ptr<kokoro_server::SpeechService> g_service;
std::unique_ptr<httplib::Server> g_server;
std::thread g_thread;
int g_port = -1;

void stop_server() {
    if (g_server) g_server->stop();
    if (g_thread.joinable()) g_thread.join();
}
}  // namespace

TEST_CASE("server synthesizes audio") {
    httplib::Client client("127.0.0.1", g_port);
    const auto wav = client.Post("/v1/audio/speech", R"({"input":"Hello world"})", "application/json");
    REQUIRE(wav);
    CHECK(wav->status == 200);
    CHECK(wav->get_header_value("Content-Type") == "audio/wav");
    CHECK(wav->body.size() > 44);
    CHECK(wav->body.substr(0, 4) == "RIFF");

    const auto pcm = client.Post("/v1/audio/speech", R"({"input":"Hello world","response_format":"pcm"})",
                                 "application/json");
    REQUIRE(pcm);
    CHECK(pcm->status == 200);
    CHECK(pcm->get_header_value("Content-Type") == "audio/pcm");
    CHECK(!pcm->body.empty());
    CHECK(pcm->body.size() % 2 == 0);

    const auto voice = client.Post("/v1/audio/speech", R"({"input":"Hello world","voice":"nope"})",
                                   "application/json");
    REQUIRE(voice);
    CHECK(voice->status == 400);
    CHECK(nlohmann::json::parse(voice->body)["error"]["param"] == "voice");

    const auto mp3 = client.Post("/v1/audio/speech", R"({"input":"Hello world","response_format":"mp3"})",
                                 "application/json");
    REQUIRE(mp3);
    CHECK(mp3->status == 400);

    const auto voices = client.Get("/v1/audio/voices");
    REQUIRE(voices);
    CHECK(voices->status == 200);
    const auto listed = nlohmann::json::parse(voices->body)["voices"];
    CHECK(std::find(listed.begin(), listed.end(), "af_maple") != listed.end());
}

int main(int argc, char** argv) {
    if (argc < 4 || !std::filesystem::exists(argv[1]) || !std::filesystem::exists(argv[2])) return 77;
    kokoro_ctx* ctx = nullptr;
    if (kokoro_create(argv[1], argv[2], argv[3], &ctx) != KOKORO_OK) return 1;
    g_service = std::make_unique<kokoro_server::SpeechService>(ctx);
    g_server = std::make_unique<httplib::Server>();
    kokoro_server::register_routes(*g_server, *g_service, "af_maple");
    g_port = g_server->bind_to_any_port("127.0.0.1");
    if (g_port < 0) return 1;
    g_thread = std::thread([] { g_server->listen_after_bind(); });
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    const int result = context.run();
    stop_server();
    return result;
}
