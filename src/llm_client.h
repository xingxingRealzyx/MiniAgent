#pragma once

#include "types.h"
#include <string>
#include <vector>
#include <functional>

namespace miniagent {

// Callback for streaming events: (event) -> void
// Return false to abort streaming early
using StreamCallback = std::function<bool(const StreamEvent& event)>;

class LLMClient {
public:
    LLMClient();
    ~LLMClient();

    // Non-copyable
    LLMClient(const LLMClient&) = delete;
    LLMClient& operator=(const LLMClient&) = delete;

    // Configure
    void set_api_key(const std::string& key) { api_key_ = key; }
    void set_base_url(const std::string& url) { base_url_ = url; }
    void set_model(const std::string& model) { model_ = model; }

    // Send a chat request with streaming.
    // messages: conversation history
    // tools_schema: OpenAI-format tools array (empty = no tools)
    // callback: called for each stream event
    // Returns: final ChatResult
    ChatResult chat(
        const std::vector<Message>& messages,
        const nlohmann::json& tools_schema,
        StreamCallback callback
    );

private:
    std::string api_key_;
    std::string base_url_;
    std::string model_;

    // Curl write callback helper
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
};

} // namespace miniagent
