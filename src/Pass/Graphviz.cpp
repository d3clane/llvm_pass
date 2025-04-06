#include "Pass/Graphviz.hpp"

#include <regex>
#include <utility>

namespace dot {

// GraphvizSubgraphBuilder

GraphvizSubgraphBuilder::GraphvizSubgraphBuilder(std::ofstream &out)
    : out_(out) {}

void GraphvizSubgraphBuilder::Start(uint64_t subgraph_id,
                                    std::string_view label) {
  out_ << "subgraph cluster_" << subgraph_id << " {" << "\n";
  std::string copy{label.begin(), label.end()};
  copy = std::regex_replace(copy, std::regex(R"(")"), R"(\\")");

  out_ << "label=\"" << copy << "\";" << "\n";
}

GraphvizSubgraphBuilder::~GraphvizSubgraphBuilder() { out_ << "}" << "\n"; }

GraphvizBuilder::GraphvizBuilder(std::ofstream &&output, bool with_begin,
                                 bool with_end)
    : out_(std::move(output)), nextNodeId_(0), with_end_(with_end) {
  if (!with_begin) {
    return;
  }

  out_ << "digraph G {" << "\n";
  out_ << "rankdir=TB;" << "\n";
}

GraphvizBuilder::~GraphvizBuilder() {
  if (with_end_) {
    out_ << "}" << "\n";
  }
  
  out_.flush();
}

GraphvizBuilder::GraphvizBuilder(GraphvizBuilder &&other)
    : out_(std::move(other.out_)), nextNodeId_(other.nextNodeId_) {}

GraphvizBuilder &GraphvizBuilder::operator=(GraphvizBuilder &&other) {
  out_ = std::move(other.out_);
  nextNodeId_ = other.nextNodeId_;
  return *this;
}

GraphvizSubgraphBuilder GraphvizBuilder::StartSubgraph(uint64_t subgraph_id,
                                                       std::string_view label) {
  GraphvizSubgraphBuilder builder{out_};

  builder.Start(subgraph_id, label);

  return builder;
}

void GraphvizBuilder::AddNode(uint64_t node_id, std::string_view name,
                              Color color) {
  std::string copy{name.begin(), name.end()};
  copy = std::regex_replace(copy, std::regex(R"(")"), R"(\")");

  out_ << "node" << node_id << " [label=\"" << copy << "\", color=\""
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
    std::terminate();
  }
}

} // namespace dot
