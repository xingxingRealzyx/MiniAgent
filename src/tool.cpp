#include "tool.h"

#include <iostream>

namespace miniagent {

void ToolRegistry::register_tool(std::unique_ptr<Tool> tool) {
    std::string n = tool->name();

    // Re-registering the same name replaces the existing tool
    auto it = tool_map_.find(n);
    if (it != tool_map_.end()) {
        for (auto& t : tools_) {
            if (t.get() == it->second) {
                t = std::move(tool);
                it->second = t.get();
                return;
            }
        }
    }

    tool_map_[n] = tool.get();
    tools_.push_back(std::move(tool));
}

nlohmann::json ToolRegistry::tools_schema() const {
    auto arr = nlohmann::json::array();
    for (const auto& tool : tools_) {
        arr.push_back(tool->to_openai_schema());
    }
    return arr;
}

bool ToolRegistry::has(const std::string& name) const {
    return tool_map_.find(name) != tool_map_.end();
}

std::string ToolRegistry::execute(const std::string& name, const nlohmann::json& arguments) {
    nlohmann::json result;

    auto it = tool_map_.find(name);
    if (it == tool_map_.end()) {
        result["success"] = false;
        result["error"] = "Unknown tool: " + name;
        return result.dump();
    }

    try {
        std::string output = it->second->execute(arguments);
        result["success"] = true;
        result["result"] = output;
    } catch (const std::exception& e) {
        result["success"] = false;
        result["error"] = std::string("Tool execution error: ") + e.what();
    }

    return result.dump();
}

} // namespace miniagent
