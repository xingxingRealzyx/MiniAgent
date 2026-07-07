#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace miniagent {

// OpenAI function call format
struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;  // JSON string of function arguments
};

// A message in the conversation (OpenAI Chat Completions format)
struct Message {
    std::string role;     // "system" | "user" | "assistant" | "tool"
    std::string content;  // text content (may be empty for tool_calls / tool results)
    std::string tool_call_id;  // for role="tool" — which tool call this result answers
    std::vector<ToolCall> tool_calls;  // for role="assistant" with function calls

    // Convenience: create a simple text message
    static Message system(const std::string& text) {
        Message m;
        m.role = "system";
        m.content = text;
        return m;
    }

    static Message user(const std::string& text) {
        Message m;
        m.role = "user";
        m.content = text;
        return m;
    }

    static Message assistant(const std::string& text) {
        Message m;
        m.role = "assistant";
        m.content = text;
        return m;
    }

    static Message assistant_tool_calls(const std::vector<ToolCall>& calls,
                                        const std::string& text = "") {
        Message m;
        m.role = "assistant";
        m.content = text;
        m.tool_calls = calls;
        return m;
    }

    static Message tool_result(const std::string& call_id, const std::string& result) {
        Message m;
        m.role = "tool";
        m.tool_call_id = call_id;
        m.content = result;
        return m;
    }
};

// Parsed streaming event from OpenAI SSE
enum class StreamEventType {
    TEXT_DELTA,         // content delta chunk
    TOOL_CALL_DELTA,    // tool_call chunk (name, arguments delta, or id)
    FINISH,             // finish_reason received
    ERROR               // error event
};

struct StreamEvent {
    StreamEventType type;
    std::string text;            // text delta or tool_call argument delta
    int tool_call_index = -1;    // which tool_call this delta belongs to (0-based)
    std::string tool_call_id;    // tool call id
    std::string tool_name;       // tool function name
    std::string finish_reason;   // "stop", "tool_calls", "length", etc.
};

// Result returned by LLMClient::chat()
struct ChatResult {
    std::string content;              // accumulated text (may be empty)
    std::vector<ToolCall> tool_calls; // tool calls (may be empty)
    bool success = false;
    std::string error_message;
};

} // namespace miniagent
