#include "zhongyuan/save_file.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;
namespace {
int checks = 0;
void check(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
    ++checks;
    std::cout << "PASS: " << message << '\n';
}
void write(const fs::path &path, const std::string &bytes) {
    std::ofstream file(path, std::ios::binary);
    file << bytes;
    file.close();
    if (!file) throw std::runtime_error("write fixture failed");
}
std::string read(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("read fixture failed");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
struct Directory {
    fs::path path = fs::temp_directory_path() / ("zhongyuan-save-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Directory() { fs::create_directory(path); }
    ~Directory() { std::error_code ignored; fs::remove_all(path, ignored); }
};
#ifdef _WIN32
struct LockedFile {
    HANDLE handle;
    explicit LockedFile(const fs::path &path) : handle(CreateFileW(path.c_str(),
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr)) {
        if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("lock fixture failed");
    }
    ~LockedFile() { CloseHandle(handle); }
};
#endif
}

int main() {
    try {
        Directory directory;
        const auto destination = directory.path / fs::u8path(u8"槽一.json");
        const auto temporary = directory.path / fs::u8path(u8"槽一.json.tmp");
        const std::string previous = "old valid save", next = "new valid save";
        write(temporary, previous);
        check(!zhongyuan::replace_save_file(temporary, destination), "create save with Unicode path");
        check(read(destination) == previous && !fs::exists(temporary), "creation moves complete bytes");
        check(bool(zhongyuan::replace_save_file(temporary, destination)), "missing source reports failure");
        check(read(destination) == previous, "missing source never deletes existing save");
        write(temporary, next);
#ifdef _WIN32
        {
            LockedFile source_lock(temporary);
            check(bool(zhongyuan::replace_save_file(temporary, destination)), "locked temporary file rejects replacement");
            check(read(destination) == previous && read(temporary) == next, "source sharing violation preserves both files");
        }
        {
            LockedFile target_lock(destination);
            check(bool(zhongyuan::replace_save_file(temporary, destination)), "locked save rejects replacement");
            check(read(destination) == previous && read(temporary) == next, "destination sharing violation preserves both files");
        }
#endif
        check(!zhongyuan::replace_save_file(temporary, destination), "overwrite existing save succeeds after locks close");
        check(read(destination) == next && !fs::exists(temporary), "successful overwrite leaves only new save");
        const auto blocked = directory.path / "blocked";
        fs::create_directory(blocked);
        write(blocked / "sentinel", previous);
        write(temporary, next);
        check(bool(zhongyuan::replace_save_file(temporary, blocked)), "directory destination reports failure");
        check(read(blocked / "sentinel") == previous && read(temporary) == next, "failed replacement preserves destination and temporary file");
        std::cout << "SAVE FILE: " << checks << " checks, 0 failures\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
