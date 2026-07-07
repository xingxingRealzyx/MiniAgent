#pragma once

#include "../stream_tool.h"

namespace miniagent {

// Experimental stream tool: prints the received message immediately.
// Exists to exercise the inline-command pipeline end to end; real
// consumers (e.g. TTS) follow the same shape.
class EchoStreamTool : public StreamTool {
public:
    std::string name() const override { return "echo"; }
    std::string description() const override {
        return "Show the user a short real-time status line while you keep writing.";
    }
    std::string args_doc() const override {
        return "{\"message\": \"正在分析代码\"}";
    }
    void execute(const nlohmann::json& args) override;
};

} // namespace miniagent
