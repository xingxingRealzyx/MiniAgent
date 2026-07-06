#pragma once

#include "../tool.h"

namespace miniagent {

class EditFileTool : public Tool {
public:
    std::string name() const override { return "edit_file"; }
    std::string description() const override {
        return "Perform exact string replacement in an existing file. "
               "When editing text, ensure you preserve the exact indentation (tabs/spaces) as it appears before.";
    }
    nlohmann::json parameters_schema() const override;
    std::string execute(const nlohmann::json& arguments) override;
};

} // namespace miniagent
