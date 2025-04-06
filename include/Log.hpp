#ifndef LOG_HPP
#define LOG_HPP

#include <cstdint>

extern "C" {

void IncreaseNPasses(uint64_t from_node, uint64_t to_node);
void PrintNPassesEdges(const char* out_file_name);

}

#endif // LOG_HPP
