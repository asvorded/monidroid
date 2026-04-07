#include "client.h"

#include <cstring>
#include <array>
#include <memory>

#include <turbojpeg.h>
#include <gst/gst.h>

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
    m_monitor = adapterConnectMonitor(adapter, m_modelName, m_preffered);
    if (!m_monitor) {
        m_state = ClientState::Error;
        return false;
    }

    // Set up buffer
    allocFrameBuffer(m_preffered.width, m_preffered.height);

    // gst_parse_launch("appsrc ! gpegenc");

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
            } else {
                ++mapFails;
                if (mapFails >= 10) {
                    Monidroid::TaggedLog(m_modelName, "Too many map() fails, stopping sending...");
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
                m_state = ClientState::Error;
            }
            break;
        }
    }
}

void Client::allocFrameBuffer(int width, int height) {
    // 
}

void Client::initPipeline() {
    //
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

void Client::disconnectMonitor() {
    // TODO Windows: process 1291 () error code
    monitorDisconnect(m_monitor);
    m_state = ClientState::Disconnected;
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
}

void Client::sendError(const std::string_view msg) {

}
