#ifndef GRAPHVIZ_H
#define GRAPHVIZ_H

#include <fstream>
#include <string_view>

namespace dot {

class GraphvizBuilder;

class GraphvizSubgraphBuilder {
  friend class GraphvizBuilder;

public:
  // Movable
  GraphvizSubgraphBuilder(GraphvizSubgraphBuilder &&) = default;
  GraphvizSubgraphBuilder &operator=(GraphvizSubgraphBuilder &&) = delete;

  // Non-copyable
  GraphvizSubgraphBuilder(const GraphvizSubgraphBuilder &) = delete;
  GraphvizSubgraphBuilder &operator=(const GraphvizSubgraphBuilder &) = delete;

  ~GraphvizSubgraphBuilder();

private:
  explicit GraphvizSubgraphBuilder(std::ofstream &out);

  void Start(uint64_t subgraph_id, std::string_view label);

private:
  std::ofstream &out_;
};

class GraphvizBuilder {
public:
  enum class Color {
    Red,
    Green,
    Blue,
    Black,
    Gray,
  };

  GraphvizBuilder(std::ofstream &&output, bool with_begin = true,
                  bool with_end = true);

  // Movable

  GraphvizBuilder(GraphvizBuilder &&);
  GraphvizBuilder &operator=(GraphvizBuilder &&);

  // Non-copyable
  GraphvizBuilder(const GraphvizBuilder &) = delete;
  GraphvizBuilder &operator=(const GraphvizBuilder &) = delete;

  ~GraphvizBuilder();

  [[nodiscard]]
  GraphvizSubgraphBuilder StartSubgraph(uint64_t subgraph_id,
                                        std::string_view label);

  void AddNode(uint64_t node_id, std::string_view name,
               Color color = Color::Gray);
  void AddEdge(uint64_t from_node, uint64_t to_node, Color color);

private:
  static const char *ColorToString(Color color);

private:
  std::ofstream out_;
  int nextNodeId_;
  
  bool with_end_{true};
};

} // namespace dot

#endif // GRAPHVIZ_H
