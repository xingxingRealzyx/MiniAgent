#include "bash.h"

#include <cstdio>
#include <array>
#include <memory>
#include <chrono>
#include <thread>

namespace miniagent {

nlohmann::json BashTool::parameters_schema() const {
    nlohmann::json schema;
    schema["type"] = "object";
    schema["properties"]["command"]["type"] = "string";
    schema["properties"]["command"]["description"] = "The bash command to execute";
    schema["properties"]["timeout"]["type"] = "integer";
    schema["properties"]["timeout"]["description"] = "Timeout in milliseconds (optional, default 60000, max 300000)";
    schema["required"] = {"command"};
    return schema;
}

std::string BashTool::execute(const nlohmann::json& arguments) {
    std::string command = arguments.value("command", "");
    if (command.empty()) {
        return "Error: command is required";
    }

    int timeout_ms = arguments.value("timeout", 60000);
    if (timeout_ms > 300000) timeout_ms = 300000;
    if (timeout_ms < 1000) timeout_ms = 1000;

    // Build command with timeout
    // Use timeout command on macOS/Linux to enforce limit
    std::string full_cmd = command + " 2>&1";

    std::array<char, 4096> buffer;
    std::string result;

    // Open pipe
    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
        return "Error: failed to execute command";
    }

    // Read with timeout
    auto start = std::chrono::steady_clock::now();

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();

        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) {
            pclose(pipe);
            if (!result.empty()) {
                result += "\n[Command timed out after " + std::to_string(timeout_ms) + "ms]";
            } else {
                result = "Command timed out after " + std::to_string(timeout_ms) + "ms";
            }
            return result;
        }
    }

    int exit_code = pclose(pipe);

    if (result.empty()) {
        result = "(no output)";
    }

    if (exit_code != 0) {
        result += "\n[Exit code: " + std::to_string(exit_code) + "]";
    }

    return result;
}

} // namespace miniagent
