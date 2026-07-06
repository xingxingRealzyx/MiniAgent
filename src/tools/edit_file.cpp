#include "edit_file.h"

#include <fstream>
#include <sstream>

namespace miniagent {

nlohmann::json EditFileTool::parameters_schema() const {
    nlohmann::json schema;
    schema["type"] = "object";
    schema["properties"]["file_path"]["type"] = "string";
    schema["properties"]["file_path"]["description"] = "Absolute path to the file to modify";
    schema["properties"]["old_string"]["type"] = "string";
    schema["properties"]["old_string"]["description"] = "The text to replace";
    schema["properties"]["new_string"]["type"] = "string";
    schema["properties"]["new_string"]["description"] = "The text to replace it with (must be different from old_string)";
    schema["required"] = {"file_path", "old_string", "new_string"};
    return schema;
}

std::string EditFileTool::execute(const nlohmann::json& arguments) {
    std::string file_path = arguments.value("file_path", "");
    std::string old_str = arguments.value("old_string", "");
    std::string new_str = arguments.value("new_string", "");

    if (file_path.empty()) return "Error: file_path is required";
    if (old_str.empty()) return "Error: old_string is required";
    if (old_str == new_str) return "Error: old_string and new_string must be different";

    // Read the file
    std::ifstream in(file_path);
    if (!in.is_open()) {
        return "Error: cannot open file: " + file_path;
    }

    std::ostringstream oss;
    oss << in.rdbuf();
    in.close();
    std::string content = oss.str();

    // Find and replace (exact match, first occurrence only)
    size_t pos = content.find(old_str);
    if (pos == std::string::npos) {
        return "Error: old_string not found in file. Make sure the string matches exactly, including whitespace.";
    }

    // Check uniqueness
    size_t second = content.find(old_str, pos + 1);
    if (second != std::string::npos) {
        return "Error: old_string appears more than once in the file. "
               "Please provide a larger string with more surrounding context to make it unique.";
    }

    content.replace(pos, old_str.size(), new_str);

    // Write back
    std::ofstream out(file_path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return "Error: cannot write to file: " + file_path;
    }

    out << content;
    out.close();

    if (out.fail()) {
        return "Error: write failed for file: " + file_path;
    }

    return "Successfully edited " + file_path + ": replaced 1 occurrence.";
}

} // namespace miniagent
