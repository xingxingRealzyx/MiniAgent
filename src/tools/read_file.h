#pragma once

#include "../tool.h"

namespace miniagent {

class ReadFileTool : public Tool {
public:
    std::string name() const override { return "read_file"; }
    std::string description() const override {
        return "Read the contents of a file. Returns the file content with line numbers.";
    }
    nlohmann::json parameters_schema() const override;
    std::string execute(const nlohmann::json& arguments) override;
};

} // namespace miniagent
