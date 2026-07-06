#include "agent.h"

#include <iostream>
#include <sstream>

namespace miniagent {

Agent::Agent(std::shared_ptr<LLMClient> client, std::shared_ptr<ToolRegistry> tools)
    : client_(std::move(client))
    , tools_(std::move(tools))
    , system_prompt_(DEFAULT_SYSTEM_PROMPT)
{
}

void Agent::reset() {
    history_.clear();
}

std::vector<Message> Agent::build_messages(const std::string& user_input) const {
    std::vector<Message> messages;

    // System prompt
    if (!system_prompt_.empty()) {
        messages.push_back(Message::system(system_prompt_));
    }

    // History
    for (const auto& msg : history_) {
        messages.push_back(msg);
    }

    // Current user input
    messages.push_back(Message::user(user_input));

    return messages;
}

std::vector<Message> Agent::execute_tools(const std::vector<ToolCall>& tool_calls) {
    std::vector<Message> results;
    for (const auto& tc : tool_calls) {
        std::cout << "\n\033[36m[Tool: " << tc.name << "]\033[0m\n";

        // Parse arguments from JSON string
        nlohmann::json args;
        try {
            if (!tc.arguments.empty()) {
                args = nlohmann::json::parse(tc.arguments);
            }
        } catch (const nlohmann::json::exception& e) {
            std::cerr << "\033[31m[Error parsing tool arguments: " << e.what() << "]\033[0m\n";
            results.push_back(Message::tool_result(tc.id,
                "Error: failed to parse arguments: " + std::string(e.what())));
            continue;
        }

        // Execute tool
        std::string output = tools_->execute(tc.name, args);

        // Print first few lines of output for visibility
        std::istringstream stream(output);
        std::string line;
        int preview_lines = 0;
        while (std::getline(stream, line) && preview_lines < 5) {
            std::cout << "  " << line << "\n";
            preview_lines++;
        }
        if (stream.good()) {
            std::cout << "  ... (truncated)\n";
        }

        results.push_back(Message::tool_result(tc.id, output));
    }
    return results;
}

bool Agent::print_callback(const StreamEvent& event) {
    switch (event.type) {
    case StreamEventType::TEXT_DELTA:
        std::cout << event.text << std::flush;
        break;
    case StreamEventType::TOOL_CALL_DELTA:
        // Don't print raw tool call deltas (we'll display executed results)
        break;
    case StreamEventType::FINISH:
        if (event.finish_reason == "tool_calls") {
            std::cout << "\n\033[33m[Calling tools...]\033[0m\n";
        }
        break;
    case StreamEventType::ERROR:
        std::cerr << "\033[31m[Stream error]\033[0m\n";
        break;
    }
    return true;  // continue streaming
}

std::string Agent::run(const std::string& user_input) {
    std::string final_response;
    int round = 0;

    // Build initial messages: system + history + user_input
    // We'll track the "current" messages separately for the LLM call loop
    std::vector<Message> llm_messages = build_messages(user_input);

    // Check for empty input
    if (user_input.empty()) {
        return "";
    }

    while (round < max_tool_rounds_) {
        round++;

        // Get tools schema
        nlohmann::json tools_schema = tools_->tools_schema();

        // Call LLM with streaming
        ChatResult result = client_->chat(llm_messages, tools_schema, print_callback);

        if (!result.success) {
            std::cerr << "\033[31m[LLM error: " << result.error_message << "]\033[0m\n";
            final_response = "Error: " + result.error_message;
            break;
        }

        // Collect the assistant message content and tool calls
        std::string text_content;
        std::vector<ToolCall> tool_calls;

        if (!result.content.empty()) {
            text_content = result.content;
        }
        if (!result.tool_calls.empty()) {
            tool_calls = result.tool_calls;
        }

        // Append assistant message to history
        if (!tool_calls.empty()) {
            history_.push_back(Message::assistant_tool_calls(tool_calls));

            // Also add to the current LLM messages for this turn
            llm_messages.push_back(Message::assistant_tool_calls(tool_calls));
        }
        if (!text_content.empty()) {
            // Note: if we have tool_calls, text_content is usually empty
            // If we have text without tool calls, this is the final answer
            if (tool_calls.empty()) {
                history_.push_back(Message::assistant(text_content));
                final_response = text_content;
                break;  // Done — no tool calls to process
            }
        }

        // If we have tool calls, execute them
        if (!tool_calls.empty()) {
            auto tool_results = execute_tools(tool_calls);

            // Add tool results to history
            for (const auto& tr : tool_results) {
                history_.push_back(tr);
                llm_messages.push_back(tr);
            }

            // Continue loop — LLM will process tool results
            std::cout << "\n";
            continue;
        }

        // Shouldn't reach here normally
        if (!text_content.empty()) {
            final_response = text_content;
            break;
        }
    }

    if (round >= max_tool_rounds_ && final_response.empty()) {
        final_response = "[Max tool call rounds reached]";
        std::cout << "\033[33m" << final_response << "\033[0m\n";
    }

    return final_response;
}

} // namespace miniagent
