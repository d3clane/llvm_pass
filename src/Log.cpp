#include "Log.hpp"

#include <cassert>
#include <fstream>
#include <map>

namespace {

class NPassesLogger {
public:
  // singleton
  static NPassesLogger &Create_NO_LLVM_INSTRUMENT() {
    static NPassesLogger logger;
    return logger;
  }

  void IncreaseNPasses_NO_LLVM_INSTRUMENT(uint64_t from_node, uint64_t to_node) {
    passes_[{from_node, to_node}]++;
  }

  void PrintNPassesEdges_NO_LLVM_INSTRUMENT(const char *out_file_name) {
    assert(out_file_name);
    std::ofstream out{out_file_name};

    for (const auto &[edge, count] : passes_) {
      out << "node" << edge.first << " -> node" << edge.second << " [label=\""
          << count << "\"];" << "\n";
    }
  }

private:
  NPassesLogger() = default;

private:
  std::map<std::pair<uint64_t, uint64_t>, uint64_t> passes_;
};

} // namespace

extern "C" {

void IncreaseNPasses(uint64_t from_node, uint64_t to_node) {
  NPassesLogger::Create_NO_LLVM_INSTRUMENT().IncreaseNPasses_NO_LLVM_INSTRUMENT(from_node, to_node);
}

void PrintNPassesEdges(const char *out_file_name) {
  NPassesLogger::Create_NO_LLVM_INSTRUMENT().PrintNPassesEdges_NO_LLVM_INSTRUMENT(out_file_name);
};

}