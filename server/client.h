#pragma once

#include <thread>
#include <vector>
#include <string>
#include <string_view>

#include <boost/asio.hpp>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include "monidroid.h"
#include "monidroid/protocol.h"

#include "native.h"

using namespace boost::asio;
using namespace Monidroid;

enum class ClientState {
    New, Identified, Connected, Streaming, ConnectionClosed, Disconnected, Error
};

struct Client {
    static constexpr auto CLIENT_TAG = "Client";

private:
    ip::tcp::socket m_socket;
    std::vector<char> m_netBuffer;
    bool m_sending = false;

    guint id = 0;
    GstElement *appsrc = nullptr;
    guint64 time = 0;

    MonitorMode m_preffered;
    std::string m_modelName;
    ClientState m_state = ClientState::New;

    Monitor m_monitor;

    std::thread m_communicationThread;

    GMainContext *m_context;
    GMainLoop *m_loop;

    void sendFullFrame(const FrameMapInfo& info);
    void sendMonitorOff();
    
    static void mediaConfigure(GstRTSPMediaFactory *factory, GstRTSPMedia *media, Client *self);
    static gboolean busWatch(GstBus * bus, GstMessage * message, Client *self);
    static void needData(GstElement *appsrc, guint length, Client *self);
    static void enoughData(GstElement *appsrc, Client *self);

    static gboolean pushData(Client* client);

public:
    Client(ip::tcp::socket socket);
    ~Client();

    MD_CLASS_PTR_ONLY(Client)

    const std::string& modelName() const;
    ClientState state() const;
    void attachThread(std::thread&& t);
    std::thread detachThread();

    bool identifyClient();
    bool connectMonitor(const Adapter& adapter);
    void sendFrames();
    void disconnectMonitor();

    void forceDisconnect();

    void sendError(ErrorCode code);
    void sendError(const std::string_view msg);
};