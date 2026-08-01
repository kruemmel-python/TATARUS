#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace tatarus::json {

class Value {
public:
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value>;
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    Value() : value_(nullptr) {}
    Value(std::nullptr_t) : value_(nullptr) {}
    Value(bool value) : value_(value) {}
    Value(int value) : value_(static_cast<double>(value)) {}
    Value(std::uint64_t value) : value_(static_cast<double>(value)) {}
    Value(double value) : value_(value) {}
    Value(const char* value) : value_(std::string(value)) {}
    Value(std::string value) : value_(std::move(value)) {}
    Value(Array value) : value_(std::move(value)) {}
    Value(Object value) : value_(std::move(value)) {}

    [[nodiscard]] bool isNull() const { return std::holds_alternative<std::nullptr_t>(value_); }
    [[nodiscard]] bool isBool() const { return std::holds_alternative<bool>(value_); }
    [[nodiscard]] bool isNumber() const { return std::holds_alternative<double>(value_); }
    [[nodiscard]] bool isString() const { return std::holds_alternative<std::string>(value_); }
    [[nodiscard]] bool isArray() const { return std::holds_alternative<Array>(value_); }
    [[nodiscard]] bool isObject() const { return std::holds_alternative<Object>(value_); }

    [[nodiscard]] bool boolean() const { return get<bool>("boolean"); }
    [[nodiscard]] double number() const { return get<double>("number"); }
    [[nodiscard]] const std::string& string() const { return get<std::string>("string"); }
    [[nodiscard]] const Array& array() const { return get<Array>("array"); }
    [[nodiscard]] Array& array() { return get<Array>("array"); }
    [[nodiscard]] const Object& object() const { return get<Object>("object"); }
    [[nodiscard]] Object& object() { return get<Object>("object"); }

    [[nodiscard]] bool contains(std::string_view key) const {
        return isObject() && object().contains(std::string(key));
    }
    [[nodiscard]] const Value& at(std::string_view key) const {
        if (!isObject()) throw std::runtime_error("JSON value is not an object");
        const auto it = object().find(std::string(key));
        if (it == object().end()) throw std::runtime_error("Missing JSON field: " + std::string(key));
        return it->second;
    }
    Value& operator[](std::string key) {
        if (isNull()) value_ = Object{};
        if (!isObject()) throw std::runtime_error("JSON value is not an object");
        return object()[std::move(key)];
    }

    [[nodiscard]] std::string dump(int indent = -1) const {
        std::ostringstream out;
        write(out, *this, indent, 0);
        return out.str();
    }

    static Value parse(std::string_view source) {
        Parser parser(source);
        Value result = parser.value();
        parser.space();
        if (!parser.done()) throw std::runtime_error("Unexpected trailing JSON data");
        return result;
    }

private:
    Storage value_;

    template <typename T>
    const T& get(const char* expected) const {
        const T* value = std::get_if<T>(&value_);
        if (value == nullptr) throw std::runtime_error(std::string("JSON value is not a ") + expected);
        return *value;
    }
    template <typename T>
    T& get(const char* expected) {
        T* value = std::get_if<T>(&value_);
        if (value == nullptr) throw std::runtime_error(std::string("JSON value is not a ") + expected);
        return *value;
    }

    static void escaped(std::ostream& out, const std::string& value) {
        out << '"';
        for (const unsigned char c : value) {
            switch (c) {
                case '"': out << "\\\""; break;
                case '\\': out << "\\\\"; break;
                case '\b': out << "\\b"; break;
                case '\f': out << "\\f"; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                default:
                    if (c < 0x20) {
                        out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                            << static_cast<int>(c) << std::dec << std::setfill(' ');
                    } else {
                        out << static_cast<char>(c);
                    }
            }
        }
        out << '"';
    }

    static void write(std::ostream& out, const Value& value, int indent, int depth) {
        const auto newline = [&](int d) {
            if (indent >= 0) out << '\n' << std::string(static_cast<std::size_t>(d * indent), ' ');
        };
        if (value.isNull()) out << "null";
        else if (value.isBool()) out << (value.boolean() ? "true" : "false");
        else if (value.isNumber()) {
            if (!std::isfinite(value.number())) throw std::runtime_error("Cannot serialize non-finite JSON number");
            out << std::setprecision(17) << value.number();
        } else if (value.isString()) escaped(out, value.string());
        else if (value.isArray()) {
            out << '[';
            for (std::size_t i = 0; i < value.array().size(); ++i) {
                if (i != 0) out << ',';
                newline(depth + 1);
                write(out, value.array()[i], indent, depth + 1);
            }
            if (!value.array().empty()) newline(depth);
            out << ']';
        } else {
            out << '{';
            std::size_t i = 0;
            for (const auto& [key, child] : value.object()) {
                if (i++ != 0) out << ',';
                newline(depth + 1);
                escaped(out, key);
                out << (indent >= 0 ? ": " : ":");
                write(out, child, indent, depth + 1);
            }
            if (!value.object().empty()) newline(depth);
            out << '}';
        }
    }

