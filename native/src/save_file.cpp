#include "zhongyuan/save_file.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace zhongyuan {
#ifdef _WIN32
namespace {
std::wstring windows_path(const std::filesystem::path &path, std::error_code &error) {
    auto absolute = std::filesystem::absolute(path, error);
    if (error) return {};
    auto name = absolute.lexically_normal().make_preferred().native();
    if (name.compare(0, 4, L"\\\\?\\") == 0) return name;
    if (name.compare(0, 2, L"\\\\") == 0) return L"\\\\?\\UNC\\" + name.substr(2);
    return L"\\\\?\\" + name;
}
}
#endif

std::error_code replace_save_file(const std::filesystem::path &temporary,
                                  const std::filesystem::path &destination) {
    std::error_code error;
#ifdef _WIN32
    const auto source = windows_path(temporary, error);
    if (error) return error;
    const auto target = windows_path(destination, error);
    if (error) return error;
    // Godot 4.5.2 DirAccessWindows::rename removes an existing target first.
    // MoveFileExW performs replacement without that destructive intermediate step.
    if (!MoveFileExW(source.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        error = std::error_code(GetLastError(), std::system_category());
#else
    std::filesystem::rename(temporary, destination, error);
#endif
    return error;
}
}
