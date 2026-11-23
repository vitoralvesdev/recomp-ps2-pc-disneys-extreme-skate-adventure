#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ds {

// Small dependency-free JSON reader used for the GitHub releases response and
// for cached package manifests. It intentionally only needs to parse, not write.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Json> array;
    std::map<std::string, Json> object;

    bool isNull() const { return type == Type::Null; }
    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }
    bool isString() const { return type == Type::String; }

    const Json* get(std::string_view key) const;
    std::string stringOr(std::string_view key, const std::string& fallback = {}) const;
    double numberOr(std::string_view key, double fallback = 0.0) const;
    bool boolOr(std::string_view key, bool fallback = false) const;

    static std::optional<Json> parse(std::string_view text, std::string* error = nullptr);
};

} // namespace ds
