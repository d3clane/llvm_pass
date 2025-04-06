#ifndef LOG_HPP
#define LOG_HPP

#include <cstdint>

extern "C" {

// TODO: bad code with static destructors
// I need to configure it inside of a main function inside of a pass (i.e check
// if I am in a main and add a start function)
void IncreaseNPasses(uint64_t from_node, uint64_t to_node);
}

#endif // LOG_HPP
