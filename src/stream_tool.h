#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace miniagent {

// A StreamTool is a fire-and-forget action the model triggers by embedding
// an inline command in its reply text:
//
//   <tool>{"name":"echo","args":{"message":"..."}}</tool>
//
// Unlike regular tools (native function calling), a stream tool:
//   - executes the moment its closing tag streams out, while the model is
//     still generating — no round trip, generation is never interrupted;
//   - returns nothing to the model, so it only suits one-way actions
//     (speaking, status display, UI updates).
class StreamTool {
public:
    virtual ~StreamTool() = default;

    virtual std::string name() const = 0;
    virtual std::string description() const = 0;

    // Human-readable args shape shown in the protocol prompt,
    // e.g. {"message": "<string>"}
    virtual std::string args_doc() const = 0;

    // Fire-and-forget execution; exceptions are caught by the registry
    virtual void execute(const nlohmann::json& args) = 0;
};

// Holds the registered stream tools, mirroring ToolRegistry.
//
// Each tool gets its own worker thread and FIFO command queue: commands to
// the same tool execute strictly in generation order, while different tools
// run concurrently and never block each other (a slow TTS synthesis must not
// stall a status display) — and dispatch() itself never blocks the SSE
// receiving thread. Tool implementations stay plain synchronous code.
class StreamToolRegistry {
public:
    StreamToolRegistry() = default;
    ~StreamToolRegistry();

    StreamToolRegistry(const StreamToolRegistry&) = delete;
    StreamToolRegistry& operator=(const StreamToolRegistry&) = delete;

    void register_tool(std::unique_ptr<StreamTool> tool);

    size_t count() const { return workers_.size(); }

    bool has(const std::string& name) const {
        return worker_map_.find(name) != worker_map_.end();
    }

    // Protocol section appended to the system prompt, describing the inline
    // command format and the registered tools
    std::string protocol_prompt() const;

    // Enqueue a command onto its tool's worker; returns immediately.
    // Returns false if the tool is unknown.
    bool dispatch(const std::string& name, const nlohmann::json& args);

    // Block until every tool's queue is empty and its worker is idle
    void drain();

private:
    // One tool + its serialization domain: a queue and a worker thread
    struct Worker {
        std::unique_ptr<StreamTool> tool;
        std::thread thread;
        std::mutex mutex;
        std::condition_variable wake;  // signals the worker
        std::condition_variable idle;  // signals drain()
        std::deque<nlohmann::json> queue;
        bool busy = false;
        bool stopping = false;
    };

    static void worker_loop(Worker* w);
    static void stop_worker(Worker& w);

    std::vector<std::unique_ptr<Worker>> workers_;
    std::unordered_map<std::string, Worker*> worker_map_;
};

// Incremental parser that separates a streamed text flow into pass-through
// display text and complete <tool>...</tool> command payloads. Handles
// markers split across arbitrary chunk boundaries. An unterminated command
// degrades back to plain text on flush() — nothing is silently lost.
class StreamCommandParser {
public:
    using CommandHandler = std::function<void(const std::string& payload)>;

    StreamCommandParser(std::string open_marker, std::string close_marker,
                        CommandHandler handler);

    // Feed a text delta; returns the text to display. Invokes the handler
    // for each command block completed within this delta.
    std::string feed(const std::string& delta);

    // End of stream: returns any held-back text (partial marker or
    // unterminated command, restored verbatim) and resets the parser.
    std::string flush();

private:
    enum class State { TEXT, OPEN_MATCH, PAYLOAD, CLOSE_MATCH };

    void step(char c, std::string& out);

    std::string open_;
    std::string close_;
    CommandHandler handler_;

    State state_ = State::TEXT;
    std::string match_;    // partially matched marker
    std::string payload_;  // command body being accumulated
};

} // namespace miniagent
