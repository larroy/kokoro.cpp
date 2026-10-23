// HTTP routes of kokoro-server: POST /v1/audio/speech, GET /v1/audio/voices, GET /health.
#ifndef KOKORO_SERVER_HTTP_ROUTES_H
#define KOKORO_SERVER_HTTP_ROUTES_H

#include "speech_service.h"

#include <httplib.h>

#include <string>

namespace kokoro_server {

// service must outlive server.
void register_routes(httplib::Server& server, SpeechService& service, std::string default_voice);

}  // namespace kokoro_server

#endif  // KOKORO_SERVER_HTTP_ROUTES_H
