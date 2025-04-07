#ifndef LOG_HPP
#define LOG_HPP

#include <cstdint>

extern "C" {

// One-shot
void PrepareIncreasePasses(uint64_t from_node);
void IncreaseNPasses(uint64_t to_node); // from have to be prepared
void PrintNPassesEdges(const char* out_file_name);

void AddUsage(uint64_t node);
void PrintUsages(const char* out_file_name);

}

#endif // LOG_HPP
