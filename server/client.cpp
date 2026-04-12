#include "client.h"

#include <string_view>
#include <cstring>
#include <array>
#include <memory>

#include <turbojpeg.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include "monidroid/logger.h"
#include "monidroid/edid.h"

Client::Client(ip::tcp::socket socket)
  : m_socket(std::move(socket)),
    m_preffered()
{
    m_netBuffer.reserve(7u + 4 + 12);
}

Client::~Client() {
    boost::system::error_code ec;
    m_socket.shutdown(m_socket.shutdown_both, ec);
    m_socket.close();
}

ClientState Client::state() const {
    return m_state;
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
        size_t bytesReceived = m_socket.read_some(boost::asio::buffer(recvBuf, bytesNeeded));
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
    // // Fine device number (X from /dev/dri/card[X])
    // evdi_add_device();
    // m_devNumber = 0;
    // while (evdi_check_device(m_devNumber) != AVAILABLE) m_devNumber++;

    // // Prepare EDID
    // Monidroid::EDID edid = Monidroid::CUSTOM_EDID;
    // edid.setDefaultMode(m_preffered.width, m_preffered.height, m_preffered.refreshRate);
    // edid.commit();

    // // Connect monitor
    // m_handle = evdi_open(m_devNumber);
    // evdi_connect2(m_handle,
    //     reinterpret_cast<const unsigned char *>(&edid), sizeof(edid),
    //     m_preffered.width * m_preffered.height,
    //     edid.dataBlocks[0].timing.pixel_clock * 10000
    // );

    // // Set up buffer
    // allocFrameBuffer(m_preffered.width, m_preffered.height);

    m_state = ClientState::Connected;

    return true;
}

void Client::sendFrames() {
    // struct evdi_event_context ctx = {
    //     .dpms_handler = dpmsHandler,
    //     .mode_changed_handler = modeHandler,
    //     .update_ready_handler = updateHandler,
    //     .user_data = this
    // };

    // m_sending = true;
    // while (m_sending) {
    //     bool ready = evdi_request_update(m_handle, m_frameBufferInfo.id);
    //     if (ready) {
    //         grabAndSend(m_frameBufferInfo.id);
    //     } else {
    //         evdi_handle_events(m_handle, &ctx);
    //     }
    // }

    GMainLoop *loop;
    GstRTSPServer *server;
    GstRTSPMountPoints *mounts;
    GstRTSPMediaFactory *factory;

    loop = g_main_loop_new(NULL, FALSE);
    server = gst_rtsp_server_new();

    factory = gst_rtsp_media_factory_new();

    GstElement *bin = gst_bin_new("bin");
    GstElement *src = gst_element_factory_make("appsrc", "mdsrc");
    GstElement *x264enc = gst_element_factory_make("x264enc", NULL);
    GstElement *rtp = gst_element_factory_make("rtph264pay", NULL);

    g_object_set(rtp, "pt", 96, NULL);

    gst_rtsp_media_factory_set_launch(factory, "( videotestsrc ! x264enc ! rtph264pay name=pay0 pt=96 )");
    mounts = gst_rtsp_server_get_mount_points(server);
    gst_rtsp_mount_points_add_factory(mounts, "/stream", factory);
    g_object_unref(mounts);

    gst_rtsp_server_attach(server, NULL);

    g_main_loop_run(loop);
}

void Client::allocFrameBuffer(int width, int height) {
    m_frameBuffer.reserve(width * height);

    m_frameBufferInfo = {
        .id = 0,
        .buffer = m_frameBuffer.data(),
        .width = width,
        .height = height,
        .stride = width * (int)sizeof(ColorType),
        // .rects = rects.data(),       // rects and rect_count are not used 
        // .rect_count = rects.size()   // in current libevdi implementation
    };
    evdi_register_buffer(m_handle, m_frameBufferInfo);
}

void Client::initPipeline() {
    
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

    std::string_view frameWord(Monidroid::FRAME_WORD);

    size_t bufSize = frameWord.size() + sizeof(jpegSize) + jpegSize;
    auto sendBuffer = std::make_unique<char[]>(bufSize);

    std::memcpy(sendBuffer.get(), frameWord.data(), frameWord.size());
    std::memcpy(sendBuffer.get() + frameWord.size(), (void*)&jpegSize, sizeof(jpegSize));
    std::memcpy(sendBuffer.get() + frameWord.size() + sizeof(jpegSize), jpegData, jpegSize);

    boost::system::error_code ec;
    size_t bytesSent = m_socket.write_some(boost::asio::buffer(sendBuffer.get(), bufSize), ec);
    if (ec) {
        m_sending = false;
        // alternative
        m_state = ClientState::ConnectionClosed;
    }

    tj3Free(jpegData);
    tj3Destroy(tj);
}

