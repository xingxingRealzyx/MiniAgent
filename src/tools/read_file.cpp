#include "read_file.h"

#include <fstream>
#include <sstream>
#include <iomanip>

namespace miniagent {

nlohmann::json ReadFileTool::parameters_schema() const {
    nlohmann::json schema;
    schema["type"] = "object";
    schema["properties"]["file_path"]["type"] = "string";
    schema["properties"]["file_path"]["description"] = "Absolute path to the file to read";
    schema["properties"]["offset"]["type"] = "integer";
    schema["properties"]["offset"]["description"] = "Line number to start reading from (1-based, optional)";
    schema["properties"]["limit"]["type"] = "integer";
    schema["properties"]["limit"]["description"] = "Maximum number of lines to read (optional, default 2000)";
    schema["required"] = {"file_path"};
    return schema;
}

std::string ReadFileTool::execute(const nlohmann::json& arguments) {
    std::string file_path = arguments.value("file_path", "");
    if (file_path.empty()) {
        return "Error: file_path is required";
    }

    std::ifstream file(file_path);
    if (!file.is_open()) {
        return "Error: cannot open file: " + file_path;
    }

    int offset = arguments.value("offset", 1);
    int limit = arguments.value("limit", 2000);
    if (offset < 1) offset = 1;

    std::ostringstream result;
    std::string line;
    int line_num = 0;
    int lines_read = 0;

    while (std::getline(file, line)) {
        line_num++;
        if (line_num < offset) continue;
        if (lines_read >= limit) break;

        result << std::setw(6) << std::right << line_num << "\t" << line << "\n";
        lines_read++;
    }

    if (lines_read == 0) {
        result << "(file empty or offset beyond end of file)";
    }

    return result.str();
}

} // namespace miniagent
