#pragma once

namespace Monidroid {

    // New protocol
    inline namespace V2 {
        inline constexpr auto PROTOCOL_PORT        = 14770;
        inline constexpr auto ADB_PORT             = 14767;

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
        inline constexpr char CL_ECHO_WORD[PROTOCOL_WORD_LEN + 1]         = "CECHO";

        inline constexpr char SV_ECHO_WORD[PROTOCOL_WORD_LEN + 1]         = "SECHO";
        
        // Client side
        inline constexpr char CL_WELCOME_WORD[PROTOCOL_WORD_LEN + 1]      = "CWLCM";

        inline constexpr char CL_INPUT_WORD[PROTOCOL_WORD_LEN + 1]        = "CINPT";

        enum class InputType : int {
            MouseMove    = 1,
            MouseButtons = 2,
            MouseScroll  = 3,

            Mouse        = 4,

            Touch        = 5,
        };
        
        // Server side        
        inline constexpr char SV_STREAM_WORD[PROTOCOL_WORD_LEN + 1]       = "SSTRM";

        inline constexpr char SV_USB_WORD[PROTOCOL_WORD_LEN + 1]          = "SUSBI";

        inline constexpr char SV_FRAME_WORD[PROTOCOL_WORD_LEN + 1]        = "SFRME";

        inline constexpr char SV_ERROR_WORD[PROTOCOL_WORD_LEN + 1]        = "SERRC";
        
        // Extensions
        inline constexpr char EXTENSION_WORD[PROTOCOL_WORD_LEN + 1]       = "MDEXT";

        inline constexpr char NO_EXTENSION_WORD[PROTOCOL_WORD_LEN + 1]    = "MDNOX";
    }

    inline constexpr auto ADB_IF_CLASS         = 0xFF;
    inline constexpr auto ADB_IF_SUBCLASS      = 0x42;
    inline constexpr auto ADB_IF_PROTOCOL      = 0x01;

    namespace Ex {
        // Delay calculation
        inline constexpr char CL_CALC_DELAY_WORD[PROTOCOL_WORD_LEN + 1]   = "CPING";

        inline constexpr char SV_CALC_DELAY_WORD[PROTOCOL_WORD_LEN + 1]   = "SPING";
    }
}
