#pragma once

#ifdef LIBUS_NO_SSL
constexpr bool SSL = false;
#else
constexpr bool SSL = true;
#endif

#include <functional>
#include <unordered_map>

#include <App.h>
#include <nlohmann/json.hpp>

#include "monidroid/logger.h"

using namespace uWS;
using namespace nlohmann;

struct ControlContext { };

class MDApp : public uWS::App {
public:
    using MessageHandler = std::function<void(const json &msg)>;
    using RequestHandler = std::function<json(const json &data, std::string &error)>;

    MDApp();

    void onEvent(std::string_view message, MessageHandler handler);
    void onRequest(std::string_view message, RequestHandler handler);
    void emit(std::string_view message, std::string_view objKey, const json &obj);

private:
    static constexpr auto TOPIC_NAME = "main";

    static constexpr auto MSG_KEY = "message";
    static constexpr auto REQ_ID_KEY = "requestId";
    static constexpr auto DATA_KEY = "data";
    static constexpr auto ERROR_KEY = "error";

    Loop *m_loop;
    std::unordered_map<std::string, MessageHandler> m_handlers;
    std::unordered_map<std::string, RequestHandler> m_requestHandlers;
};

MDApp::MDApp()
  : m_loop(Loop::get())
{
    ws<ControlContext>("/", {
        .open = [](WebSocket<SSL, true, ControlContext> *ws) {
            ws->subscribe(TOPIC_NAME);
        },

        .message = [this](WebSocket<SSL, true, ControlContext> *ws, std::string_view message, uWS::OpCode opCode) {
            json obj = json::parse(message, nullptr, false);
            if (obj.is_discarded()) {
                Monidroid::DefaultLog("Received unsupported message: \"{}\"", message);
                return;
            } else if (!obj.contains(MSG_KEY)) {
                Monidroid::DefaultLog("Property \"message\" not found in message {}", message);
                return;
            }

            const json::string_t &msg = obj[MSG_KEY].get_ref<const json::string_t&>();
            if (obj.contains(REQ_ID_KEY) && m_requestHandlers.contains(msg)) {
                std::string error;
                
                json data = m_requestHandlers[msg](obj.value(DATA_KEY, json()), error);

                json res;
                res[REQ_ID_KEY] = obj[REQ_ID_KEY];
                if (error.empty()) res[DATA_KEY] = data;
                else res[ERROR_KEY] = error;

                ws->send(res.dump(), OpCode::TEXT);                
            } else if (m_handlers.contains(msg)) {
                m_handlers[msg](obj);
            } else {
                Monidroid::DefaultLog("Unknown message \"{}\"", msg);
            }
        },

        .close = [](WebSocket<SSL, true, ControlContext> *ws, int code, std::string_view message) {
            ws->unsubscribe(TOPIC_NAME);
        }
    });

    listen(Monidroid::Control::PORT, [](us_listen_socket_t *s) {
        if (s) {
            Monidroid::DefaultLog("Control panel interface is listening on port {}", Monidroid::Control::PORT);
        } else {
            throw std::runtime_error("Failed to open control panel interface");
        }
    });
}

inline void MDApp::onEvent(std::string_view message, MessageHandler handler) {
    m_handlers[std::string(message)] = handler;
}

inline void MDApp::onRequest(std::string_view message, RequestHandler handler) {
    m_requestHandlers[std::string(message)] = handler;
}

inline void MDApp::emit(std::string_view message, std::string_view objKey, const json &obj) {
    json j;
    j[MSG_KEY] = message;
    if (!objKey.empty()) {
        j[objKey] = obj;
    }

    m_loop->defer([this, j]() {
        publish(TOPIC_NAME, j.dump(), OpCode::TEXT);
    });
}
