#pragma once

#include "../stream_tool.h"

namespace miniagent {

// TTS as a stream tool: the model emits one inline command per sentence
// while writing, and sentences are spoken in order on this tool's worker
// thread — speech tracks generation in real time.
// Placeholder backend: prints the text; swap execute() for real synthesis.
class TtsStreamTool : public StreamTool {
public:
    std::string name() const override { return "tts"; }
    std::string description() const override {
        return "Speak one sentence aloud to the user. Emit a command right "
               "after writing each sentence so speech follows your writing "
               "in real time. Plain conversational text only: no markdown, "
               "no emoji, no code.";
    }
    std::string args_doc() const override {
        return "{\"text\": \"<one sentence>\"}";
    }
    void execute(const nlohmann::json& args) override;
};

} // namespace miniagent