int Client::grabAndSend(int bufferId)
{
    std::array<evdi_rect, 16> rects {};
    int num_rects = 0;

    evdi_grab_pixels(m_handle, rects.data(), &num_rects);
    
    if (num_rects > 0) {
        tjhandle tj = tj3Init(TJINIT_COMPRESS);
        unsigned char *jpegData = static_cast<unsigned char*>(tj3Alloc(1));
        size_t _jpegsize = 0;

        tj3Set(tj, TJPARAM_QUALITY, 50);
        tj3Set(tj, TJPARAM_SUBSAMP, TJSAMP_422);
        
        int code = tj3Compress8(tj,
            reinterpret_cast<const unsigned char*>(m_frameBuffer.data()),
            m_preffered.width, 0, m_preffered.height, TJPF_RGBA,
            &jpegData, &_jpegsize
        );

        if (code != 0) {
            Monidroid::DefaultLog(
                "Frame compression failed with code {}, message: \"{}\"", tj3GetErrorCode(tj), tj3GetErrorStr(tj)
            );
        }

        int jpegSize = _jpegsize;

        std::string_view frameWord(Monidroid::FRAME_WORD);

        size_t bufSize = frameWord.size() + sizeof(jpegSize) + jpegSize;
        auto sendBuffer = std::make_unique<char[]>(bufSize);

        std::memcpy(sendBuffer.get(), frameWord.data(), frameWord.size());
        std::memcpy(sendBuffer.get() + frameWord.size(), (void*)&jpegSize, sizeof(jpegSize));
        std::memcpy(sendBuffer.get() + frameWord.size() + sizeof(jpegSize), jpegData, jpegSize);

        boost::system::error_code ec;
        size_t bytesSent = m_socket.write_some(boost::asio::buffer(sendBuffer.get(), bufSize), ec);
        if (ec) {
            m_sending = false;
        }

        tj3Free(jpegData);
        tj3Destroy(tj);
    }
    return 0;
}

void Client::sendMonitorOff() {
    std::string_view frameWord(Monidroid::FRAME_WORD);

    int jpegSize = 0;
    size_t bufSize = frameWord.size() + sizeof(jpegSize);
    auto sendBuffer = std::make_unique<char[]>(bufSize);

    std::memcpy(sendBuffer.get(), frameWord.data(), frameWord.size());
    std::memcpy(sendBuffer.get() + frameWord.size(), (void*)&jpegSize, sizeof(jpegSize));

    boost::system::error_code ec;
    size_t bytesSent = m_socket.write_some(boost::asio::buffer(sendBuffer.get(), bufSize), ec);
    if (ec) {
        m_sending = false;
        // alternative
        m_state = ClientState::ConnectionClosed;
    }
}

void Client::sendError(ErrorCode code) {
    std::string_view errorWord(Monidroid::ERROR_WORD);    
    boost::system::error_code ec;
    
    m_socket.write_some(boost::asio::buffer(errorWord), ec);
    m_socket.write_some(boost::asio::buffer((void*)&code, sizeof(code)), ec);
    
    m_socket.shutdown(ip::tcp::socket::shutdown_both);
}

void Client::sendError(const std::string_view msg) {
    std::string_view errorWord(Monidroid::ERROR_WORD);    
    ErrorCode code = ErrorCode::MessageEncoded;
    int len = errorWord.size();
    boost::system::error_code ec;
    
    m_socket.write_some(boost::asio::buffer(errorWord), ec);
    m_socket.write_some(boost::asio::buffer((void*)&code, sizeof(code)), ec);
    m_socket.write_some(boost::asio::buffer((void*)&len, sizeof(len)), ec);
    m_socket.write_some(boost::asio::buffer(errorWord), ec);
    
    m_socket.shutdown(ip::tcp::socket::shutdown_both);
}

void Client::dpmsHandler(int dpms_mode, void *user_data) {
    
}

void Client::modeHandler(evdi_mode mode, void *user_data) {
    Client* client = static_cast<Client*>(user_data);

    client->m_preffered = {
        .width = (u32)mode.width,
        .height = (u32)mode.height,
        .refreshRate = (u32)mode.refresh_rate
    };

    evdi_unregister_buffer(client->m_handle, client->m_frameBufferInfo.id);

    client->allocFrameBuffer(mode.width, mode.height);
}

void Client::updateHandler(int buffer_to_be_updated, void *user_data) {
    Client* client = static_cast<Client*>(user_data);

    client->grabAndSend(buffer_to_be_updated);
}

void Client::disconnectMonitor() {
    evdi_unregister_buffer(m_handle, m_frameBufferInfo.id);

    evdi_close(m_handle);
}
