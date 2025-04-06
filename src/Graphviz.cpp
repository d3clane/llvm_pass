#include "Graphviz.hpp"
#include <utility>

namespace dot {

// GraphvizSubgraphBuilder

GraphvizSubgraphBuilder::GraphvizSubgraphBuilder(std::ofstream &out)
    : out_(out) {}

void GraphvizSubgraphBuilder::Start(uint64_t subgraph_id,
                                    std::string_view label) {
  out_ << "subgraph cluster_" << subgraph_id << " {" << "\n";
  out_ << "label=\"" << label << "\";" << "\n";
}

GraphvizSubgraphBuilder::~GraphvizSubgraphBuilder() { out_ << "}" << "\n"; }

GraphvizBuilder::GraphvizBuilder(std::ofstream &&output)
    : out_(std::move(output)), nextNodeId_(0) {
  out_ << "digraph G {" << "\n";
  out_ << "rankdir=TB;" << "\n";
}

GraphvizBuilder::~GraphvizBuilder() {
  out_ << "}" << "\n";
  out_.flush();
}

GraphvizSubgraphBuilder
GraphvizBuilder::StartSubgraph(uint64_t subgraph_id,
                               std::string_view label) {
  GraphvizSubgraphBuilder builder{out_};

  builder.Start(subgraph_id, label);

  return builder;
}

void GraphvizBuilder::AddNode(uint64_t node_id, std::string_view name,
                              Color color) {
  out_ << "node" << node_id << " [label=\"" << name << "\", color=\""
       << ColorToString(color) << "\"];" << std::endl;
}

void GraphvizBuilder::AddEdge(uint64_t from_node, uint64_t to_node,
                              Color color) {
  out_ << "node" << from_node << " -> node" << to_node << " [color=\""
       << ColorToString(color) << "\"];"
       << "\n";
}

const char *GraphvizBuilder::ColorToString(Color color) {
  switch (color) {
  case Color::Red:
    return "red";
  case Color::Green:
    return "green";
  case Color::Blue:
    return "blue";
  case Color::Black:
    return "black";
  default:
    return "black";
  }
}

} // namespace dot
