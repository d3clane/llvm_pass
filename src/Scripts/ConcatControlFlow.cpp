#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

std::string ReadFile(std::string_view filename) {
  std::ifstream file(filename.data());

  if (!file) {
    throw std::runtime_error("Can't open file " + std::string(filename));
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void ProceedFile(std::string_view filename, std::string edges_file_content) {
  std::string file_string = ReadFile(filename.data());

  std::unordered_set<uint64_t> nodes;
  std::regex node_regex(R"(.*node(\d+).*)");

  auto begin =
      std::sregex_iterator(file_string.begin(), file_string.end(), node_regex);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    uint64_t val = std::stoull(it->str(1));
    nodes.insert(val);
  }

  std::vector<std::string> valid_lines;
  std::string line;

  std::regex edgeRegex(R"(node(\d+).*->.*node(\d+).*)");

  std::stringstream edges_file{edges_file_content};
  while (std::getline(edges_file, line)) {
    std::smatch match;
    if (std::regex_match(line, match, edgeRegex)) {
      auto num1 = std::stoull(match[1]);
      auto num2 = std::stoull(match[2]);
      if (nodes.count(num1) && nodes.count(num2)) {
        valid_lines.push_back(line);
      }
    }
  }

  std::stringstream out_content;
  out_content << "digraph G {\n" << "rankdir=TB;\n";
  out_content << file_string << "\n";

  for (const auto &line : valid_lines) {
    out_content << line << "\n";
  }

  out_content << "}\n";

  std::ofstream outFile(filename.data());
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
