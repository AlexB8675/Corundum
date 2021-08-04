#include <corundum/detail/file_view.hpp>

#if defined(_WIN32)
    #include <Windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

namespace crd::detail {
    crd_nodiscard FileView make_file_view(const char* path) noexcept {
        FileView file;
#if defined(_WIN32)
        file.handle = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        file.mapping = CreateFileMapping(file.handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
        file.size = GetFileSize(file.handle, nullptr);
        file.data = MapViewOfFile(file.mapping, FILE_MAP_READ, 0, 0, file.size);
#else
        file.handle = open(path, O_RDONLY);
        struct stat file_info;
        crd_assert(fstat(file.handle, &file_info) != -1, "failed to get file info");
        file.data = mmap(nullptr, file_info.st_size, PROT_READ, MAP_PRIVATE, file.handle, 0);
        file.size = file_info.st_size;
#endif
        crd_assert(file.data, "file not found");
        return file;
    }

    void destroy_file_view(FileView& file) noexcept {
#if defined(_WIN32)
        UnmapViewOfFile(file.data);
        CloseHandle(file.handle);
        CloseHandle(file.mapping);
#else
        munmap(const_cast<void*>(file.data), file.size);
        close(file.handle);
#endif
        file = {};
    }
} // namespace crd::detail