    class Parser {
    public:
        explicit Parser(std::string_view source) : source_(source) {}
        void space() { while (position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_]))) ++position_; }
        [[nodiscard]] bool done() const { return position_ == source_.size(); }
        Value value() {
            space();
            if (done()) fail("Unexpected end of JSON");
            const char c = source_[position_];
            if (c == '{') return objectValue();
            if (c == '[') return arrayValue();
            if (c == '"') return stringValue();
            if (c == 't') return literal("true", Value(true));
            if (c == 'f') return literal("false", Value(false));
            if (c == 'n') return literal("null", Value(nullptr));
            if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return numberValue();
            fail("Invalid JSON token");
        }
    private:
        std::string_view source_;
        std::size_t position_ = 0;
        [[noreturn]] void fail(const std::string& message) const {
            throw std::runtime_error(message + " at byte " + std::to_string(position_));
        }
        bool take(char c) { space(); if (position_ < source_.size() && source_[position_] == c) { ++position_; return true; } return false; }
        void require(char c) { if (!take(c)) fail(std::string("Expected '") + c + "'"); }
        Value literal(std::string_view token, Value result) {
            if (source_.substr(position_, token.size()) != token) fail("Invalid JSON literal");
            position_ += token.size(); return result;
        }
        static void appendUtf8(std::string& out, unsigned codepoint) {
            if (codepoint <= 0x7f) out.push_back(static_cast<char>(codepoint));
            else if (codepoint <= 0x7ff) { out.push_back(static_cast<char>(0xc0 | (codepoint >> 6))); out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f))); }
            else { out.push_back(static_cast<char>(0xe0 | (codepoint >> 12))); out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f))); out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f))); }
        }
        Value stringValue() {
            require('"'); std::string out;
            while (position_ < source_.size()) {
                char c = source_[position_++];
                if (c == '"') return Value(std::move(out));
                if (static_cast<unsigned char>(c) < 0x20) fail("Control character in string");
                if (c != '\\') { out.push_back(c); continue; }
                if (position_ >= source_.size()) fail("Incomplete escape");
                c = source_[position_++];
                switch (c) {
                    case '"': out.push_back('"'); break; case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break; case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break; case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break; case 't': out.push_back('\t'); break;
                    case 'u': {
                        if (position_ + 4 > source_.size()) fail("Incomplete unicode escape");
                        unsigned cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            const char h = source_[position_++]; cp <<= 4;
                            if (h >= '0' && h <= '9') cp += h - '0';
                            else if (h >= 'a' && h <= 'f') cp += h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp += h - 'A' + 10;
                            else fail("Invalid unicode escape");
                        }
                        appendUtf8(out, cp); break;
                    }
                    default: fail("Invalid string escape");
                }
            }
            fail("Unterminated string");
        }
        Value numberValue() {
            const std::size_t start = position_;
            if (source_[position_] == '-') ++position_;
            if (position_ >= source_.size()) fail("Incomplete number");
            if (source_[position_] == '0') ++position_;
            else { if (!std::isdigit(static_cast<unsigned char>(source_[position_]))) fail("Invalid number"); while (position_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[position_]))) ++position_; }
            if (position_ < source_.size() && source_[position_] == '.') { ++position_; if (position_ >= source_.size() || !std::isdigit(static_cast<unsigned char>(source_[position_]))) fail("Invalid fraction"); while (position_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[position_]))) ++position_; }
            if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) { ++position_; if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) ++position_; if (position_ >= source_.size() || !std::isdigit(static_cast<unsigned char>(source_[position_]))) fail("Invalid exponent"); while (position_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[position_]))) ++position_; }
            const std::string token(source_.substr(start, position_ - start));
            const double result = std::stod(token);
            if (!std::isfinite(result)) fail("Non-finite number");
            return Value(result);
        }
        Value arrayValue() {
            require('['); Array result; if (take(']')) return Value(std::move(result));
            do { result.push_back(value()); } while (take(',')); require(']'); return Value(std::move(result));
        }
        Value objectValue() {
            require('{'); Object result; if (take('}')) return Value(std::move(result));
            do { space(); if (done() || source_[position_] != '"') fail("Object key must be a string"); const std::string key = stringValue().string(); require(':'); if (!result.emplace(key, value()).second) fail("Duplicate object key"); } while (take(','));
            require('}'); return Value(std::move(result));
        }
    };
};

inline const std::string& requiredString(const Value& object, std::string_view key) { return object.at(key).string(); }
inline double requiredNumber(const Value& object, std::string_view key) { return object.at(key).number(); }

}  // namespace tatarus::json
