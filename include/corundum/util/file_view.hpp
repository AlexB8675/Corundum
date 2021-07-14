#pragma once

#include <corundum/util/macros.hpp>

#include <cstddef>

namespace crd::util {
    struct FileView {
#if defined(_WIN32)
        void* handle;
        void* mapping;
#endif
        const void* data;
        std::size_t size;
    };

    crd_nodiscard crd_module FileView make_file_view(const char*) noexcept;
                  crd_module void     destroy_file_view(FileView&) noexcept;
} // namespace crd::util
