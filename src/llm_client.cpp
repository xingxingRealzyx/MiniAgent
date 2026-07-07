#include "llm_client.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>

namespace miniagent {

// Cap on how much of the raw response body we keep for error reporting
static constexpr size_t RAW_BODY_CAP = 8192;

// ============================================================
// Internal state for SSE parsing via curl write callback
// ============================================================
struct StreamState {
    StreamCallback callback;
    std::string buffer;      // accumulates a partial line
    std::string event_data;  // accumulated "data:" payload of the current SSE event
    std::string text_content;  // collected full text
    std::map<int, ToolCall> pending_tool_calls;  // index -> partial ToolCall
    std::string finish_reason;
    std::string raw_body;    // first bytes of the response, for HTTP error reporting
    bool aborted = false;    // callback requested abort
};

// Deliver an event to the user callback; a false return aborts the stream
static void emit(StreamState& state, const StreamEvent& event) {
    if (state.aborted) return;
    if (!state.callback(event)) {
        state.aborted = true;
    }
}

// Parse a single SSE "data: {...}" line
static void parse_sse_data(const std::string& data, StreamState& state) {
    if (data == "[DONE]") {
        return;  // stream end marker
    }

    try {
        auto chunk = nlohmann::json::parse(data);
        auto& choices = chunk["choices"];
        if (choices.empty()) return;

        auto& choice = choices[0];
        auto& delta = choice["delta"];

        // --- Text content delta ---
        if (delta.contains("content") && !delta["content"].is_null()) {
            std::string text = delta["content"].get<std::string>();
            state.text_content += text;

            StreamEvent event;
            event.type = StreamEventType::TEXT_DELTA;
            event.text = text;
            emit(state, event);
        }

        // --- Tool calls delta ---
        if (delta.contains("tool_calls") && !delta["tool_calls"].is_null()) {
            for (const auto& tc : delta["tool_calls"]) {
                int idx = tc.value("index", -1);
                if (idx < 0) continue;

                auto& pending = state.pending_tool_calls[idx];

                // Tool call ID
                if (tc.contains("id") && !tc["id"].is_null()) {
                    std::string call_id = tc["id"].get<std::string>();
                    pending.id = call_id;

                    StreamEvent event;
                    event.type = StreamEventType::TOOL_CALL_DELTA;
                    event.tool_call_index = idx;
                    event.tool_call_id = call_id;
                    emit(state, event);
                }

                // Function name
                if (tc.contains("function")) {
                    auto& func = tc["function"];
                    if (func.contains("name") && !func["name"].is_null()) {
                        pending.name = func["name"].get<std::string>();

                        StreamEvent event;
                        event.type = StreamEventType::TOOL_CALL_DELTA;
                        event.tool_call_index = idx;
                        event.tool_name = pending.name;
                        emit(state, event);
                    }
                    // Function arguments (streamed as JSON fragments)
                    if (func.contains("arguments") && !func["arguments"].is_null()) {
                        std::string args = func["arguments"].get<std::string>();
                        pending.arguments += args;

                        StreamEvent event;
                        event.type = StreamEventType::TOOL_CALL_DELTA;
                        event.tool_call_index = idx;
                        event.text = args;
                        emit(state, event);
                    }
                }
            }
        }

        // --- Finish reason ---
        if (choice.contains("finish_reason") && !choice["finish_reason"].is_null()) {
            state.finish_reason = choice["finish_reason"].get<std::string>();

            StreamEvent event;
            event.type = StreamEventType::FINISH;
            event.finish_reason = state.finish_reason;
            emit(state, event);
        }

    } catch (const nlohmann::json::exception& e) {
        // Ignore malformed JSON chunks (shouldn't happen normally)
        std::cerr << "[warn] JSON parse error in SSE: " << e.what() << "\n";
    }
}

// Curl write callback — accumulates data and parses complete SSE lines.
// Handles both LF and CRLF line endings, and "data:" with or without a space.
static size_t write_callback_impl(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* state = static_cast<StreamState*>(userdata);
    size_t total = size * nmemb;

    // Keep the head of the raw body for HTTP error reporting
    if (state->raw_body.size() < RAW_BODY_CAP) {
        state->raw_body.append(ptr, std::min(total, RAW_BODY_CAP - state->raw_body.size()));
    }

    state->buffer.append(ptr, total);

    size_t pos;
    while ((pos = state->buffer.find('\n')) != std::string::npos) {
        std::string line = state->buffer.substr(0, pos);
        state->buffer.erase(0, pos + 1);

        // Trim trailing \r (CRLF line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            // Blank line = end of SSE event: dispatch accumulated data
            if (!state->event_data.empty()) {
                parse_sse_data(state->event_data, *state);
                state->event_data.clear();
            }
        } else if (line[0] == ':') {
            // SSE comment line — ignore
        } else if (line.rfind("data:", 0) == 0) {
            std::string value = line.substr(5);
            if (!value.empty() && value.front() == ' ') {
                value.erase(0, 1);
            }
            // Per SSE spec, multiple data lines in one event are joined with \n
            if (!state->event_data.empty()) {
                state->event_data += '\n';
            }
            state->event_data += value;
        }

        if (state->aborted) {
            return 0;  // signal curl to abort the transfer
        }
    }

