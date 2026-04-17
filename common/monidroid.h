#pragma once

#define MONIDROID_SERVICE_VERSION                     "0.1.0"

#define MD_CLASS_PTR_ONLY(Type) \
    Type(const Type&) = delete; \
    Type& operator=(const Type&) = delete;

#define MD_CLASS_NON_COPYABLE(Type) MD_CLASS_PTR_ONLY(Type); \
    Type(Type&&); \
    Type& operator=(Type&&);

namespace Monidroid {
    inline constexpr auto MAX_MONITORS_SUPPORTED    = 16;
}
