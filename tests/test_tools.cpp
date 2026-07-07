#include "tool.h"
#include "tools/read_file.h"
#include "tools/write_file.h"
#include "tools/edit_file.h"
#include "tools/bash.h"

#include <iostream>
#include <fstream>
#include <cassert>

int main() {
    miniagent::ToolRegistry registry;
    registry.register_tool(std::make_unique<miniagent::ReadFileTool>());
    registry.register_tool(std::make_unique<miniagent::WriteFileTool>());
    registry.register_tool(std::make_unique<miniagent::EditFileTool>());
    registry.register_tool(std::make_unique<miniagent::BashTool>());

    assert(registry.count() == 4);

    // Test tools schema
    auto schema = registry.tools_schema();
    assert(schema.is_array());
    assert(schema.size() == 4);

    std::cout << "[PASS] Tool registry: " << registry.count() << " tools registered\n";

    // Test read_file on a file created by the test itself
    {
        std::ofstream f("/tmp/miniagent_test_read.txt");
        f << "line one\nline two\n";
    }
    auto args = nlohmann::json::object();
    args["file_path"] = "/tmp/miniagent_test_read.txt";
    std::string read_result = registry.execute("read_file", args);
    assert(read_result.find("line one") != std::string::npos);
    assert(read_result.find("line two") != std::string::npos);
    std::cout << "[PASS] read_file: " << read_result.size() << " bytes output\n";

    // read_file on a missing file must report an error
    auto missing_args = nlohmann::json::object();
    missing_args["file_path"] = "/tmp/miniagent_test_does_not_exist.txt";
    std::string missing_result = registry.execute("read_file", missing_args);
    assert(missing_result.find("cannot open file") != std::string::npos);
    std::cout << "[PASS] read_file: missing file reports error\n";

    // Test write_file
    auto write_args = nlohmann::json::object();
    write_args["file_path"] = "/tmp/miniagent_test_write.txt";
    write_args["content"] = "Hello from MiniAgent!";
    std::string write_result = registry.execute("write_file", write_args);
    assert(write_result.find("Successfully wrote") != std::string::npos);
    std::cout << "[PASS] write_file: " << write_result << "\n";

    // Test edit_file
    auto edit_args = nlohmann::json::object();
    edit_args["file_path"] = "/tmp/miniagent_test_write.txt";
    edit_args["old_string"] = "Hello from MiniAgent!";
    edit_args["new_string"] = "Hello from MiniAgent (edited)!";
    std::string edit_result = registry.execute("edit_file", edit_args);
    assert(edit_result.find("Successfully edited") != std::string::npos);
    std::cout << "[PASS] edit_file: " << edit_result << "\n";

    // Verify edit
    auto read_edited = nlohmann::json::object();
    read_edited["file_path"] = "/tmp/miniagent_test_write.txt";
    std::string verify = registry.execute("read_file", read_edited);
    assert(verify.find("(edited)") != std::string::npos);
    std::cout << "[PASS] edit verification: content correct\n";

    // Test bash
    auto bash_args = nlohmann::json::object();
    bash_args["command"] = "echo 'bash works!'";
    std::string bash_result = registry.execute("bash", bash_args);
    assert(bash_result.find("bash works!") != std::string::npos);
    std::cout << "[PASS] bash: " << bash_result;

    // Cleanup
    std::remove("/tmp/miniagent_test_write.txt");
    std::remove("/tmp/miniagent_test_read.txt");

    std::cout << "\nAll tests passed!\n";
    return 0;
}
