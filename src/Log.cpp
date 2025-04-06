#include "Log.hpp"
#include "Util.hpp"

#include <fstream>
#include <map>

namespace {

#if 0
class NPassesLogger {
public:
  // singleton
  static NPassesLogger &Create(std::string_view env_var_name,
                               std::string_view backup_name) {
    static NPassesLogger logger(
        util::OpenFile(env_var_name.data(), backup_name.data()));
    return logger;
  }

  void IncreaseNPasses(uint64_t from_node, uint64_t to_node) {
    passes_[{from_node, to_node}]++;
  }

private:
  NPassesLogger(std::ofstream &&out) : out_(std::move(out)) {}

private:
  std::map<std::pair<uint64_t, uint64_t>, uint64_t> passes_;

  std::ofstream out_;
};

#endif

} // namespace

extern "C" {

void IncreaseNPasses(uint64_t from_node, uint64_t to_node) {
}

}