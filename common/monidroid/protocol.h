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
        inline constexpr char CL_USB_WELCOME_WORD[PROTOCOL_WORD_LEN + 1]  = "CUSBW";

        inline constexpr unsigned int ADB_IF_CLASS         = 0xFF4201;
        inline constexpr unsigned int ADB_MTP_IF_CLASS     = 0x060101;

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

        inline constexpr char SV_FRAME_WORD[PROTOCOL_WORD_LEN + 1]        = "SFRME";

        inline constexpr char SV_ERROR_WORD[PROTOCOL_WORD_LEN + 1]        = "SERRC";
        
        // Extensions
        inline constexpr char EXTENSION_WORD[PROTOCOL_WORD_LEN + 1]       = "MDEXT";

        inline constexpr char NO_EXTENSION_WORD[PROTOCOL_WORD_LEN + 1]    = "MDNOX";
    }

    namespace Control {
        inline constexpr auto PORT               = 14768;

        inline constexpr auto GET_CLIENTS        = "clients/all";
        inline constexpr auto CONNECTED_EVENT    = "clients/new";
        inline constexpr auto DISCONNECTED_EVENT = "clients/disconnected";
        inline constexpr auto GET_CONFIG         = "config/all";
        inline constexpr auto SET_STATE          = "config/serverState";
        inline constexpr auto SHUTDOWN           = "config/shutdown";
    }

    namespace Ex {
        // Delay calculation
        inline constexpr char CL_CALC_DELAY_WORD[PROTOCOL_WORD_LEN + 1]   = "CPING";

        inline constexpr char SV_CALC_DELAY_WORD[PROTOCOL_WORD_LEN + 1]   = "SPING";
    }
}
