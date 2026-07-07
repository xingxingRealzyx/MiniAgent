#include "echo.h"

#include <iostream>

namespace miniagent {

void EchoStreamTool::execute(const nlohmann::json& args) {
    std::string message = args.value("message", "");
    // Single write: execute() runs on the tool's worker thread, and one
    // string insertion won't interleave with the main thread's output
    std::string line = "\n\033[35m📢 [echo] " + message + "\033[0m\n";
    std::cout << line << std::flush;
}

} // namespace miniagent
