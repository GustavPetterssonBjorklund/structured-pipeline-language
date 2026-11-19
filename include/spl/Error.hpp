#pragma once

#include "Value.hpp"

#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace spl {

struct ErrorField {
    std::string name;
    Type type;
};

struct ErrorDefinition {
    std::string id;
    std::vector<ErrorField> fields;
};

class ErrorValue {
public:
    ErrorValue(const ErrorDefinition& definition, Values fields)
        : definition_id_(definition.id), fields_(std::move(fields)) {
        if (definition.id.empty()) throw std::invalid_argument("error definition must have an id");

        std::map<std::string, bool> declared_fields;
        for (const auto& field : definition.fields) {
            if (!declared_fields.emplace(field.name, true).second) {
                throw std::invalid_argument("duplicate error field: " + field.name);
            }
            const auto value = fields_.find(field.name);
            if (value == fields_.end()) {
                throw std::invalid_argument("missing error field: " + field.name);
            }
            if (value->second.type() != field.type) {
                throw std::invalid_argument("type mismatch for error field: " + field.name);
            }
        }
        for (const auto& [name, value] : fields_) {
            (void)value;
            bool declared = false;
            for (const auto& field : definition.fields) {
                if (field.name == name) {
                    declared = true;
                    break;
                }
            }
            if (!declared) throw std::invalid_argument("unknown error field: " + name);
        }
    }

    const std::string& definition_id() const noexcept { return definition_id_; }
    const Values& fields() const noexcept { return fields_; }

    std::string to_string() const {
        std::string result = definition_id_ + "{";
        bool first = true;
        for (const auto& [name, value] : fields_) {
            if (!first) result += ", ";
            first = false;
            result += name + "=" + value.to_string();
        }
        return result + "}";
    }

private:
    std::string definition_id_;
    Values fields_;
};

using NodeResult = std::variant<Values, ErrorValue>;

} // namespace spl
