#pragma once

#include <corundum/detail/macros.hpp>

#include <cstddef>

namespace crd::dtl {
    struct FileView {
#if defined(_WIN32)
        void* handle;
        void* mapping;
#else
        int handle;
#endif
        const void* data;
        std::size_t size;
    };

    crd_nodiscard FileView make_file_view(const char*) noexcept;
                  void     destroy_file_view(FileView&) noexcept;
} // namespace crd::dtl
