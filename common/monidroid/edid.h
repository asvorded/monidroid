#pragma once

namespace Monidroid {
    using u8 = unsigned char;
    using u16 = unsigned short;
    using u32 = unsigned int;
    
    struct EDID {
        u8 header[8];
        struct {
            u8 manufacturerId[2];
            u8 productId[2];
            u8 serial[4];
            u8 week;
            u8 year;
        } vendor;
        struct {
            u8 version;
            u8 revision;
        } edid;
        struct {
            u8 inputDef;
            u8 hSize;
            u8 vSize;
            u8 gamma;
            u8 features;
        } features;
        struct {
            u8 redGreen;
            u8 blueWhite;
            u8 redX;   u8 redY;
            u8 greenX; u8 greenY;
            u8 blueX;  u8 blueY;
            u8 whiteX; u8 whiteY;
        } color;
        struct EstablishedTimings {
            u8 timingsOne;
            u8 timingsTwo;
            u8 myTimings;
        } estTimings;
        struct StandardTiming {
            u8 hPixels;
            u8 ar_refreshRate;
        } standardTimings[8];
        union {
            struct DetailedTiming {
                u16 pixel_clock;
                u8 hactive_lo;
                u8 hblank_lo;
                u8 hactive_hblank_hi;
                u8 vactive_lo;
                u8 vblank_lo;
                u8 vactive_vblank_hi;
                u8 hsync_offset_lo;
                u8 hsync_width_lo;
                u8 vsync_offset_width_lo;
                u8 hsync_vsync_offset_width_hi;
                u8 width_mm_lo;
                u8 height_mm_lo;
                u8 width_height_mm_hi;
                u8 hborder;
                u8 vborder;
                u8 misc;
            } timing;
            struct DisplayDescriptor {
                u8 __r1[3];
                u8 tag;
                u8 __r2;
                u8 data[13];
            } desc;
        } dataBlocks[4];
        u8 extCount;
        u8 checksum;
    };
    
    static_assert(sizeof(EDID) == 128);

    inline u8 edidChecksum(const EDID &edid) {
        auto *raw = reinterpret_cast<const u8*>(&edid);
        unsigned int sum = 0;
        for (int i = 0; i < sizeof(edid) - 1; ++i) {
            sum += raw[i];
        }
        u16 round = (sum + 0x100) & ~0xFF;
        return round - sum;
    }

    inline constexpr u8 CUSTOM_RAW_EDID[128] = {
        0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,

        0b0'0110100,0b100'00100,        // Manufacturer ID := MDD => 13, 4, 4 => 0 01101, 00100, 00100
        0x00,0x00,                      // Product ID (unspecified)
        0x00,0x00,0x00,0x00,            // Serial number (unspecified)
        0x00,0x24,                      // Week, year of manufacture (week := -, year := 2026 => 2026 - 1990 = 36 => 0x24)

        0x01,0x04,                      // EDID version := 1.4

        0b1'000'0000,                   // Video Input Definition := digital, no color depth, no digital interface
        0x00,0x00,                      // Screen Size/Aspect Ratio := no size specified
        0x00,                           // Gamma := 1.00
        0b000'11'100,                   // Feature Support := DPM-compliant, all color formats (bit 7 at 14h is '1'),
                                        // TODO: Bit 1,
                                        // non-continuous frequency

        0x6C,0xE5,                      // Red / Green, Blue / White
        0xA5,0x55,                      // Red x,y
        0x50,0xA0,                      // Green x,y
        0x23,0x0B,                      // Blue x,y
        0x50,0x54,                      // White point x,y

        0b00100001,0b00001000,0x00,     // Established timings: 1 := 640 x 480 @ 60, 800 x 600 @ 60; 2 := 1024 x 768 @ 60
        
        0xD1,0xC0,                      // 1 standard timing := 1920 x 1080 @ 60
        0x01,0x01,                      // no standard timing
        0x01,0x01,                      // no standard timing
        0x01,0x01,                      // no standard timing
        0x01,0x01,                      // no standard timing
        0x01,0x01,                      // no standard timing
        0x01,0x01,                      // no standard timing
        0x01,0x01,                      // no standard timing

        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 1 18-byte block: reserved for mobile device

        0x00,0x00,0x00,                 // 2 18-byte block: Display descriptor
        0xFC,                           // Display Product Name
        0x00,                           // Reserved
         'M', 'o', 'n', 'i', 'd', 'r', 'o', 'i', 'd', '\n', ' ', ' ', ' ',

        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,
        0x00                            // Checksum
    };

