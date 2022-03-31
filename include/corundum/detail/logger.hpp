#pragma once

#include <corundum/detail/macros.hpp>

#include <iomanip>
#include <sstream>
#include <string>
#include <chrono>
#include <cstdio>

namespace crd {
    namespace dtl {
        template <typename... Args>
        void log(const char* sender, const char* severity, const char* type, const char* message, Args&&... args) noexcept {
    #if defined(crd_debug_logging)
            const auto clock = std::chrono::system_clock::now();
            const auto time  = std::chrono::system_clock::to_time_t(clock);
            auto timestamp = std::stringstream();
            timestamp << std::put_time(std::localtime(&time), "%H:%M:%S");
            auto size = std::snprintf(nullptr, 0, message, std::forward<Args>(args)...);
            std::string buffer(size + 1, '\0');
            std::snprintf(buffer.data(), buffer.size(), message, std::forward<Args>(args)...);
            std::printf("[%s] [%s] [%s] [%s]: %s\n", timestamp.str().c_str(), sender, severity, type, buffer.c_str());
    #endif
        }
    } // namespace crd::dtl

    enum Severity {
        severity_verbose = 1 << 0,
        severity_info    = 1 << 1,
        severity_warning = 1 << 2,
        severity_error   = 1 << 3,
    };

    enum Type {
        type_general     = 1 << 0,
        type_validation  = 1 << 1,
        type_performance = 1 << 2,
    };

    template <typename... Args>
    void log(const char* sender, Severity severity, Type type, const char* message, Args&&... args) noexcept {
#if defined(crd_debug_logging)
        const char* severity_string;
        switch (severity) {
            case severity_verbose: severity_string = "Verbose"; break;
            case severity_info:    severity_string = "Info";    break;
            case severity_warning: severity_string = "Warning"; break;
            case severity_error:   severity_string = "Error";   break;
        }
        crd_assert(severity_string, "severity_string empty (?)");
        const char* type_string;
        switch (type) {
            case type_general:     type_string = "General";     break;
            case type_validation:  type_string = "Validation";  break;
            case type_performance: type_string = "Performance"; break;
        }
        crd_assert(type_string, "type_string empty (?)");
        if (severity >= crd_log_level) {
            dtl::log(sender, severity_string, type_string, message, std::forward<Args>(args)...);
        }
#endif
    }
} // namespace crd
