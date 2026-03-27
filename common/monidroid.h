#pragma once

#define MONIDROID_SERVICE_VERSION                     "0.1.0"

namespace Monidroid {

    // Network
    inline constexpr auto MONIDROID_PORT_SZ         = "14765";
    inline constexpr auto MONIDROID_PORT            =  14765;
    
    // Protocol
    inline constexpr auto WELCOME_WORD              = "WELCOME";
    
    inline constexpr auto FRAME_WORD                = "FRAME";
    
    inline constexpr auto CLIENT_ECHO_WORD          = "MDCLIENT_ECHO";
    
    inline constexpr auto SERVER_ECHO_WORD          = "MDIDD_ECHO";

    // Info
    inline constexpr auto MAX_MONITORS_SUPPORTED    = 16;
} // namespace Monidroid
