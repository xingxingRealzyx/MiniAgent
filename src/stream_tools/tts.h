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
        return "Speak one sentence per command. When asked to read something "
               "aloud, the full content goes through this channel only, "
               "prose keeps a title at most; otherwise prose is primary and "
               "you speak just the key sentences. Plain spoken text: no "
               "markdown, no emoji, no code.";
    }
    std::string args_doc() const override {
        return "{\"text\": \"从前有一座山。\"}";
    }
    void execute(const nlohmann::json& args) override;
};

} // namespace miniagent