    inline constexpr EDID CUSTOM_EDID {
        .header = { 0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00 },
        .vendor {
            .manufacturerId = { 0b0'01101'00,0b100'11010 },  // MDZ => 13, 4, 26 => 0 (must be '0'), 01101, 00100, 11010
            .productId = { 0x00, 0x00 },
            .serial = { 0x00, 0x00, 0x00, 0x00 },
            .week = 0, .year = 2026 - 1990,                  // 2026
        },
        .edid { .version = 1, .revision = 4 },
        .features {
            .inputDef = 0b1'000'0000,                        // digital, no color depth, no digital interface
            .hSize = 0, .vSize = 0,                          // no size specified
            .gamma = 0x00,                                   // 1.00
            .features = 0b000'11'110,                        // DPM-compliant, all color formats (bit 7 at 14h is '1'), 
                                                             // pref. timing mode contains pixel format and refresh rate
                                                             // non-continuous frequency
        },
        .color {
            .redGreen = 0x6C, .blueWhite = 0xE5,
            .redX = 0xA5,     .redY = 0x55,
            .greenX = 0x50,   .greenY = 0xA0,
            .blueX = 0x23,    .blueY = 0x0B,
            .whiteX = 0x50,   .whiteY = 0x54,
        },
        .estTimings {
            .timingsOne = 0b00100001,                        // 640 x 480 @ 60, 800 x 600 @ 60
            .timingsTwo = 0b00001000,                        // 1024 x 768 @ 60
            .myTimings = 0,
        },
        .standardTimings {
            [0] = {                                          // 1920 x 1080 @ 60
                .hPixels = (1920 / 8) - 31,
                .ar_refreshRate = (0b11 << 6) | (60 - 60),   // 16 : 9                            
            },
            [1] = { 0x01, 0x01 },
            [2] = { 0x01, 0x01 },
            [3] = { 0x01, 0x01 },
            [4] = { 0x01, 0x01 },
            [5] = { 0x01, 0x01 },
            [6] = { 0x01, 0x01 },
            [7] = { 0x01, 0x01 },
        },
        .dataBlocks {
            [0] = {                          // Mobile device screen's timings
                .timing {
                    .pixel_clock = 2400 * 1500 * 60 / 10'000,
                    .hactive_lo = 2400 & 0xFFu,
                    .hblank_lo = 0,
                    .hactive_hblank_hi = ((2400 & 0x0F00u) >> 4) | ((0 & 0x0F00u) >> 8),
                    .vactive_lo = 1500 & 0xFFu,
                    .vblank_lo = 0,
                    .vactive_vblank_hi = ((1500 & 0x0F00u) >> 4) | ((0 & 0x0F00u) >> 8),
                    .hsync_offset_lo = 0 & 0xFFu,
                    .hsync_width_lo = 0 & 0xFFu,
                    .vsync_offset_width_lo = ((0 & 0x0Fu) << 4) | (0 & 0x0Fu),
                    .hsync_vsync_offset_width_hi = ((0 & 0x30u) << 6) | ((0 & 0x30u) << 4) | ((0 & 0x30u) << 2) | (0 & 0x30u),
                    .width_mm_lo = 0,
                    .height_mm_lo = 0,
                    .width_height_mm_hi = 0,
                    .hborder = 0,
                    .vborder = 0,
                    .misc = 0b0'00'1111'0    // non-interlaced, no stereo, digital separate sync, +hsync, +vsync
                }
            },                       
            [1] = {
                .desc {
                    .tag = 0xFC,             // Display Product Name
                    .data = { 'M', 'o', 'n', 'i', 'd', 'r', 'o', 'i', 'd', '\n', ' ', ' ', ' ' }
                }
            },
            [2] = { .desc { .tag = 0x10 } }, // Dummy descriptor
            [3] = { .desc { .tag = 0x10 } }, // Dummy descriptor
        },
        .extCount = 0,
        .checksum = 0,
    };
    
    inline void setMode(EDID &edid, int width, int height, int hertz) {
    }
}
