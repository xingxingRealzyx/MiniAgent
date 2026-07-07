#include "agent.h"

#include <chrono>
#include <iostream>
#include <sstream>

namespace miniagent {

Agent::Agent(std::shared_ptr<LLMClient> client,
             std::shared_ptr<ToolRegistry> tools,
             std::shared_ptr<StreamToolRegistry> stream_tools)
    : client_(std::move(client))
    , tools_(std::move(tools))
    , stream_tools_(stream_tools ? std::move(stream_tools)
                                 : std::make_shared<StreamToolRegistry>())
    , system_prompt_(DEFAULT_SYSTEM_PROMPT)
{
}

void Agent::reset() {
    history_.clear();
}

std::vector<Message> Agent::build_messages() const {
    std::vector<Message> messages;

    // System prompt, plus the inline-command protocol if stream tools exist
    if (!system_prompt_.empty()) {
        std::string sys = system_prompt_;
        if (stream_tools_->count() > 0) {
            sys += stream_tools_->protocol_prompt();
        }
        messages.push_back(Message::system(sys));
    }

    // History (already includes the current user input)
    for (const auto& msg : history_) {
        messages.push_back(msg);
    }

    return messages;
}

std::vector<Message> Agent::execute_tools(const std::vector<ToolCall>& tool_calls) {
    std::vector<Message> results;
    for (const auto& tc : tool_calls) {
        // Show the tool name and its arguments (dimmed, truncated if long)
        std::string args_preview = tc.arguments.empty() ? "{}" : tc.arguments;
        if (args_preview.size() > 300) {
            args_preview.resize(300);
            args_preview += "...";
        }
        std::cout << "\n\033[36m[Tool: " << tc.name << "]\033[0m \033[2m"
                  << args_preview << "\033[0m\n";

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

        // The model sometimes mistakes a stream tool for a function tool.
        // Execute it anyway (the user still gets the effect), but teach the
        // model the inline syntax in the result so it self-corrects.
        std::string output;
        if (!tools_->has(tc.name) && stream_tools_->has(tc.name)) {
            stream_tools_->dispatch(tc.name, args);
            nlohmann::json correction;
            correction["success"] = true;
            correction["result"] =
                "Executed, but '" + tc.name + "' is a streaming command, not a "
                "function tool. Next time embed it inline in your reply text: "
                "<tool>{\"name\":\"" + tc.name + "\",\"args\":{...}}</tool>";
            output = correction.dump();
        } else {
            output = tools_->execute(tc.name, args);
        }

        // Print first few lines of output for visibility
        std::istringstream stream(output);
        std::string line;
        int preview_lines = 0;
        while (std::getline(stream, line)) {
            if (preview_lines >= 5) {
                std::cout << "  ... (truncated)\n";
                break;
            }
            std::cout << "  " << line << "\n";
            preview_lines++;
        }

        results.push_back(Message::tool_result(tc.id, output));
    }
    return results;
}

void Agent::handle_stream_command(const std::string& payload) {
    try {
        auto cmd = nlohmann::json::parse(payload);
        std::string name = cmd.value("name", "");
        nlohmann::json args = cmd.value("args", nlohmann::json::object());

        if (!stream_tools_->dispatch(name, args)) {
            std::cerr << "\033[31m[Unknown stream tool: " << name << "]\033[0m\n";
        }
    } catch (const nlohmann::json::exception&) {
        // Malformed command — degrade to visible text so nothing is lost
        std::cout << "\033[2m" << payload << "\033[0m" << std::flush;
    }
}

std::string Agent::run(const std::string& user_input) {
    if (user_input.empty()) {
        return "";
    }

    auto start_time = std::chrono::steady_clock::now();
    std::string final_response;
    int round = 0;

    // Record the user message in history, then build the request from it
    history_.push_back(Message::user(user_input));
    std::vector<Message> llm_messages = build_messages();

    while (round < max_tool_rounds_) {
        round++;

        // Get tools schema
        nlohmann::json tools_schema = tools_->tools_schema();

        // Inline commands cannot span rounds — fresh parser per round.
        // It splits the text stream into display text and command payloads.
        StreamCommandParser parser("<tool>", "</tool>",
            [this](const std::string& payload) { handle_stream_command(payload); });

        // Call LLM with streaming
        ChatResult result = client_->chat(llm_messages, tools_schema,
            [&parser](const StreamEvent& event) {
                switch (event.type) {
                case StreamEventType::TEXT_DELTA:
                    std::cout << parser.feed(event.text) << std::flush;
                    break;
                case StreamEventType::TOOL_CALL_DELTA:
                    // Not printed — tool execution is displayed separately
                    break;
                case StreamEventType::FINISH:
                    std::cout << parser.flush() << std::flush;
                    if (event.finish_reason == "tool_calls") {
                        std::cout << "\n\033[33m[Calling tools...]\033[0m\n";
                    }
                    break;
                case StreamEventType::ERROR:
                    std::cerr << "\033[31m[Stream error]\033[0m\n";
                    break;
                }
                return true;
            });

        // Safety net if the stream ended without a FINISH event
        std::cout << parser.flush() << std::flush;

        if (!result.success) {
            std::cerr << "\033[31m[LLM error: " << result.error_message << "]\033[0m\n";
            final_response = "Error: " + result.error_message;
            break;
        }

        // No tool calls — this is the final answer
        if (result.tool_calls.empty()) {
            if (!result.content.empty()) {
                history_.push_back(Message::assistant(result.content));
                final_response = result.content;
            }
            break;
        }

        // Assistant message with tool calls (may also carry text content)
        Message assistant_msg = Message::assistant_tool_calls(result.tool_calls, result.content);
        history_.push_back(assistant_msg);
        llm_messages.push_back(assistant_msg);

        // Execute tools and feed results back
        auto tool_results = execute_tools(result.tool_calls);
        for (const auto& tr : tool_results) {
            history_.push_back(tr);
            llm_messages.push_back(tr);
        }

        std::cout << "\n";
    }

    if (round >= max_tool_rounds_ && final_response.empty()) {
        final_response = "[Max tool call rounds reached]";
        std::cout << "\033[33m" << final_response << "\033[0m\n";
    }

    // Wait for all pending stream commands before yielding the terminal,
    // so late output doesn't interleave with the next prompt
    stream_tools_->drain();

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    std::cout << "\n\033[2m[" << elapsed_ms << "ms]\033[0m";

    return final_response;
}

} // namespace miniagent
