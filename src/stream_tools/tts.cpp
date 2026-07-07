#include "tts.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace miniagent {

void TtsStreamTool::execute(const nlohmann::json& args) {
    std::string text = args.value("text", "");
    if (text.empty()) {
        return;
    }

    // Terminal feedback for what is being spoken (single write: we are on
    // this tool's worker thread, concurrent with the main thread's output)
    std::string line = "\n\033[35m🔊 " + text + "\033[0m\n";
    std::cout << line << std::flush;

#ifdef __APPLE__
    // macOS built-in TTS. `say` blocks until playback finishes, which is
    // exactly right here: the worker thread's FIFO queue turns that into
    // ordered, gapless sentence-by-sentence speech without blocking SSE.
    std::string cmd = "say";
    if (const char* voice = std::getenv("MINIAGENT_TTS_VOICE")) {
        std::string v = voice;
        v.erase(std::remove(v.begin(), v.end(), '\''), v.end());
        if (!v.empty()) {
            cmd += " -v '" + v + "'";
        }
    }

    // Feed the text via stdin — no shell escaping of model output needed
    FILE* pipe = popen(cmd.c_str(), "w");
    if (!pipe) {
        std::cerr << "\033[31m[tts: failed to launch 'say']\033[0m\n";
        return;
    }
    fwrite(text.data(), 1, text.size(), pipe);
    fwrite("\n", 1, 1, pipe);
    pclose(pipe);
#endif
}

} // namespace miniagent
