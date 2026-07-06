# MiniAgent

> A minimal general-purpose AI agent in C++17 — clean, fast, and surprisingly readable.

## Why C++?

Let's be honest — when someone says "I'm building an LLM agent in C++," the first reaction is usually "...why not Python? Or Rust?"

But once you see the code, it clicks:

**The type system hits a sweet spot.** Not too bare like C, not too ceremonial like Rust. `std::variant` for content blocks, simple structs for messages, virtual dispatch for tools — every abstraction maps directly to what the program *actually does*. No `Arc<Mutex<Box<dyn Future<Output = Result<...>>>>>` — just a `while` loop you can read from top to bottom in one sitting.

**No async runtime, no problem.** The agent loop is synchronous: build messages → POST to LLM → stream back → tool calls? → execute → append to history → loop. That's four steps, no `.await` chains, no `Pin<Box<>>`. The flow is the code is the flow.

**C++ trusts you, and that's liberating during prototyping.** No borrow checker arguing with you about who owns the conversation history. No lifetime annotations threading through every callback. You want to stash a string in a struct? Just... do it. The compiler gets out of your way so you can get the architecture right first.

**The performance is a quiet flex.** Zero-copy string views, cache-friendly memory layout, and a binary that starts in milliseconds. Your GPU is doing the heavy lifting for the LLM anyway — why should your agent shell waste cycles on a JIT or GC?

Rust will give you fearless concurrency. Python will give you a one-liner `requests.post()`. But C++ gives you something rarer: **code that reads like pseudocode, runs like a native binary, and lets you focus on the architecture instead of the ceremony.** Sometimes that's exactly what you need.

---

## Architecture

```
┌──────────────┐     ┌──────────────┐     ┌─────────────────┐
│   main.cpp   │────▶│  Agent Loop  │────▶│   LLM Client    │
│  (REPL/pipe) │     │  (agent.cpp) │     │ (llm_client.cpp) │
└──────────────┘     └──────┬───────┘     └────────┬────────┘
                            │                       │
                            ▼                       ▼
                     ┌──────────────┐     ┌─────────────────┐
                     │ Tool Registry│     │  OpenAI API     │
                     │  (tool.cpp)  │     │  (HTTP SSE)     │
                     └──────────────┘     └─────────────────┘
```

**Core loop:**
```
User Input → Build Messages → LLM API (streaming SSE)
                                    ↓
                               Response
                                    ↓
                    ┌── Tool calls? → Execute → Append → Loop
                    └── Text? → Print → Done
```

## Features

- **OpenAI API standard** — works with OpenAI, DeepSeek, or any compatible provider
- **Streaming SSE** — real-time text output via libcurl
- **Tool calling** — LLM can read/write/edit files and run bash commands
- **Extensible tools** — clean virtual base class, add a tool in ~30 lines
- **Interactive REPL** — default mode (auto-detects pipe input)
- **Single binary** — ~200KB, no runtime dependencies beyond libcurl

## Quick Start

```bash
# Clone
git clone git@github.com:xingxingRealzyx/MiniAgent.git
cd MiniAgent

# Build (requires cmake, libcurl, C++17 compiler)
mkdir build && cd build
cmake .. && make

# Set your API key
export OPENAI_API_KEY="your-key-here"

# Launch interactive REPL
./miniagent

# Or pipe a single query
echo "Read CMakeLists.txt and explain the build setup" | ./miniagent

# Use with DeepSeek / other compatible APIs
export OPENAI_BASE_URL="https://api.deepseek.com"
export OPENAI_MODEL="deepseek-chat"
./miniagent
```

## Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `OPENAI_API_KEY` | *(required)* | Your API key |
| `OPENAI_BASE_URL` | `https://api.openai.com` | Compatible API endpoint |
| `OPENAI_MODEL` | `gpt-4o` | Model name |

CLI options:
```
  --model <name>   Override model
  --help, -h       Show usage
```

REPL commands:
```
  /exit, /quit     Quit
  /clear           Reset conversation history
```

## Built-in Tools

| Tool | Description |
|------|-------------|
| `read_file` | Read a file with line numbers |
| `write_file` | Create or overwrite a file |
| `edit_file` | Exact string replacement (with uniqueness check) |
| `bash` | Execute shell commands |

## Dependencies

- **C++17** compiler (Clang, GCC, MSVC)
- **CMake** ≥ 3.14
- **libcurl** (comes with macOS, `apt install libcurl4-openssl-dev` on Linux)
- **nlohmann/json** (auto-downloaded as header-only)

## Project Structure

```
miniagent/
├── CMakeLists.txt
├── src/
│   ├── main.cpp              # Entry point + REPL
│   ├── types.h               # Message / ToolCall types
│   ├── agent.h / agent.cpp   # Agent core loop
│   ├── llm_client.h / .cpp   # OpenAI API + SSE streaming
│   ├── tool.h / tool.cpp     # Tool base class + registry
│   └── tools/
│       ├── read_file.h / .cpp
│       ├── write_file.h / .cpp
│       ├── edit_file.h / .cpp
│       └── bash.h / .cpp
└── tests/
    └── test_tools.cpp
```

## License

MIT
