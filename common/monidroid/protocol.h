#pragma once

namespace Monidroid {
    // New protocol
    inline namespace V2 {
        inline constexpr auto PROTOCOL_PORT        = 14770;

        inline constexpr auto PROTOCOL_WORD_LEN    = 5;

        enum class ErrorCode : int {
            MessageEncoded            = 0,

            NotIdentified             = 1,
            IncorrectMonitorOptions   = 2,
            InvalidClient             = 3,
            
            DisconnectedByServer      = 10,
            MonitorConnectFail        = 11,
            TooManyFails              = 12,

            Unspecified               = 100,
        };

        // ECHO
        inline constexpr auto CL_ECHO_WORD         = "CECHO";

        inline constexpr auto SV_ECHO_WORD         = "SECHO";
        
        // Client side
        inline constexpr auto CL_WELCOME_WORD      = "CWLCM";
        
        // Server side        
        inline constexpr auto SV_STREAM_WORD       = "SSTRM";

        inline constexpr auto SV_FRAME_WORD        = "SFRME";

        inline constexpr auto SV_ERROR_WORD        = "SERRC";
        
        // Extensions
        inline constexpr auto EXTENSION_WORD       = "MDNXT";
    }

    namespace Extensions {
        // USB detection
        inline constexpr auto CL_USB_ECHO_WORD     = "CEUSB";

        inline constexpr auto SV_USB_ECHO_WORD     = "SEUSB";

        // Delay calculation
        inline constexpr auto CL_CALC_DELAY_WORD   = "CPING";

        inline constexpr auto SV_CALC_DELAY_WORD   = "SPING";
    }
}
