#include "write_file.h"

#include <fstream>

namespace miniagent {

nlohmann::json WriteFileTool::parameters_schema() const {
    nlohmann::json schema;
    schema["type"] = "object";
    schema["properties"]["file_path"]["type"] = "string";
    schema["properties"]["file_path"]["description"] = "Absolute path to the file to write";
    schema["properties"]["content"]["type"] = "string";
    schema["properties"]["content"]["description"] = "The content to write to the file";
    schema["required"] = {"file_path", "content"};
    return schema;
}

std::string WriteFileTool::execute(const nlohmann::json& arguments) {
    std::string file_path = arguments.value("file_path", "");
    std::string content = arguments.value("content", "");

    if (file_path.empty()) {
        return "Error: file_path is required";
    }

    std::ofstream file(file_path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return "Error: cannot write to file: " + file_path;
    }

    file << content;
    file.close();

    if (file.fail()) {
        return "Error: write failed for file: " + file_path;
    }

    return "Successfully wrote " + std::to_string(content.size()) + " bytes to " + file_path;
}

} // namespace miniagent
