#include "json.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace ds {
namespace {

void appendUtf8(std::string& out, uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    std::optional<Json> parse(std::string& error) {
        skipWhitespace();
        auto value = parseValue(0);
        if (!value) {
            error = error_;
            return std::nullopt;
        }
        skipWhitespace();
        if (position_ != text_.size()) {
            error = "conteúdo extra após o JSON";
            return std::nullopt;
        }
        return value;
    }

private:
    void skipWhitespace() {
        while (position_ < text_.size()) {
            const char ch = text_[position_];
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
                ++position_;
            } else {
                break;
            }
        }
    }

    bool fail(const char* message) {
        if (error_.empty()) {
            error_ = std::string(message) + " na posição " + std::to_string(position_);
        }
        return false;
    }

    std::optional<Json> parseValue(int depth) {
        if (depth > 64) {
            fail("JSON aninhado demais");
            return std::nullopt;
        }
        skipWhitespace();
        if (position_ >= text_.size()) {
            fail("fim inesperado do JSON");
            return std::nullopt;
        }
        const char ch = text_[position_];
        if (ch == '{') return parseObject(depth + 1);
        if (ch == '[') return parseArray(depth + 1);
        if (ch == '"') {
            Json result;
            result.type = Json::Type::String;
            if (!parseString(result.string)) return std::nullopt;
            return result;
        }
        if (ch == 't') {
            if (!consume("true")) return std::nullopt;
            Json result;
            result.type = Json::Type::Bool;
            result.boolean = true;
            return result;
        }
        if (ch == 'f') {
            if (!consume("false")) return std::nullopt;
            Json result;
            result.type = Json::Type::Bool;
            return result;
        }
        if (ch == 'n') {
            if (!consume("null")) return std::nullopt;
            return Json{};
        }
        if (ch == '-' || (ch >= '0' && ch <= '9')) return parseNumber();
        fail("valor JSON inválido");
        return std::nullopt;
    }

    bool consume(std::string_view literal) {
        if (text_.substr(position_, literal.size()) != literal) {
            fail("literal JSON inválido");
            return false;
        }
        position_ += literal.size();
        return true;
    }

    std::optional<Json> parseObject(int depth) {
        Json result;
        result.type = Json::Type::Object;
        ++position_; // {
        skipWhitespace();
        if (position_ < text_.size() && text_[position_] == '}') {
            ++position_;
            return result;
        }
        for (;;) {
            skipWhitespace();
            std::string key;
            if (position_ >= text_.size() || text_[position_] != '"') {
                fail("chave JSON inválida");
                return std::nullopt;
            }
            if (!parseString(key)) return std::nullopt;
            skipWhitespace();
            if (position_ >= text_.size() || text_[position_] != ':') {
                fail("':' esperado");
                return std::nullopt;
            }
            ++position_;
            auto value = parseValue(depth);
            if (!value) return std::nullopt;
            result.object.emplace(std::move(key), std::move(*value));
            skipWhitespace();
            if (position_ >= text_.size()) {
                fail("objeto JSON incompleto");
                return std::nullopt;
            }
            if (text_[position_] == ',') {
                ++position_;
                continue;
            }
            if (text_[position_] == '}') {
                ++position_;
                return result;
            }
            fail("',' ou '}' esperado");
            return std::nullopt;
        }
    }

    std::optional<Json> parseArray(int depth) {
        Json result;
        result.type = Json::Type::Array;
        ++position_; // [
        skipWhitespace();
        if (position_ < text_.size() && text_[position_] == ']') {
            ++position_;
            return result;
        }
        for (;;) {
            auto value = parseValue(depth);
            if (!value) return std::nullopt;
            result.array.push_back(std::move(*value));
            skipWhitespace();
            if (position_ >= text_.size()) {
                fail("array JSON incompleto");
                return std::nullopt;
            }
            if (text_[position_] == ',') {
                ++position_;
                continue;
            }
            if (text_[position_] == ']') {
                ++position_;
                return result;
            }
            fail("',' ou ']' esperado");
            return std::nullopt;
        }
    }

