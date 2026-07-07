#include "tts.h"

#include <iostream>

namespace miniagent {

void TtsStreamTool::execute(const nlohmann::json& args) {
    std::string text = args.value("text", "");
    if (text.empty()) {
        return;
    }

    // Placeholder: print instead of synthesizing speech. A real backend
    // does synthesize + play here, synchronously — ordering and concurrency
    // are already handled by this tool's worker thread.
    std::string line = "\n\033[35m🔊 " + text + "\033[0m\n";
    std::cout << line << std::flush;
}

} // namespace miniagent
