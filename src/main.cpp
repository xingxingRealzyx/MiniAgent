#include "agent.h"
#include "llm_client.h"
#include "tool.h"
#include "tools/read_file.h"
#include "tools/write_file.h"
#include "tools/edit_file.h"
#include "tools/bash.h"

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

// ANSI color helpers
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_GREEN   "\033[32m"

static void print_banner() {
    std::cout << COLOR_CYAN << COLOR_BOLD
              << "╔══════════════════════════════════╗\n"
              << "║         MiniAgent v1.0          ║\n"
              << "║  Minimal General-Purpose Agent  ║\n"
              << "╚══════════════════════════════════╝\n"
              << COLOR_RESET << "\n"
              << "Type /exit to quit, /clear to reset history\n\n";
}

static void run_repl(miniagent::Agent& agent) {
    print_banner();

    std::string line;
    while (true) {
        std::cout << COLOR_GREEN "> " COLOR_RESET << std::flush;
        if (!std::getline(std::cin, line)) {
            break;  // EOF
        }

        if (line == "/exit" || line == "/quit") {
            std::cout << "Goodbye.\n";
            break;
        }

        if (line == "/clear") {
            agent.reset();
            std::cout << "Conversation history cleared.\n";
            continue;
        }

        if (line.empty()) {
            continue;
        }

        // Support multi-line input: lines starting with "| " are appended
        // This is a simple approach — type `| ` prefix for continuation lines
        std::ostringstream input;
        input << line;

        std::cout << "\n";
        std::string response = agent.run(input.str());
        std::cout << "\n\n";
    }
}

static std::string read_stdin() {
    std::ostringstream oss;
    oss << std::cin.rdbuf();
    return oss.str();
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n"
              << "  Default: interactive REPL mode.\n"
              << "  Pipe input to run a single-turn query.\n"
              << "\nOptions:\n"
              << "  --model <name>  Model to use (default: gpt-4o)\n"
              << "  --help, -h      Show this help\n"
              << "\nEnvironment:\n"
              << "  OPENAI_API_KEY   API key (required)\n"
              << "  OPENAI_BASE_URL  Base URL (default: https://api.openai.com)\n"
              << "  OPENAI_MODEL     Model name (default: gpt-4o)\n"
              << "\nExamples:\n"
              << "  miniagent                           # interactive REPL\n"
              << "  echo \"List files\" | miniagent       # single-turn via pipe\n"
              << "  miniagent --model gpt-4o\n";
}

int main(int argc, char* argv[]) {
    // Parse CLI args
    std::string model_override;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            model_override = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Get config from environment
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key || !*api_key) {
        std::cerr << "Error: OPENAI_API_KEY environment variable is not set.\n";
        return 1;
    }

    const char* base_url = std::getenv("OPENAI_BASE_URL");
    const char* model_env = std::getenv("OPENAI_MODEL");

    std::string model = model_override.empty()
        ? (model_env ? model_env : "gpt-4o")
        : model_override;

    // Initialize components
    auto client = std::make_shared<miniagent::LLMClient>();
    client->set_api_key(api_key);
    if (base_url) {
        client->set_base_url(base_url);
    }
    client->set_model(model);

    auto tools = std::make_shared<miniagent::ToolRegistry>();
    tools->register_tool(std::make_unique<miniagent::ReadFileTool>());
    tools->register_tool(std::make_unique<miniagent::WriteFileTool>());
    tools->register_tool(std::make_unique<miniagent::EditFileTool>());
    tools->register_tool(std::make_unique<miniagent::BashTool>());

    miniagent::Agent agent(client, tools);
    agent.set_max_tool_rounds(10);

    // Auto-detect: pipe input → single-shot; tty → REPL
    bool is_pipe = !isatty(STDIN_FILENO);

    if (is_pipe) {
        std::string input = read_stdin();

        if (input.empty()) {
            return 0;
        }

        // Trim trailing newlines
        while (!input.empty() && (input.back() == '\n' || input.back() == '\r')) {
            input.pop_back();
        }

        if (input.empty()) {
            return 0;
        }

        std::string response = agent.run(input);
        std::cout << "\n";
    } else {
        run_repl(agent);
    }

    return 0;
}
