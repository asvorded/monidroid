#pragma once

namespace Monidroid {

    // New protocol
    inline namespace ProtocolV2 {
        inline constexpr auto PROTOCOL_PORT        =  14770;

        inline constexpr auto PROTOCOL_WORD_LEN    =  5;

        // ECHO
        inline constexpr auto CL_ECHO_WORD         =  "CECHO";

        inline constexpr auto SV_ECHO_WORD         =  "SECHO";
        
        // Client side
        inline constexpr auto CL_WELCOME_WORD      =  "WLCME";
        
        // Server side        
        inline constexpr auto SV_STREAM_WORD       =  "RTPON";

        inline constexpr auto SV_FRAME_WORD        =  "FRAME";

        inline constexpr auto SV_ERROR_WORD        =  "ERROR";
        
        // Extensions
        inline constexpr auto EXTENSION_WORD       =  "MDNXT";
    }

    inline namespace ProtocolV2Ext {
        
    }
}
