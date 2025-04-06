#ifndef LOG_HPP
#define LOG_HPP

#include <cstdint>

extern "C" {

// One-shot
void StoreFrom(uint64_t from_node);
void IncreaseNPasses(uint64_t to_node); // from have to be stored
void PrintNPassesEdges(const char* out_file_name);

}

#endif // LOG_HPP