    return total;
}

// ============================================================
// LLMClient implementation
// ============================================================

LLMClient::LLMClient()
    : base_url_("https://api.openai.com")
    , model_("gpt-4o")
{
    // curl_global_init/cleanup are not reference-counted; do it once per process
    static std::once_flag curl_init_flag;
    std::call_once(curl_init_flag, [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        std::atexit([] { curl_global_cleanup(); });
    });
}

LLMClient::~LLMClient() = default;

size_t LLMClient::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    return write_callback_impl(ptr, size, nmemb, userdata);
}

ChatResult LLMClient::chat(
    const std::vector<Message>& messages,
    const nlohmann::json& tools_schema,
    StreamCallback callback)
{
    ChatResult result;

    if (api_key_.empty()) {
        result.success = false;
        result.error_message = "OPENAI_API_KEY not set";
        return result;
    }

    // Build request body
    nlohmann::json body;
    body["model"] = model_;
    body["stream"] = true;

    // Build messages array
    auto msgs_json = nlohmann::json::array();
    for (const auto& msg : messages) {
        nlohmann::json m;
        m["role"] = msg.role;

        if (msg.role == "tool") {
            // Tool result message
            m["tool_call_id"] = msg.tool_call_id;
            m["content"] = msg.content;
        } else if (msg.role == "assistant" && !msg.tool_calls.empty()) {
            // Assistant message with tool calls
            if (msg.content.empty()) {
                m["content"] = nullptr;
            } else {
                m["content"] = msg.content;
            }
            auto tcs = nlohmann::json::array();
            for (const auto& tc : msg.tool_calls) {
                nlohmann::json t;
                t["id"] = tc.id;
                t["type"] = "function";
                t["function"]["name"] = tc.name;
                t["function"]["arguments"] = tc.arguments;
                tcs.push_back(t);
            }
            m["tool_calls"] = tcs;
        } else {
            // System, user, or plain assistant message
            m["content"] = msg.content;
        }

        msgs_json.push_back(m);
    }
    body["messages"] = msgs_json;

    // Add tools if present
    if (!tools_schema.empty()) {
        body["tools"] = tools_schema;
    }

    std::string body_str = body.dump();
    std::string url = base_url_ + "/v1/chat/completions";

    // Setup curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        result.success = false;
        result.error_message = "Failed to initialize curl";
        return result;
    }

    // State for SSE parsing
    StreamState state;
    state.callback = std::move(callback);

    // Headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + api_key_;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body_str.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);  // 5 min timeout

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    bool user_aborted = (res == CURLE_WRITE_ERROR && state.aborted);

    if (res != CURLE_OK && !user_aborted) {
        result.success = false;
        result.error_message = std::string("curl error: ") + curl_easy_strerror(res);
    } else if (http_code >= 400) {
        result.success = false;
        result.error_message = "HTTP " + std::to_string(http_code);
        // Error responses are plain JSON, not SSE — surface the body
        std::string body = state.raw_body;
        while (!body.empty() && (body.back() == '\n' || body.back() == '\r')) {
            body.pop_back();
        }
        if (!body.empty()) {
            result.error_message += ": " + body;
        }
    } else {
        // Dispatch a trailing event not terminated by a blank line
        if (!state.aborted && !state.event_data.empty()) {
            parse_sse_data(state.event_data, state);
        }
        result.success = true;  // partial content on user abort is still a success
        result.content = state.text_content;
        // Move collected tool calls, sorted by index (std::map iterates in order)
        for (auto& [idx, tc] : state.pending_tool_calls) {
            result.tool_calls.push_back(std::move(tc));
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return result;
}

} // namespace miniagent
