#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
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

void ProceedFile(std::string file1_input, std::string file2_input,
                std::string out_name) {
  std::vector<std::string> vec_file1_lines;
  std::set<std::string> set_file1_nodes;

  std::regex re_file1("^\\s*(node\\d+)\\s*->\\s*(node\\d+).*");

  std::stringstream stream_file1(file1_input);

  std::string str_line;
  while (std::getline(stream_file1, str_line)) {
    vec_file1_lines.push_back(str_line);
    std::smatch match_result;
    if (std::regex_match(str_line, match_result, re_file1)) {
      set_file1_nodes.insert(match_result[1].str());
      set_file1_nodes.insert(match_result[2].str());
    }
  }

  std::vector<std::string> vec_file2_lines;
  std::set<std::string> set_file2_nodes;
  std::regex re_file2("^\\s*(node\\d+).*");

  std::stringstream stream_file2(file2_input);

  while (std::getline(stream_file2, str_line)) {
    std::smatch match_result;
    if (std::regex_match(str_line, match_result, re_file2)) {
      std::string str_node = match_result[1].str();
      if (set_file1_nodes.find(str_node) != set_file1_nodes.end()) {
        vec_file2_lines.push_back(str_line);
        set_file2_nodes.insert(str_node);
      }
    } else {
      vec_file2_lines.push_back(str_line);
    }
  }

  std::vector<std::string> vec_filtered_file1_lines;
  for (const auto &str_current_line : vec_file1_lines) {
    std::smatch match_result;
    if (std::regex_match(str_current_line, match_result, re_file1)) {
      std::string str_node1 = match_result[1].str();
      std::string str_node2 = match_result[2].str();
      if (set_file2_nodes.find(str_node1) != set_file2_nodes.end() &&
          set_file2_nodes.find(str_node2) != set_file2_nodes.end()) {
        vec_filtered_file1_lines.push_back(str_current_line);
      }
    }
  }

  std::ofstream stream_output(out_name);
  if (!stream_output) {
    throw std::runtime_error{"stream output opening error"};
  }

  stream_output << "digraph G{\n";
  stream_output << "rankdir=TB;\n";

  for (const auto &str_current_line : vec_file2_lines) {
    stream_output << str_current_line << "\n";
  }

  for (const auto &str_current_line : vec_filtered_file1_lines) {
    stream_output << str_current_line << "\n";
  }

  stream_output << "}\n";
}

void BuildGraph(std::string_view filename) {
  std::system("mkdir -p png");

  std::string command = "dot -Tpng " + std::string(filename) + " -o " +
                        std::string("png/") + std::string(filename) + ".png";
  std::system(command.c_str());
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0] << " <edge_file> <prefix> <out_file_name>"
              << std::endl;
    return EXIT_FAILURE;
  }

  std::string edge_filename = argv[1];
  std::string prefix = argv[2];
  std::string out_file_name = argv[3];

  std::string edge_file_content = ReadFile(edge_filename);

  for (const auto &entry :
       std::filesystem::directory_iterator(std::filesystem::current_path())) {
    if (!entry.is_regular_file())
      continue;

    std::string filename = entry.path().filename().string();
    if (filename.starts_with(prefix)) {
      std::string out_dot = out_file_name + filename + ".dot";
      std::string file_input = ReadFile(filename);

      ProceedFile(edge_file_content, file_input, out_dot);
      BuildGraph(out_dot);
    }
  }

  return 0;
}