#include "Pass/FOR_LLVM_Log.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <map>

namespace {

class NPassesLogger {
public:
  // singleton
  static NPassesLogger &Create() {
    static NPassesLogger logger;
    return logger;
  }

  void PrepareIncreasePasses(uint64_t from_node) {
    from_ = from_node;
    invalid = false;
  }

  void IncreaseNPasses(uint64_t to_node) {
    if (invalid) {
      return;
    }

    passes_[{from_, to_node}]++;
    from_ = 0;
    invalid = true;
  }

  void PrintNPassesEdges(const char *out_file_name) {
    assert(out_file_name);
    std::ofstream out{out_file_name};

    uint64_t max_passes =
        std::max_element(passes_.begin(), passes_.end(), [](auto &a, auto &b) {
          return a.second < b.second;
        })->second;

    for (const auto &[edge, count] : passes_) {
      double ratio = (double)count / max_passes;

      int red = static_cast<int>(std::min(255.0, 255.0 * ratio));
      int green = static_cast<int>(std::min(255.0, 255.0 * (1 - ratio)));

      out << "node" << edge.first << " -> node" << edge.second << " [label=\""
          << count << "\", color=\"#" << std::setfill('0') << std::setw(2)
          << std::hex << red << std::setw(2) << std::hex << green << "00\"];"
          << "\n" << std::dec;
    }
  }

private:
  NPassesLogger() = default;

private:
  std::map<std::pair<uint64_t, uint64_t>, uint64_t> passes_;

  uint64_t from_;
  bool invalid{true};
};

} // namespace

extern "C" {

void PrepareIncreasePasses(uint64_t from_node) {
  NPassesLogger::Create().PrepareIncreasePasses(from_node);
}

void IncreaseNPasses(uint64_t to_node) {
  NPassesLogger::Create().IncreaseNPasses(to_node);
}

void PrintNPassesEdges(const char *out_file_name) {
  NPassesLogger::Create().PrintNPassesEdges(out_file_name);
};
}