#include "bash.h"

#include <chrono>
#include <cerrno>
#include <csignal>
#include <string>

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace miniagent {

nlohmann::json BashTool::parameters_schema() const {
    nlohmann::json schema;
    schema["type"] = "object";
    schema["properties"]["command"]["type"] = "string";
    schema["properties"]["command"]["description"] = "The bash command to execute";
    schema["properties"]["timeout"]["type"] = "integer";
    schema["properties"]["timeout"]["description"] = "Timeout in milliseconds (optional, default 60000, max 300000)";
    schema["required"] = {"command"};
    return schema;
}

std::string BashTool::execute(const nlohmann::json& arguments) {
    std::string command = arguments.value("command", "");
    if (command.empty()) {
        return "Error: command is required";
    }

    int timeout_ms = arguments.value("timeout", 60000);
    if (timeout_ms > 300000) timeout_ms = 300000;
    if (timeout_ms < 1000) timeout_ms = 1000;

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return "Error: failed to create pipe";
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return "Error: failed to fork";
    }

    if (pid == 0) {
        // Child: own process group so the whole command tree can be killed on timeout
        setpgid(0, 0);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execl("/bin/sh", "sh", "-c", command.c_str(), (char*)nullptr);
        _exit(127);
    }

    // Also set in parent to close the race with the child's exec
    setpgid(pid, pid);
    close(pipefd[1]);

    std::string result;
    bool timed_out = false;
    char buf[4096];

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);

    while (true) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) {
            timed_out = true;
            break;
        }

        struct pollfd pfd;
        pfd.fd = pipefd[0];
        pfd.events = POLLIN;
        pfd.revents = 0;

        int pr = poll(&pfd, 1, (int)remaining);
        if (pr < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (pr == 0) {
            timed_out = true;
            break;
        }

        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n > 0) {
            result.append(buf, (size_t)n);
        } else {
            break;  // EOF (command finished and closed the pipe) or read error
        }
    }
    close(pipefd[0]);

    if (timed_out) {
        kill(-pid, SIGKILL);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}

    if (timed_out) {
        std::string msg = "Command timed out after " + std::to_string(timeout_ms) + "ms";
        if (!result.empty()) {
            result += "\n[" + msg + "]";
        } else {
            result = msg;
        }
        return result;
    }

    if (result.empty()) {
        result = "(no output)";
    }

    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code != 0) {
            result += "\n[Exit code: " + std::to_string(code) + "]";
        }
    } else if (WIFSIGNALED(status)) {
        result += "\n[Terminated by signal " + std::to_string(WTERMSIG(status)) + "]";
    }

    return result;
}

} // namespace miniagent
