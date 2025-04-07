#include "Pass/FOR_LLVM_Log.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

namespace {

std::string InterpolateColor(double ratio) {
  int red = static_cast<int>(255 * ratio);
  int green = static_cast<int>(255 * (1.0 - ratio));
  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "#%02X%02X00", red, green);
  return std::string(buffer);
}

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
      assert(ratio <= 1);

      int red = static_cast<int>(std::min(255.0, 255.0 * ratio));
      int green = static_cast<int>(std::min(255.0, 255.0 * (1 - ratio)));

      out << "node" << edge.first << " -> node" << edge.second << " [label=\""
          << count << "\", color=\"" << InterpolateColor(ratio)
          << "\", penwidth=" << std::dec << (1 + 4 * ratio) << "];" << "\n";
    }
  }

private:
  NPassesLogger() = default;

private:
  std::map<std::pair<uint64_t, uint64_t>, uint64_t> passes_;

  uint64_t from_;
  bool invalid{true};
};

class NodesUsageCounter {
public:
  // singleton
  static NodesUsageCounter &Create() {
    static NodesUsageCounter counter;
    return counter;
  }

  void AddUsage(uint64_t node) { counter_[node]++; }

  void PrintUsages(const char *out_file_name) {
    assert(out_file_name);
    std::ofstream out{out_file_name};

    for (const auto &[node, counter] : counter_) {
      out << "node" << node << " " << counter << "\n";
    }
  }

private:
  NodesUsageCounter() = default;

private:
  std::map<uint64_t, uint64_t> counter_;
};

class MemoryTracker {
public:
  // singleton
  static MemoryTracker &Create() {
    static MemoryTracker tracker;
    return tracker;
  }

  void AddDynMemCreation(uint64_t node, void *mem) {
    assert(last_node_.count(mem) == 0);

    last_node_[mem] = node;
    history_[mem].push_back(node);
  }

  void LogMemIfDyn(uint64_t node, void *mem) {
    if (last_node_.count(mem) == 0) {
      return;
    }

    last_node_[mem] = node;
    history_[mem].push_back(node);
  }

  void RemoveDynMem(uint64_t node, void *mem) {
    auto last_node_it = last_node_.find(mem);

    assert(last_node_it != last_node_.end());

    last_node_.erase(last_node_.find(mem));
    history_[mem].push_back(node);
    history_[mem].push_back(kHistoryNodesDelimeter);
  }

  void Print(const char *out_file_name) {
    assert(out_file_name);

    std::ofstream out{out_file_name};

    for (auto &[mem, history] : history_) {
      for (ssize_t i = 0; i < history.size() - 1; ++i) {
        if (history[i + 1] == kHistoryNodesDelimeter) {
          continue;
        }

        out << "node" << history[i] << " -> " << "node" << history[i + 1]
            << " [color=\"black\"];\n";
      }
    }
  }

private:
  MemoryTracker() = default;

private:
  std::map<void *, uint64_t> last_node_;
  std::map<void *, std::vector<uint64_t>> history_;

  // it is used to delimit usage of memory with the same address
  // but allocated further in the program by another to call to allocator
  static constexpr uint64_t kHistoryNodesDelimeter = static_cast<uint64_t>(-1);
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
}

void AddUsage(uint64_t node) { NodesUsageCounter::Create().AddUsage(node); }

void PrintUsages(const char *out_file_name) {
  NodesUsageCounter::Create().PrintUsages(out_file_name);
}

void AddDynamicallyAllocatedMemory(uint64_t node, void *memory) {
  MemoryTracker::Create().AddDynMemCreation(node, memory);
}

void LogIfMemoryIsDynamicallyAllocated(uint64_t node, void *memory) {
  MemoryTracker::Create().LogMemIfDyn(node, memory);
}

void RemoveDynamicallAllocatedMemory(uint64_t node, void *memory) {
  MemoryTracker::Create().RemoveDynMem(node, memory);
}

void PrintAllocatedMemoryInfo(const char *out_file_name) {
  MemoryTracker::Create().Print(out_file_name);
}
}