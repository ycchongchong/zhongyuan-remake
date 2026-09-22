#pragma once
#include <filesystem>
#include <system_error>

namespace zhongyuan {
// Replace a save with its closed temporary file in the same directory.
// Never delete the destination before attempting the rename. The caller owns
// temporary-file cleanup on failure. This is not a power-loss durability promise.
std::error_code replace_save_file(const std::filesystem::path &temporary,
                                  const std::filesystem::path &destination);
}
