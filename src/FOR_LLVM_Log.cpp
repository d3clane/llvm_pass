#include "FOR_LLVM_Log.hpp"

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

  void StoreFrom(uint64_t from_node) { 
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

    for (const auto &[edge, count] : passes_) {
      out << "node" << edge.first << " -> node" << edge.second << " [label=\""
          << count << "\"];" << "\n";
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

void StoreFrom(uint64_t from_node) {
  NPassesLogger::Create().StoreFrom(from_node);
}

void IncreaseNPasses(uint64_t to_node) {
  NPassesLogger::Create().IncreaseNPasses(to_node);
}

void PrintNPassesEdges(const char *out_file_name) {
  NPassesLogger::Create().PrintNPassesEdges(out_file_name);
};
}