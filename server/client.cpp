#include "client.h"

#include <cstring>
#include <array>
#include <memory>

#include <turbojpeg.h>
#include <gst/video/gstvideometa.h>

#include "monidroid/logger.h"
#include "monidroid/edid.h"
#include "monidroid/debug.h"

Client::Client(ip::tcp::socket socket)
  : m_socket(std::move(socket)),
    m_preffered()
{
    m_netBuffer.reserve(7u + 4 + 12);
}

Client::~Client() {
    boost::system::error_code ec;
    m_socket.close(ec);
}

const std::string &Client::modelName() const {
    return m_modelName;
}

ClientState Client::state() const
{
    return m_state;
}

void Client::attachThread(std::thread &&t) {
    m_communicationThread = std::move(t);
}

std::thread Client::detachThread() {
    std::thread t = std::move(m_communicationThread);
    return t;
}

bool Client::identifyClient() {
    enum class WelcomeStates { Welcome, Model, Modes };

    char recvBuf[256];
    int bytesNeeded = 0;
    WelcomeStates state = WelcomeStates::Welcome;

    boost::system::error_code ec;

    std::string_view welcomeWord(Monidroid::WELCOME_WORD);
    bytesNeeded = welcomeWord.size() + sizeof(int);
    
    while (bytesNeeded > 0) {
        size_t bytesReceived = m_socket.read_some(boost::asio::buffer(recvBuf, bytesNeeded), ec);
        if (ec) {
            Monidroid::DefaultLog("Socket error \"{}\", identification failed", ec.message());
            return false;
        }
        m_netBuffer.insert(m_netBuffer.end(), recvBuf, recvBuf + bytesReceived);
        if (m_netBuffer.size() < bytesNeeded) continue;

        switch (state) {
        case WelcomeStates::Welcome:
        {
            std::string_view word(m_netBuffer.data(), welcomeWord.size());
            if (word != welcomeWord) {
                Monidroid::DefaultLog("WELCOME word mismatch, unknown Monidroid Client");
                return false;
            }

            int nameLength = *(reinterpret_cast<int*>(m_netBuffer.data() + welcomeWord.size()));
            
            m_netBuffer.erase(m_netBuffer.begin(), m_netBuffer.begin() + bytesNeeded);
            bytesNeeded = nameLength;
            state = WelcomeStates::Model;
            break;
        }
        case WelcomeStates::Model:
        {
            m_modelName = std::string(m_netBuffer.data(), bytesNeeded);

            m_netBuffer.erase(m_netBuffer.begin(), m_netBuffer.begin() + bytesNeeded);
            bytesNeeded = 12;
            state = WelcomeStates::Modes;
            break;
        }
        case WelcomeStates::Modes:
        {
            int *settings = reinterpret_cast<int*>(m_netBuffer.data());
            // Get multiple of 2
            unsigned int width = settings[0] & ~0x01;
            unsigned int height = settings[1] & ~0x01;
            unsigned int refreshRate = settings[2];
            if (width < height) {
                std::swap(width, height);
            }
            m_preffered = {
                .width = width,
                .height = height,
                .refreshRate = refreshRate
            };

            Monidroid::DefaultLog("New client identified as \"{}\", preferred mode: {}x{}@{}", m_modelName, width, height, refreshRate);
            m_state = ClientState::Identified;
            
            m_netBuffer.erase(m_netBuffer.begin(), m_netBuffer.begin() + bytesNeeded);
            bytesNeeded = 0;
        }
        }
    }

    return true;
}

bool Client::connectMonitor(const Adapter &adapter) {
    m_monitor = adapterConnectMonitor(adapter, m_modelName, m_preffered);
    if (!m_monitor) {
        m_state = ClientState::Error;
        return false;
    }

    m_state = ClientState::Connected;

    return true;
}

void Client::sendFrames() {
    // Diagnostics
    int frameFails = 0;
    int mapFails = 0;

    m_state = ClientState::Streaming;

    FrameMapInfo info;
    unsigned int dataPixSize;

    while (m_state == ClientState::Streaming) {
        MDStatus status = monitorRequestFrame(m_monitor);
        switch (status) {
        case MDStatus::ModeChanged:
        case MDStatus::FrameReady:
            frameFails = 0;

            monitorMapCurrent(m_monitor, info);
            if (info.data != nullptr) {
                mapFails = 0;
                sendFullFrame(info);
                monitorUnmap(m_monitor);
            } else {
                ++mapFails;
                if (mapFails >= 10) {
                    Monidroid::TaggedLog(m_modelName, "Too many map() fails, stopping sending...");
                    sendError(ErrorCode::TooManyFails);
                    m_state = ClientState::Error;
                }
            }
            break;
        case MDStatus::NoUpdates:
            frameFails = 0;

            break;
        case MDStatus::MonitorOff:
            frameFails = 0;

            sendMonitorOff();
            break;
        default:
            // Error branch
            ++frameFails;
            if (frameFails >= 15) {
                Monidroid::TaggedLog(m_modelName, "Too many failed frame requests, stopping sending...");
                sendError(ErrorCode::TooManyFails);
                m_state = ClientState::Error;
            }
            break;
        }
    }
}


