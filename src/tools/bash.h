#pragma once

#include "../tool.h"

namespace miniagent {

class BashTool : public Tool {
public:
    std::string name() const override { return "bash"; }
    std::string description() const override {
        return "Execute a bash shell command. Returns stdout and stderr output. "
               "Use this to run build commands, list files, search code, etc.";
    }
    nlohmann::json parameters_schema() const override;
    std::string execute(const nlohmann::json& arguments) override;
};

} // namespace miniagent
