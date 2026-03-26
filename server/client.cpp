#include "client.h"

#include <string_view>
#include <cstring>
#include <array>
#include <memory>

#include <turbojpeg.h>
#include <gst/gst.h>

#include "monidroid.h"
#include "monidroid/logger.h"

const unsigned char Client::MD_EDID[128] = {
    0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,
    0x10,0xAC,0xE6,0xD0,0x55,0x5A,0x4A,0x30,0x24,0x1D,0x01,
    0x04,0xA5,0x3C,0x22,0x78,0xFB,0x6C,0xE5,0xA5,0x55,0x50,0xA0,0x23,0x0B,0x50,0x54,0x00,0x02,0x00,
    0xD1,0xC0,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x58,0xE3,0x00,
    0xA0,0xA0,0xA0,0x29,0x50,0x30,0x20,0x35,0x00,0x55,0x50,0x21,0x00,0x00,0x1A,0x00,0x00,0x00,0xFF,
    0x00,0x37,0x4A,0x51,0x58,0x42,0x59,0x32,0x0A,0x20,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xFC,0x00,
    0x53,0x32,0x37,0x31,0x39,0x44,0x47,0x46,0x0A,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xFD,0x00,0x28,
    0x9B,0xFA,0xFA,0x40,0x01,0x0A,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x2C
};

Client::Client(ip::tcp::socket socket) : m_socket(std::move(socket)) {
    m_netBuffer.reserve(7 + 4 + 12);
}

void Client::identifyClient() {
    enum class WelcomeStates { Welcome, Model, Modes };

    char recvBuf[256];
    int bytesNeeded = 0;
    WelcomeStates state = WelcomeStates::Welcome;

    boost::system::error_code ec;

    std::string_view welcomeWord(Monidroid::WELCOME_WORD);
    bytesNeeded = welcomeWord.size() + sizeof(int);
    
    while (bytesNeeded > 0) {
        size_t bytesReceived = m_socket.read_some(boost::asio::buffer(recvBuf, bytesNeeded));
        // if (ec) {
        //     break;
        // }
        m_netBuffer.insert(m_netBuffer.end(), recvBuf, recvBuf + bytesReceived);
        // bytesCount += bytesReceived;
        if (m_netBuffer.size() < bytesNeeded) continue;

        switch (state) {
        case WelcomeStates::Welcome:
        {
            std::string_view word(m_netBuffer.data(), welcomeWord.size());
            if (word != welcomeWord) {
                // TODO
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
            m_width = settings[0];
            m_height = settings[1];
            m_hertz = settings[2];

            Monidroid::DefaultLog("New client identified as \"{}\", preferred mode: {}x{}@{}", m_modelName, m_width, m_height, m_hertz);
            
            m_netBuffer.erase(m_netBuffer.begin(), m_netBuffer.begin() + bytesNeeded);
            bytesNeeded = 0;
        }
        }
    }
}

int Client::connectMonitor() {
    // Fine device number (X from /dev/dri/card[X])
    evdi_add_device();
    m_devNumber = 0;
    while (evdi_check_device(m_devNumber) != AVAILABLE) m_devNumber++;

    // Connect monitor
    m_handle = evdi_open(m_devNumber);
    evdi_connect(m_handle, MD_EDID, sizeof(MD_EDID), m_width * m_height);

    // Set up buffer
    allocFrameBuffer(m_width, m_height);

    // gst_parse_launch("appsrc ! gpegenc");

    return 0;
}

int Client::sendFrames() {
    struct evdi_event_context ctx = {
        .dpms_handler = dpmsHandler,
        .mode_changed_handler = modeHandler,
        .update_ready_handler = updateHandler,
        .user_data = this
    };

    m_sending = true;
    while (m_sending) {
        bool ready = evdi_request_update(m_handle, m_frameBufferInfo.id);
        if (ready) {
            grabAndSend(m_frameBufferInfo.id);
        } else {
            evdi_handle_events(m_handle, &ctx);
        }
    }
    
    return 0;
}

int Client::allocFrameBuffer(int width, int height) {
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

    return 0;
}

int Client::initPipeline() {
    

    return 0;
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
            m_width, 0, m_height, TJPF_RGBA,
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

void Client::dpmsHandler(int dpms_mode, void *user_data) {
    
}

void Client::modeHandler(evdi_mode mode, void *user_data) {
    Client* client = static_cast<Client*>(user_data);

    client->m_width = mode.width;
    client->m_height = mode.height;    
    client->m_hertz = mode.refresh_rate;

    evdi_unregister_buffer(client->m_handle, client->m_frameBufferInfo.id);

    client->allocFrameBuffer(mode.width, mode.height);
}

void Client::updateHandler(int buffer_to_be_updated, void *user_data) {
    Client* client = static_cast<Client*>(user_data);

    client->grabAndSend(buffer_to_be_updated);
}

int Client::disconnectMonitor() {
    evdi_unregister_buffer(m_handle, m_frameBufferInfo.id);

    evdi_close(m_handle);
    
    return 0;
}

int Client::finalize() {
    boost::system::error_code ec;
    m_socket.shutdown(m_socket.shutdown_both, ec);
    m_socket.close();

    return 0;
}