void Client::sendFrames2() {
    m_context = g_main_context_new();
    m_loop = g_main_loop_new(m_context, FALSE);
    
    GstRTSPServer *server = gst_rtsp_server_new();
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
    
    gst_rtsp_media_factory_set_launch(factory,
        "( appsrc name=mdsrc ! videoconvert ! video/x-raw,format=I420 ! x264enc ! rtph264pay name=pay0 pt=96 )");
        
    g_signal_connect(factory, "media-configure", (GCallback)mediaConfigure, this);
    
    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);
    gst_rtsp_mount_points_add_factory(mounts, "/stream", factory);
    g_object_unref(mounts);
    
    gst_rtsp_server_attach(server, m_context);
    g_main_loop_run(m_loop);
}
    
void Client::mediaConfigure(GstRTSPMediaFactory *factory, GstRTSPMedia *media, Client *self) {
    GstElement *element = gst_rtsp_media_get_element(media);
    GstElement *appsrc = gst_bin_get_by_name_recurse_up(GST_BIN(element), "mdsrc");
    
    MonitorMode mode = monitorRequestMode(self->m_monitor, true);
    // Configure the caps of the video
    gst_util_set_object_arg(G_OBJECT(appsrc), "format", "time");
    g_object_set(G_OBJECT(appsrc),
        "caps", gst_caps_new_simple("video/x-raw",
            "format", G_TYPE_STRING, "BGRA",
            "width", G_TYPE_INT, mode.width, // TODO: proper width
            "height", G_TYPE_INT, mode.height, // TODO: proper height
            "framerate", GST_TYPE_FRACTION, 0, 1,
        NULL),
    NULL);
    
    g_signal_connect(appsrc, "need-data", (GCallback)needData, self);
    g_signal_connect(appsrc, "enough-data", (GCallback)enoughData, self);
    
    self->appsrc = appsrc;

    GstBus *bus = gst_element_get_bus(element);
    guint sourceid = gst_bus_add_watch(bus, (GstBusFunc)busWatch, self);
    
    gst_object_unref(bus);
    gst_object_unref(appsrc);
    gst_object_unref(element);
}

gboolean Client::busWatch(GstBus *bus, GstMessage *message, Client *self) {
    GstMessageType type = GST_MESSAGE_TYPE(message);
    if (type == GST_MESSAGE_EOS || type == GST_MESSAGE_ERROR) {
        Monidroid::TaggedLog(self->m_modelName, "Got message");

        g_main_loop_quit(self->m_loop);
        g_main_loop_unref(self->m_loop);
        g_main_context_unref(self->m_context);

        return FALSE;
    } else {
        return TRUE;
    }
}

void Client::needData(GstElement *appsrc, guint length, Client *self) {
    if (self->id == 0) {
        self->id = g_idle_add((GSourceFunc)Client::pushData, self);
    }
}

void Client::enoughData(GstElement *appsrc, Client *self) {
    if (self->id != 0) {
        g_source_remove(self->id);
        self->id = 0;
    }
}

gboolean Client::pushData(Client *client) {
    MDStatus status = monitorRequestFrame(client->m_monitor);
    if (status != MDStatus::FrameReady) {
        return FALSE;
    }

    FrameMapInfo mapInfo;
    monitorMapCurrent(client->m_monitor, mapInfo);

    GstVideoInfo videoInfo;
    gst_video_info_set_format(&videoInfo,
        GST_VIDEO_FORMAT_BGRA, mapInfo.width, mapInfo.height
    );
    videoInfo.stride[0] = mapInfo.stride;

    GstBuffer *buffer = gst_buffer_new_wrapped_full(
        GST_MEMORY_FLAG_READONLY,
        mapInfo.data,
        mapInfo.height * mapInfo.width * 4,
        0,
        mapInfo.height * mapInfo.width * 4,
        client,
        (GDestroyNotify)[](gpointer client) {
            monitorUnmap(static_cast<Client*>(client)->m_monitor);
        }
    );
    
    gst_buffer_add_video_meta_full(buffer,
        GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT(&videoInfo),
        GST_VIDEO_INFO_WIDTH(&videoInfo),
        GST_VIDEO_INFO_HEIGHT(&videoInfo),
        GST_VIDEO_INFO_N_PLANES(&videoInfo),
        videoInfo.offset,
        videoInfo.stride
    );

    GST_BUFFER_PTS(buffer) = client->time;
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 60); // TODO
    client->time += GST_BUFFER_DURATION(buffer);

    GstFlowReturn ret;
    g_signal_emit_by_name(client->appsrc, "push-buffer", buffer, &ret);

    return ret == GST_FLOW_OK;
}

