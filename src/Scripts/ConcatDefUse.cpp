#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

std::string InterpolateColor(double ratio) {
  int red = static_cast<int>(255 * ratio);
  int green = static_cast<int>(255 * (1.0 - ratio));
  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "#%02X%02X00", red, green);
  return std::string(buffer);
}

std::string ReadFile(std::string_view filename) {
  std::ifstream file(filename.data());

  if (!file) {
    throw std::runtime_error("Can't open file " + std::string(filename));
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void ProceedFile(std::string_view filename, std::string nodes_file_content) {
  std::string file_string = ReadFile(filename);

  std::unordered_set<uint64_t> nodes;
  std::regex node_regex(R"(node(\d+))");
  auto begin =
      std::sregex_iterator(file_string.begin(), file_string.end(), node_regex);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    uint64_t val = std::stoull((*it)[1].str());
    nodes.insert(val);
  }

  std::map<uint64_t, uint64_t> values; // node id -> its value
  std::regex edgeRegex(R"(node(\d+)\s+(\d+))");
  std::string line;
  std::stringstream ss(nodes_file_content);
  while (std::getline(ss, line)) {
    std::smatch match;
    if (std::regex_match(line, match, edgeRegex)) {
      uint64_t node_id = std::stoull(match[1].str());
      uint64_t value = std::stoull(match[2].str());
      if (nodes.count(node_id)) {
        values[node_id] = value;
      }
    }
  }

  uint64_t max_value =
      std::max_element(values.begin(), values.end(), [](auto &a, auto &b) {
        return a.second < b.second;
      })->second;

  std::regex color_regex(R"((node(\d+).*?fillcolor=")([^"]*)(".*))");
  std::stringstream out_file_ss{file_string};
  std::string updated_string;

  while (std::getline(out_file_ss, line)) {
    std::smatch match;
    if (std::regex_match(line, match, color_regex)) {
      uint64_t id = std::stoull(match[2].str());
      auto value_it = values.find(id);
      if (value_it != values.end()) {
        updated_string +=
            match[1].str() +
            InterpolateColor(static_cast<double>(value_it->second) /
                             static_cast<double>(max_value)) +
            match[4].str() + "\n";
      } else {
        updated_string += line + "\n";
      }
    } else {
      updated_string += line + "\n";
    }
  }

  std::stringstream out_content;
  out_content << "digraph G {\n"
              << "rankdir=TB;\n";
  out_content << updated_string << "\n";
  out_content << "}\n";

  std::ofstream outFile(filename.data());
  if (!outFile) {
    throw std::runtime_error("Can't open file for writing: " +
                             std::string(filename));
  }
  outFile << out_content.str();
}

void BuildGraph(std::string_view filename) {
  std::system("mkdir -p png");

  std::string command = "dot -Tpng " + std::string(filename) + " -o " +
                        std::string("png/") + std::string(filename) + ".png";
  std::system(command.c_str());
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <edge_file> <prefix>" << std::endl;
    return EXIT_FAILURE;
  }

  std::string edgeFilename = argv[1];
  std::string prefix = argv[2];

  std::string edge_file_content = ReadFile(edgeFilename);

  for (const auto &entry :
       std::filesystem::directory_iterator(std::filesystem::current_path())) {
    if (!entry.is_regular_file())
      continue;

    std::string filename = entry.path().filename().string();
    if (filename.starts_with(prefix)) {
      ProceedFile(filename, edge_file_content);
      BuildGraph(filename);
    }
  }

  return 0;
}
