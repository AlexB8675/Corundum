#pragma once

#include <corundum/detail/macros.hpp>

#include <iomanip>
#include <sstream>
#include <string>
#include <chrono>
#include <cstdio>

namespace crd::detail {
    enum class Severity {
        eInfo    = 1 << 0,
        eWarning = 1 << 1,
        eError   = 1 << 2,
    };

    enum class Type {
        eGeneral     = 1 << 0,
        eValidation  = 1 << 1,
        ePerformance = 1 << 2,
    };

    template <typename... Args>
    void log(const char* sender, const char* severity, const char* type, const char* message, Args&&... args) noexcept {
#if defined(crd_debug_logging)
        const auto clock     = std::chrono::system_clock::now();
        const auto time      = std::chrono::system_clock::to_time_t(clock);
              auto timestamp = std::stringstream();
                   timestamp << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        const auto size      = std::snprintf(nullptr, 0, message, std::forward<Args>(args)...);
        std::string buffer(size + 1, '\0');
        std::snprintf(buffer.data(), buffer.size(), message, std::forward<Args>(args)...);
        std::printf("[%s] [%s] [%s] [%s]: %s\n", timestamp.str().c_str(), sender, severity, type, buffer.c_str());
#endif
    }

    template <typename... Args>
    void log(const char* sender, Severity severity, Type type, const char* message, Args&&... args) noexcept {
#if defined(crd_debug_logging)
        const char* severity_string;
        switch (severity) {
            case Severity::eInfo:    severity_string = "Info";    break;
            case Severity::eWarning: severity_string = "Warning"; break;
            case Severity::eError:   severity_string = "Error";   break;
        }
        const char* type_string;
        switch (type) {
            case Type::eGeneral:     type_string = "General";     break;
            case Type::eValidation:  type_string = "Validation";  break;
            case Type::ePerformance: type_string = "Performance"; break;
        }
        log(sender, severity_string, type_string, message, std::forward<Args>(args)...);
#endif
    }
} // namespace crd::detail