void Client::sendFullFrame(const FrameMapInfo& info) {
    tjhandle tj = tj3Init(TJINIT_COMPRESS);
    unsigned char *jpegData = static_cast<unsigned char*>(tj3Alloc(1));
    size_t _jpegsize = 0;
    
    tj3Set(tj, TJPARAM_QUALITY, 50);
    tj3Set(tj, TJPARAM_SUBSAMP, TJSAMP_422);
    
    int code = tj3Compress8(tj,
        reinterpret_cast<const uint8_t*>(info.data),
        info.width, info.stride, info.height, TJPF_BGRA,
        &jpegData, &_jpegsize
    );
    
    if (code != 0) {
        Monidroid::DefaultLog(
            "Frame compression failed with code {}, message: \"{}\"", tj3GetErrorCode(tj), tj3GetErrorStr(tj)
        );
        return;
    }
    
    int jpegSize = _jpegsize;
    
    std::array<boost::asio::const_buffer, 3> buffers {
        boost::asio::buffer(std::string_view(Monidroid::FRAME_WORD)),
        boost::asio::buffer((void*)&jpegSize, sizeof(jpegSize)),
        boost::asio::buffer(jpegData, jpegSize),
    };
    
    boost::system::error_code ec;
    boost::asio::write(m_socket, buffers, ec);
    if (ec) {
        m_sending = false;
        std::cout << ec.message() << "\n";
        // alternative
        m_state = ClientState::ConnectionClosed;
    }

    tj3Free(jpegData);
    tj3Destroy(tj);
}


void Client::sendMonitorOff() {
    std::string_view frameWord(Monidroid::FRAME_WORD);
    
    int jpegSize = 0;
    size_t bufSize = frameWord.size() + sizeof(jpegSize);
    auto sendBuffer = std::make_unique<char[]>(bufSize);
    
    std::memcpy(sendBuffer.get(), frameWord.data(), frameWord.size());
    std::memcpy(sendBuffer.get() + frameWord.size(), (void*)&jpegSize, sizeof(jpegSize));
    
    boost::system::error_code ec;
    boost::asio::write(m_socket, boost::asio::buffer(sendBuffer.get(), bufSize), ec);
    if (ec) {
        m_sending = false;
        std::cout << ec.message() << "\n";
        // alternative
        m_state = ClientState::ConnectionClosed;
    }
}

void Client::disconnectMonitor() {
    // TODO Windows: process 1291 () error code
    monitorDisconnect(m_monitor);
    m_state = ClientState::Disconnected;
}

void Client::forceDisconnect() {
    sendError(ErrorCode::DisconnectedByServer);

    m_state = ClientState::ConnectionClosed;
}

void Client::sendError(ErrorCode code) {
    std::array<boost::asio::const_buffer, 2> buffers = {
        boost::asio::buffer(std::string_view(Monidroid::ERROR_WORD)),
        boost::asio::buffer((void*)&code, sizeof(code))
    };

    boost::system::error_code ec;
    boost::asio::write(m_socket, buffers, ec);
    if (ec) std::cout << ec.message() << "\n";
    m_socket.shutdown(ip::tcp::socket::shutdown_both, ec);
}

void Client::sendError(const std::string_view msg) {
    std::string_view errorWord(Monidroid::ERROR_WORD);
    ErrorCode code = ErrorCode::MessageEncoded;
    int len = msg.size();

    std::array<boost::asio::const_buffer, 4> buffers {
        boost::asio::buffer(std::string_view(Monidroid::ERROR_WORD)),
        boost::asio::buffer((void*)&code, sizeof(code)),
        boost::asio::buffer((void*)&len, sizeof(len)),
        boost::asio::buffer(msg),
    };

    boost::system::error_code ec;
    boost::asio::write(m_socket, buffers, ec);
    if (ec) std::cout << ec.message() << "\n";
    m_socket.shutdown(ip::tcp::socket::shutdown_both, ec);
}
