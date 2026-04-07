#pragma once

#include <cmath>
#include <stdint.h>

namespace Monidroid {
    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;

    inline constexpr auto EDID_H_BLANK = 80u;
    inline constexpr auto EDID_H_FRONT = 8u;
    inline constexpr auto EDID_H_SYNC  = 32u;

    inline constexpr auto EDID_MIN_V_BLANK_US = 460u;

    inline constexpr auto EDID_V_SYNC  = 8u;
    inline constexpr auto EDID_V_BACK  = 6u;
    
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

        void setDefaultMode(int width, int height, int hertz) {
            // Black magic, do not try to understand how it works
            double vFieldRate = hertz * (1 + 350 / 1'000'000.0); // ver.3: +350 ppm
            double hPeriodEst = (1'000'000.0 / vFieldRate - EDID_MIN_V_BLANK_US) / height;
            u32 vbiLines = (u32)ceil(EDID_MIN_V_BLANK_US / hPeriodEst);
            u32 rbMinVbi = 1 + EDID_V_SYNC + EDID_V_BACK;
            
            u32 vBlank = vbiLines < rbMinVbi ? rbMinVbi : vbiLines;
            u32 vOffset = vBlank - EDID_V_BACK - EDID_V_SYNC;
            
            dataBlocks[0].timing = {
                .pixel_clock = (u16)((width + EDID_H_BLANK) * (height + vBlank) * hertz / 10'000),
                .hactive_lo = (u8)(width & 0xFFu),
                .hblank_lo = EDID_H_BLANK,
                .hactive_hblank_hi = (u8)(((width & 0x0F00u) >> 4) | ((EDID_H_BLANK & 0x0F00u) >> 8)),
                .vactive_lo = (u8)(height & 0xFFu),
                .vblank_lo = (u8)(vBlank),
                .vactive_vblank_hi = (u8)(((height & 0x0F00u) >> 4) | ((vBlank & 0x0F00u) >> 8)),
                .hsync_offset_lo = EDID_H_FRONT & 0xFFu,
                .hsync_width_lo = EDID_H_SYNC & 0xFFu,
                .vsync_offset_width_lo = (u8)(((vOffset & 0x0Fu) << 4) | (EDID_V_SYNC & 0x0Fu)),
                .hsync_vsync_offset_width_hi = (u8)(((EDID_H_FRONT & 0x300u) >> 2)
                                                  | ((EDID_H_SYNC & 0x300u) >> 4)
                                                  | ((vOffset & 0x30u) >> 2)
                                                  | ((EDID_V_SYNC & 0x30u) >> 4)),
                .width_mm_lo = 0,
                .height_mm_lo = 0,
                .width_height_mm_hi = 0,
                .hborder = 0,
                .vborder = 0,
                .misc = 0b0'00'1111'0    // non-interlaced, no stereo, digital separate sync, +hsync, +vsync
            };
        }

        void commit() {
            auto *raw = reinterpret_cast<const u8*>(this);
            unsigned int sum = 0;
            for (int i = 0; i < sizeof(*this) - 1; ++i) {
                sum += raw[i];
            }
            u32 round = (sum + 0x100) & ~0xFF;
            checksum = (u8)(round - sum);
        }

        void* raw() {
            return reinterpret_cast<void*>(this);
        }
    };
    
    static_assert(sizeof(EDID) == 128);

    struct MonitorMode {
        u32 width;
        u32 height;
        u32 refreshRate;

        bool operator==(const MonitorMode& rhs) const = default;
    };

    inline constexpr EDID CUSTOM_EDID {
        .header = { 0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00 },
        .vendor {
            .manufacturerId = { 0b0'01101'00,0b100'10001 },  // MDQ => 13, 4, 17 => 0 (must be '0'), 01101, 00100, 10001
            .productId = { 0x00, 0x00 },
            .serial = { 0x00, 0x00, 0x00, 0x00 },
            .week = 0, .year = 2026 - 1990,                  // 2026
        },
        .edid { .version = 1, .revision = 4 },
        .features {
            .inputDef = 0b1'000'0000,                        // digital, no color depth, no digital interface
            .hSize = 0, .vSize = 0,                          // no size specified
            .gamma = 220 - 100,                              // 2.20
            .features = 0b000'11'010,                        // DPM-compliant, all color formats (bit 7 at 14h is '1'), 
                                                             // non sRGB
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
            {                                          // 1920 x 1080 @ 60
                .hPixels = (1920 / 8) - 31,
                .ar_refreshRate = (0b11 << 6) | (60 - 60),   // 16 : 9                            
            },
            { 0x01, 0x01 },
            { 0x01, 0x01 },
            { 0x01, 0x01 },
            { 0x01, 0x01 },
            { 0x01, 0x01 },
            { 0x01, 0x01 },
            { 0x01, 0x01 },
        },
        .dataBlocks {
            {                          // Mobile device screen's timings
                .timing {
                    .pixel_clock = (2400 + EDID_H_BLANK) * (1500 + 67) * 60u / 10'000,
                    .hactive_lo = 2400 & 0xFFu,
                    .hblank_lo = EDID_H_BLANK,
                    .hactive_hblank_hi = ((2400 & 0x0F00u) >> 4) | ((EDID_H_BLANK & 0x0F00u) >> 8),
                    .vactive_lo = 1500 & 0xFFu,
                    .vblank_lo = 67,
                    .vactive_vblank_hi = ((1500 & 0x0F00u) >> 4) | ((67 & 0x0F00u) >> 8),
                    .hsync_offset_lo = EDID_H_FRONT & 0xFFu,
                    .hsync_width_lo = EDID_H_SYNC & 0xFFu,
                    .vsync_offset_width_lo = ((53 & 0x0Fu) << 4) | (8 & 0x0Fu),
                    .hsync_vsync_offset_width_hi = ((EDID_H_FRONT & 0x300u) >> 2) | ((EDID_H_SYNC & 0x300u) >> 4) | ((53 & 0x30u) >> 2) | ((8 & 0x30u) >> 4),
                    .width_mm_lo = 0,
                    .height_mm_lo = 0,
                    .width_height_mm_hi = 0,
                    .hborder = 0,
                    .vborder = 0,
                    .misc = 0b0'00'1111'0    // non-interlaced, no stereo, digital separate sync, +hsync, +vsync
                }
            },                       
            {
                .desc {
                    .tag = 0xFC,             // Display Product Name
                    .data = { 'M', 'o', 'n', 'i', 'd', 'r', 'o', 'i', 'd', '\n', ' ', ' ', ' ' }
                }
            },
            { .desc { .tag = 0x10 } }, // Dummy descriptor
            { .desc { .tag = 0x10 } }, // Dummy descriptor
        },
        .extCount = 0,
        .checksum = 0,
    };

    inline constexpr MonitorMode CUSTOM_EDID_MODES[] = {
        {  }, // reserved
        { .width = 1920, .height = 1080, .refreshRate = 60 },
        { .width = 640, .height = 480, .refreshRate = 60 },
        { .width = 800, .height = 600, .refreshRate = 60 },
        { .width = 1024, .height = 768, .refreshRate = 60 },
    };
}
