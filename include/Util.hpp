#ifndef UTIL_HPP
#define UTIL_HPP

#include <fstream>

namespace util {

std::ofstream OpenFile(const char *env_var_to_take_name,
                       const char *backup_name);

} // namespace util

#endif // UTIL_HPP