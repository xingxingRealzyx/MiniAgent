#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace miniagent {

// Abstract base class for tools
class Tool {
public:
    virtual ~Tool() = default;

    // OpenAI function definition
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual nlohmann::json parameters_schema() const = 0;

    // Execute the tool, returns the result string
    virtual std::string execute(const nlohmann::json& arguments) = 0;

    // Build the OpenAI-format tool definition
    nlohmann::json to_openai_schema() const {
        nlohmann::json tool;
        tool["type"] = "function";
        tool["function"]["name"] = name();
        tool["function"]["description"] = description();
        tool["function"]["parameters"] = parameters_schema();
        return tool;
    }
};

// Registry of available tools
class ToolRegistry {
public:
    void register_tool(std::unique_ptr<Tool> tool);

    // Get all tools in OpenAI format
    nlohmann::json tools_schema() const;

    // Execute a tool by name, return JSON string result
    // Returns JSON: {"success": true/false, "result": "...", "error": "..."}
    std::string execute(const std::string& name, const nlohmann::json& arguments);

    // Check if a tool exists
    bool has(const std::string& name) const;

    // Get tool count
    size_t count() const { return tools_.size(); }

private:
    std::vector<std::unique_ptr<Tool>> tools_;
    std::unordered_map<std::string, Tool*> tool_map_;
};

} // namespace miniagent