    std::optional<Json> parseNumber() {
        const size_t start = position_;
        if (text_[position_] == '-') ++position_;
        while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
            ++position_;
        }
        if (position_ < text_.size() && text_[position_] == '.') {
            ++position_;
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
                ++position_;
            }
        }
        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            ++position_;
            if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) {
                ++position_;
            }
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
                ++position_;
            }
        }
        const std::string token(text_.substr(start, position_ - start));
        if (token.empty()) {
            fail("número inválido");
            return std::nullopt;
        }
        char* end = nullptr;
        const double parsed = std::strtod(token.c_str(), &end);
        if (!end || *end != '\0') {
            fail("número inválido");
            return std::nullopt;
        }
        Json result;
        result.type = Json::Type::Number;
        result.number = parsed;
        return result;
    }

    bool parseHex4(uint32_t& value) {
        if (position_ + 4 > text_.size()) return fail("escape unicode incompleto");
        value = 0;
        for (int i = 0; i < 4; ++i) {
            const char ch = text_[position_++];
            value <<= 4;
            if (ch >= '0' && ch <= '9') {
                value |= static_cast<uint32_t>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                value |= static_cast<uint32_t>(ch - 'a' + 10);
            } else if (ch >= 'A' && ch <= 'F') {
                value |= static_cast<uint32_t>(ch - 'A' + 10);
            } else {
                return fail("escape unicode inválido");
            }
        }
        return true;
    }

    bool parseString(std::string& out) {
        out.clear();
        ++position_; // opening quote
        while (position_ < text_.size()) {
            const unsigned char ch = static_cast<unsigned char>(text_[position_++]);
            if (ch == '"') return true;
            if (ch == '\\') {
                if (position_ >= text_.size()) return fail("escape incompleto");
                const char escaped = text_[position_++];
                switch (escaped) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    uint32_t codepoint = 0;
                    if (!parseHex4(codepoint)) return false;
                    if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                        if (position_ + 1 >= text_.size() || text_[position_] != '\\' ||
                            text_[position_ + 1] != 'u') {
                            return fail("par substituto incompleto");
                        }
                        position_ += 2;
                        uint32_t low = 0;
                        if (!parseHex4(low)) return false;
                        if (low < 0xdc00 || low > 0xdfff) return fail("par substituto inválido");
                        codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                    } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                        return fail("par substituto inválido");
                    }
                    appendUtf8(out, codepoint);
                    break;
                }
                default: return fail("escape inválido");
                }
            } else if (ch < 0x20) {
                return fail("caractere de controle em string");
            } else {
                out.push_back(static_cast<char>(ch));
            }
        }
        return fail("string JSON incompleta");
    }

    std::string_view text_;
    size_t position_ = 0;
    std::string error_;
};

} // namespace

const Json* Json::get(std::string_view key) const {
    if (type != Type::Object) return nullptr;
    const auto it = object.find(std::string(key));
    return it == object.end() ? nullptr : &it->second;
}

std::string Json::stringOr(std::string_view key, const std::string& fallback) const {
    const Json* value = get(key);
    if (!value || !value->isString()) return fallback;
    return value->string;
}

double Json::numberOr(std::string_view key, double fallback) const {
    const Json* value = get(key);
    if (!value || value->type != Type::Number) return fallback;
    return value->number;
}

bool Json::boolOr(std::string_view key, bool fallback) const {
    const Json* value = get(key);
    if (!value || value->type != Type::Bool) return fallback;
    return value->boolean;
}

std::optional<Json> Json::parse(std::string_view text, std::string* error) {
    Parser parser(text);
    std::string localError;
    auto result = parser.parse(localError);
    if (!result && error) *error = localError;
    return result;
}

} // namespace ds
