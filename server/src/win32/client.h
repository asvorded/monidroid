#pragma once

#include <thread>
#include <vector>
#include <string>

#include <boost/asio.hpp>

using namespace boost::asio;

struct Client {
    using ColorType = int;

public:
    ip::tcp::socket m_socket;
    std::vector<char> m_netBuffer;
    bool m_sending = false;

    int m_width = -1;
    int m_height = -1;
    int m_hertz = -1;

    std::string m_modelName;

    evdi_handle m_handle = EVDI_INVALID_HANDLE;
    int m_devNumber = 0;

    evdi_buffer m_frameBufferInfo;
    std::vector<ColorType> m_frameBuffer;

    std::thread m_communicationThread; // ???

    static const unsigned char MD_EDID[128];
    static constexpr auto SKU_AREA_LIMIT = 60;

    int allocFrameBuffer(int width, int height);
    int initPipeline();

    int grabAndSend(int bufferId);

    static void dpmsHandler(int dpms_mode, void *user_data);
    static void modeHandler(struct evdi_mode mode, void *user_data);
    static void updateHandler(int buffer_to_be_updated, void *user_data);

public:
    Client(ip::tcp::socket socket);

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    void identifyClient();
    int connectMonitor();
    int sendFrames();
    int disconnectMonitor();
    int finalize();
};
