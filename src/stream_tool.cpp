#include "stream_tool.h"

#include <iostream>

namespace miniagent {

// ============================================================
// StreamToolRegistry
// ============================================================

StreamToolRegistry::~StreamToolRegistry() {
    for (auto& w : workers_) {
        stop_worker(*w);
    }
}

void StreamToolRegistry::worker_loop(Worker* w) {
    for (;;) {
        nlohmann::json args;
        {
            std::unique_lock<std::mutex> lock(w->mutex);
            w->wake.wait(lock, [w] { return w->stopping || !w->queue.empty(); });
            if (w->queue.empty()) {
                return;  // stopping and fully drained
            }
            args = std::move(w->queue.front());
            w->queue.pop_front();
            w->busy = true;
        }

        try {
            w->tool->execute(args);
        } catch (const std::exception& e) {
            std::cerr << "\033[31m[Stream tool '" << w->tool->name()
                      << "' error: " << e.what() << "]\033[0m\n";
        }

        {
            std::lock_guard<std::mutex> lock(w->mutex);
            w->busy = false;
            if (w->queue.empty()) {
                w->idle.notify_all();
            }
        }
    }
}

void StreamToolRegistry::stop_worker(Worker& w) {
    {
        std::lock_guard<std::mutex> lock(w.mutex);
        w.stopping = true;
    }
    w.wake.notify_all();
    if (w.thread.joinable()) {
        w.thread.join();
    }
}

void StreamToolRegistry::register_tool(std::unique_ptr<StreamTool> tool) {
    std::string n = tool->name();

    // Re-registering the same name replaces the existing tool
    auto it = worker_map_.find(n);
    if (it != worker_map_.end()) {
        Worker* old = it->second;
        stop_worker(*old);
        worker_map_.erase(it);
        for (auto wit = workers_.begin(); wit != workers_.end(); ++wit) {
            if (wit->get() == old) {
                workers_.erase(wit);
                break;
            }
        }
    }

    auto worker = std::make_unique<Worker>();
    worker->tool = std::move(tool);
    worker->thread = std::thread(worker_loop, worker.get());

    worker_map_[n] = worker.get();
    workers_.push_back(std::move(worker));
}

std::string StreamToolRegistry::protocol_prompt() const {
    std::string p =
        "\n## Streaming commands\n"
        "While writing your reply you can trigger real-time, fire-and-forget "
        "actions by embedding a command inline in your text:\n"
        "<tool>{\"name\":\"<command>\",\"args\":{...}}</tool>\n"
        "The command executes the moment its closing tag is emitted; you keep "
        "writing normally after it. Commands return no result.\n"
        "Available commands:\n";
    for (const auto& worker : workers_) {
        p += "- " + worker->tool->name() + ": " + worker->tool->description()
           + " Args: " + worker->tool->args_doc() + "\n";
    }
    p += "Rules:\n"
         "- These commands are NOT function tools. Never invoke them via "
         "function calling — embed them inline in your reply text exactly "
         "as shown above.\n"
         "- The whole command must be valid JSON on a single line, "
         "not wrapped in code fences.\n"
         "- Never show or mention these tags to the user.\n";
    return p;
}

bool StreamToolRegistry::dispatch(const std::string& name, const nlohmann::json& args) {
    auto it = worker_map_.find(name);
    if (it == worker_map_.end()) {
        return false;
    }

    Worker* w = it->second;
    {
        std::lock_guard<std::mutex> lock(w->mutex);
        w->queue.push_back(args);
    }
    w->wake.notify_all();
    return true;
}

void StreamToolRegistry::drain() {
    for (auto& w : workers_) {
        std::unique_lock<std::mutex> lock(w->mutex);
        w->idle.wait(lock, [&w] { return w->queue.empty() && !w->busy; });
    }
}

// ============================================================
// StreamCommandParser
// ============================================================

StreamCommandParser::StreamCommandParser(std::string open_marker,
                                         std::string close_marker,
                                         CommandHandler handler)
    : open_(std::move(open_marker))
    , close_(std::move(close_marker))
    , handler_(std::move(handler))
{
}

std::string StreamCommandParser::feed(const std::string& delta) {
    std::string out;
    for (char c : delta) {
        step(c, out);
    }
    return out;
}

std::string StreamCommandParser::flush() {
    std::string out;
    switch (state_) {
    case State::TEXT:
        break;
    case State::OPEN_MATCH:
        out = match_;  // partial opening marker was just text
        break;
    case State::PAYLOAD:
    case State::CLOSE_MATCH:
        // Unterminated command — restore it verbatim as text
        out = open_ + payload_ + match_;
        break;
    }
    state_ = State::TEXT;
    match_.clear();
    payload_.clear();
    return out;
}

void StreamCommandParser::step(char c, std::string& out) {
    switch (state_) {
    case State::TEXT:
        if (c == open_[0]) {
            state_ = State::OPEN_MATCH;
            match_ = c;
        } else {
            out += c;
        }
        break;

    case State::OPEN_MATCH:
        if (c == open_[match_.size()]) {
            match_ += c;
            if (match_.size() == open_.size()) {
                state_ = State::PAYLOAD;
                match_.clear();
                payload_.clear();
            }
        } else {
            // Mismatch: first held char was plain text; reprocess the rest
            std::string rest = match_.substr(1);
            rest += c;
            out += match_[0];
            match_.clear();
            state_ = State::TEXT;
            for (char r : rest) {
                step(r, out);
            }
        }
        break;

    case State::PAYLOAD:
        if (c == close_[0]) {
            state_ = State::CLOSE_MATCH;
            match_ = c;
        } else {
            payload_ += c;
        }
        break;

    case State::CLOSE_MATCH:
        if (c == close_[match_.size()]) {
            match_ += c;
            if (match_.size() == close_.size()) {
                handler_(payload_);
                match_.clear();
                payload_.clear();
                state_ = State::TEXT;
            }
        } else {
            // Mismatch: held chars belong to the payload; reprocess the rest
            payload_ += match_[0];
            std::string rest = match_.substr(1);
            rest += c;
            match_.clear();
            state_ = State::PAYLOAD;
            for (char r : rest) {
                step(r, out);
            }
        }
        break;
    }
}

} // namespace miniagent
