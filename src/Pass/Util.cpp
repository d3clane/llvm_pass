#include "Pass/Util.hpp"

#include <stdexcept>
#include <cstdlib>

namespace util {

std::ofstream OpenFile(const char *env_var_name, const char *backup_name) {
  const char* filename = nullptr;

  if (env_var_name) {
    filename = std::getenv(env_var_name);
  }

  if (!filename) {
    filename = backup_name;
  }

  auto out = std::ofstream{filename};
  if (!out) {
    throw std::runtime_error{"Can't open graphviz out file"};
  }

  return out;
}

} // namespace util
