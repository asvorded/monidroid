#include "client.h"

#include <fstream>
#include <array>
#include <memory>

#include <turbojpeg.h>

#include "monidroid/logger.h"
#include "monidroid/edid.h"
#include "monidroid/debug.h"

Client::Client(ip::tcp::socket socket)
  : m_socket(std::move(socket)),
    m_codec(),
    m_pic(),
    m_preffered(),
    m_isUsb(false)
{
    m_netBuffer.reserve(7u + 4 + 12);
}

Client::~Client() {
    if (m_codec) {
        x264_encoder_close(m_codec);
    }

    boost::system::error_code ec;
    m_socket.close(ec);
}

const std::string &Client::modelName() const {
    return m_modelName;
}

ClientState Client::state() const {
    return m_state;
}

bool Client::isUsb() const {
    return m_isUsb;
}

bool Client::identifyClient() {
    enum class WelcomeStates { Welcome, Model, Modes };

    char recvBuf[256];
    int bytesNeeded = 0;
    WelcomeStates state = WelcomeStates::Welcome;

    bytesNeeded = Monidroid::PROTOCOL_WORD_LEN + sizeof(int);
    
    while (bytesNeeded > 0) {
        boost::system::error_code ec;
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
            std::string_view word(m_netBuffer.data(), Monidroid::PROTOCOL_WORD_LEN);

            if (word == Monidroid::CL_WELCOME_WORD) {
                m_isUsb = false;
            } else if (word == Monidroid::CL_USB_WELCOME_WORD) {
                m_isUsb = true;
            } else {
                Monidroid::DefaultLog("\"Welcome\" word mismatch, unknown client");
                return false;
            }

            int nameLength = *(reinterpret_cast<int*>(m_netBuffer.data() + Monidroid::PROTOCOL_WORD_LEN));
            
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

    return sync();
}

bool Client::sync() {
    std::string syncWord(Monidroid::BUF_STUB);
    error_code ec;

    asio::read(m_socket, asio::buffer(syncWord), ec);
    // Set stamp right after read() returns
    m_syncTime = steady_clock::now();
    
    if (ec) {
        Monidroid::TaggedLog(m_modelName, "Time sync failed, socket error \"{}\"", ec.message());
        return false;
    } else if (syncWord != Monidroid::CL_TIME_SYNC_WORD) {
        Monidroid::TaggedLog(m_modelName, "Time sync failed, {} word mismatch ", Monidroid::CL_TIME_SYNC_WORD);
        sendError(ErrorCode::InvalidClient);
        return false;
    }

    return true;
}

bool Client::initEncoder(const MonitorMode &mode) {
    x264_param_t param;

    if (x264_param_default_preset(&param, "ultrafast", "zerolatency") < 0) {
        Monidroid::TaggedLog(m_modelName, "Failed to init preset and tune");
        return false;
    }

    // Configure non-default params
    param.i_bitdepth = 8;
    param.i_csp = X264_CSP_BGRA;
    param.i_width  = (int)mode.width;
    param.i_height = (int)mode.height;
    param.i_fps_num = (int)mode.refreshRate;
    param.i_fps_den = 1;
    param.b_repeat_headers = 1;

    param.b_vfr_input = 1;
    param.i_timebase_num = 1;
    param.i_timebase_den = 1'000'000; // microseconds
    // TODO: size or start codes?
    // put 4 byte size
    param.b_annexb = 0;
    param.i_bframe = 0;

    x264_param_apply_profile(&param, "high");

    m_codec = x264_encoder_open(&param);
    if (!m_codec) {
        Monidroid::TaggedLog(m_modelName, "Failed to open encoder");
        return false;
    }

    x264_picture_init(&m_pic);
    m_pic.img.i_csp = param.i_csp;
    m_pic.img = {
        .i_csp = param.i_csp,
        // Using BGRA, so 1 plane
        .i_plane = 1,
        // Stride size and plane pointer will be set after each frame request and map
        .i_stride = { },
        .plane = { },
    };

    return true;
}

bool Client::reconfigureEncoder(const MonitorMode &mode) {
    if (m_codec) {
        x264_encoder_close(m_codec);
        m_codec = nullptr;
    }

    return initEncoder(mode);
}

bool Client::connectMonitor(const Adapter &adapter) {
    m_monitor = adapterConnectMonitor(adapter, m_modelName, m_preffered);
    if (!m_monitor) {
        m_state = ClientState::Error;
        return false;
    }

    if (!initEncoder(m_preffered)) {
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
    std::vector<TimeLogEntry> logData(3'000);

    m_inputThread = std::jthread([this]() { receiveMain(); });

    FrameMetadata meta { };
    FrameMapInfo info { };

    while (m_state == ClientState::Streaming) {
        auto t1_start = steady_clock::now();
        FrameStatus status = monitorRequestFrame(m_monitor, &meta);
        auto t1_end = steady_clock::now();
        
        switch (status) {
        case FrameStatus::ModeChanged: {
            MonitorMode mode = monitorRequestMode(m_monitor, false);
            if (!reconfigureEncoder(mode)) {
                Monidroid::TaggedLog(m_modelName, "Failed to reconfigure encoder");
                sendError(ErrorCode::SessionError);
                m_state = ClientState::Error;
            }
            break;
        }
        case FrameStatus::FrameReady: {
            frameFails = 0;

            double time_point = duration<double>(steady_clock::now() - m_syncTime).count();
            double duration1 = duration<double, std::milli>(t1_end - t1_start).count();
            
            monitorMapCurrent(m_monitor, &info);
            if (info.data != nullptr) {
                mapFails = 0;
                
                auto t2_start = steady_clock::now();
                // sendFullFrame(info, meta);
                sendStreamFrame(info, meta);
                auto t2_end = steady_clock::now();
                
                double duration2 = duration<double, std::milli>(t2_end - t2_start).count();
                logData.push_back({ time_point, duration1, duration2 });

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
        }
        case FrameStatus::NoUpdates:
            frameFails = 0;

            break;
        case FrameStatus::MonitorOff:
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

    std::string fileName = m_modelName + (m_isUsb ? " USB" : " Wi-Fi") + ".csv";
    std::ofstream file(fileName);
    
    if (file.is_open()) {
        file << "time_point,request_time_ms,send_time_ms\r\n";

        for (const auto& entry : logData) {
            file << entry.time_point << ","
                 << entry.duration1 << ","
                 << entry.duration2 << "\r\n";
        }
        
        file.close();
        Monidroid::TaggedLog(m_modelName, "Timings have been written to \"{}\"", fileName);
    } else {
        Monidroid::TaggedLog(m_modelName, "An error had occurred while writing timings to the file");
    }
}

void Client::sendFullFrame(const FrameMapInfo& info, const FrameMetadata& meta) {
    tjhandle tj = tj3Init(TJINIT_COMPRESS);
    unsigned char *jpegData = static_cast<unsigned char*>(tj3Alloc(1));
    size_t _jpegsize = 0;
    
    tj3Set(tj, TJPARAM_QUALITY, 1);
    tj3Set(tj, TJPARAM_SUBSAMP, TJSAMP_411);
    
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
    
    // std::array<const_buffer, 3> buffers {
    //     asio::buffer(std::string_view(Monidroid::SV_FRAME_WORD)),
    //     asio::buffer((void*)&jpegSize, sizeof(jpegSize)),
    //     asio::buffer(jpegData, jpegSize),
    // };

    u64 frameTime = meta.timestampNs - duration_cast<nanoseconds>(m_syncTime.time_since_epoch()).count();
    std::array<const_buffer, 4> buffers = {
        asio::buffer(std::string_view(Monidroid::SV_FRAME2_WORD)),
        asio::buffer((void*)&frameTime, sizeof(frameTime)),
        asio::buffer((void*)&jpegSize, sizeof(jpegSize)),
        asio::buffer(jpegData, jpegSize),
    };
    
    error_code ec;
    asio::write(m_socket, buffers, ec);
    if (ec) {
        std::cout << ec.message() << "\n";
        m_state = ClientState::ConnectionClosed;
    }

    tj3Free(jpegData);
    tj3Destroy(tj);
}

void Client::sendStreamFrame(const FrameMapInfo &info, const FrameMetadata &meta) {
    m_pic.img.i_stride[0] = (int)info.stride;
    m_pic.img.plane[0] = reinterpret_cast<uint8_t*>(info.data);
    u64 frameTime = meta.timestampNs - duration_cast<nanoseconds>(m_syncTime.time_since_epoch()).count();
    m_pic.i_pts = frameTime / 1000; // ns -> μs

    int numNals = 0;
    x264_nal_t *nals;
    x264_picture_t picOut;
    int dataSize = x264_encoder_encode(m_codec, &nals, &numNals, &m_pic, &picOut);
    if (dataSize < 0) {
        Monidroid::TaggedLog(m_modelName, "Frame encoding failed");
        return;
    } else if (dataSize == 0) {
        return;
    }

    std::array<const_buffer, 4> buffers = {
        asio::buffer(std::string_view(Monidroid::SV_STREAM_FRAME_WORD)),
        asio::buffer((void*)&frameTime, sizeof(frameTime)),
        asio::buffer((void*)&dataSize, sizeof(dataSize)),
        asio::buffer(nals->p_payload, dataSize),
    };

    error_code ec;
    asio::write(m_socket, buffers, ec);
    if (ec) {
        std::cout << ec.message() << "\n";
        m_state = ClientState::ConnectionClosed;
    }

    // // TODO: remove after testing
    // std::ofstream outFile("frames.h264", std::ios_base::app);
    // if (outFile.is_open()) {
    //     outFile.write(reinterpret_cast<char*>(nals->p_payload), dataSize);
    //     outFile.close();
    // } else {
    //     sendError(ErrorCode::Unspecified);
    //     // TODO: sendError() sets Error state
    // }
}

void Client::sendMonitorOff() {
    int jpegSize = 0;

    std::array<const_buffer, 2> buffers {
        asio::buffer(std::string_view(Monidroid::SV_FRAME_WORD)),
        asio::buffer((void*)&jpegSize, sizeof(jpegSize)),
    };
    
    error_code ec;
    asio::write(m_socket, buffers, ec);
    if (ec) {
        std::cout << ec.message() << "\n";
        m_state = ClientState::ConnectionClosed;
    }
}

void Client::receiveMain() {
    std::string word(5, 'X');

    try {
        while (m_state == ClientState::Streaming) {
            asio::read(m_socket, asio::buffer(word));

            if (word == CL_INPUT_WORD) {
                handleInput();
            } else {
                Monidroid::TaggedLog(m_modelName, "Unknown word {}, ignoring", word);
            }
        }
    } catch (const system_error& e) {
        if (e.code() != asio::error::eof) {
            Monidroid::TaggedLog(m_modelName, "Receive loop failed: \"{}\"", e.code().message());
        }
    }
}

void Client::handleInput() {
    u8 buf[256];
    // Read type
    asio::read(m_socket, asio::buffer(buf, 1));
    switch ((InputType)buf[0]) {
        case InputType::MouseMove: {
            // <X offset(int)><Y offset(int)> (total 8 bytes)
            asio::read(m_socket, asio::buffer(buf, 8));
            int *deltas = reinterpret_cast<int*>(buf);
            monitorSendInput(m_monitor, deltas[0], deltas[1]);
            break;
        }
        case InputType::MouseButtons:
            // <flags(byte)> (total 1 byte)
            asio::read(m_socket, asio::buffer(buf, 1));
            monitorSendInput(m_monitor, buf[0]);
            break;
        case InputType::MouseScroll:
            // <delta(int)> (total 4 bytes)
            asio::read(m_socket, asio::buffer(buf, 4));
            monitorSendInput(m_monitor, *reinterpret_cast<int*>(buf));
            break;
        default:
            throw new std::runtime_error("[TODO] Input type is not implemented");
    }
}

void Client::disconnectMonitor() {
    if (m_inputThread.joinable()) {
        m_inputThread.join();
    }

    // TODO Windows: process 1291 () error code
    monitorDisconnect(m_monitor);

    m_state = ClientState::Disconnected;

    error_code ec;
    m_socket.shutdown(m_socket.shutdown_both, ec);
}

void Client::forceDisconnect(bool withError) {
    if (withError) {
        sendError(ErrorCode::DisconnectedByServer);
    } else {
        error_code ec;
        m_socket.shutdown(m_socket.shutdown_both, ec);
    }

    m_state = ClientState::ConnectionClosed;
}

void Client::sendError(ErrorCode code) {
    std::array<const_buffer, 2> buffers = {
        asio::buffer(std::string_view(Monidroid::SV_ERROR_WORD)),
        asio::buffer((void*)&code, sizeof(code))
    };

    error_code ec;
    asio::write(m_socket, buffers, ec);
    if (ec) std::cout << ec.message() << "\n";
    m_socket.shutdown(m_socket.shutdown_both, ec);

    m_state = ClientState::Error;
}

void Client::sendError(const std::string_view msg) {
    ErrorCode code = ErrorCode::MessageEncoded;
    int len = msg.size();

    std::array<const_buffer, 4> buffers {
        asio::buffer(std::string_view(Monidroid::SV_ERROR_WORD)),
        asio::buffer((void*)&code, sizeof(code)),
        asio::buffer((void*)&len, sizeof(len)),
        asio::buffer(msg),
    };

    error_code ec;
    asio::write(m_socket, buffers, ec);
    if (ec) std::cout << ec.message() << "\n";
    m_socket.shutdown(m_socket.shutdown_both, ec);

    m_state = ClientState::Error;
}
