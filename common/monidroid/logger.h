#pragma once

#include <iostream>
#include <format>
#include <chrono>

namespace Monidroid {
    template <typename... Args>
    void DefaultLog(std::format_string<Args...> fmt, Args&&... args) {
        auto systemNow = std::chrono::system_clock::now();
        auto now = std::chrono::floor<std::chrono::seconds>(systemNow);
        
        std::cout
            << std::format("[{}, Monidroid] ", now)
            << std::format(fmt, std::forward<Args>(args)...)
            << std::endl;
    }

    template <typename... Args>
    void TaggedLog(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
        auto systemNow = std::chrono::system_clock::now();
        auto now = std::chrono::floor<std::chrono::seconds>(systemNow);

        std::cout
            << std::format("[{}, {}] ", now, tag)
            << std::format(fmt, std::forward<Args>(args)...)
            << std::endl;
    }
} // namespace Monidroid
