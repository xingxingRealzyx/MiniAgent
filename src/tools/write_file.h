#pragma once

#include "../tool.h"

namespace miniagent {

class WriteFileTool : public Tool {
public:
    std::string name() const override { return "write_file"; }
    std::string description() const override {
        return "Write content to a file. Creates the file if it does not exist, overwrites if it does.";
    }
    nlohmann::json parameters_schema() const override;
    std::string execute(const nlohmann::json& arguments) override;
};

} // namespace miniagent
