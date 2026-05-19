#include "mosaic/Json.h"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <stdexcept>

namespace mosaic::json {

namespace {

class Parser {
public:
  explicit Parser(std::string_view t) : text_(t) {}

  Value parse() {
    skipWs();
    Value v = parseValue();
    skipWs();
    if (pos_ != text_.size()) {
      throw std::runtime_error("json: trailing garbage at offset " + std::to_string(pos_));
    }
    return v;
  }

private:
  std::string_view text_;
  std::size_t pos_ = 0;

  void skipWs() {
    while (pos_ < text_.size()) {
      char c = text_[pos_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
      else break;
    }
  }

  [[noreturn]] void fail(std::string const& msg) {
    throw std::runtime_error("json: " + msg + " at offset " + std::to_string(pos_));
  }

  char peek() {
    if (pos_ >= text_.size()) fail("unexpected EOF");
    return text_[pos_];
  }

  bool consumeLit(std::string_view lit) {
    if (text_.size() - pos_ < lit.size()) return false;
    if (text_.substr(pos_, lit.size()) != lit) return false;
    pos_ += lit.size();
    return true;
  }

  Value parseValue() {
    skipWs();
    char c = peek();
    switch (c) {
      case '{': return parseObject();
      case '[': return parseArray();
      case '"': return Value(parseString());
      case 't':
        if (!consumeLit("true")) fail("expected 'true'");
        return Value(true);
      case 'f':
        if (!consumeLit("false")) fail("expected 'false'");
        return Value(false);
      case 'n':
        if (!consumeLit("null")) fail("expected 'null'");
        return Value();
      default:
        if (c == '-' || (c >= '0' && c <= '9')) return Value(parseNumber());
        fail(std::string("unexpected char '") + c + "'");
    }
  }

  Value parseObject() {
    if (text_[pos_] != '{') fail("expected '{'");
    ++pos_;
    Object obj;
    skipWs();
    if (pos_ < text_.size() && text_[pos_] == '}') { ++pos_; return Value(std::move(obj)); }
    while (true) {
      skipWs();
      std::string key = parseString();
      skipWs();
      if (pos_ >= text_.size() || text_[pos_] != ':') fail("expected ':'");
      ++pos_;
      Value v = parseValue();
      obj.emplace(std::move(key), std::move(v));
      skipWs();
      if (pos_ >= text_.size()) fail("unexpected EOF in object");
      if (text_[pos_] == ',') { ++pos_; continue; }
      if (text_[pos_] == '}') { ++pos_; break; }
      fail("expected ',' or '}'");
    }
    return Value(std::move(obj));
  }

  Value parseArray() {
    if (text_[pos_] != '[') fail("expected '['");
    ++pos_;
    Array arr;
    skipWs();
    if (pos_ < text_.size() && text_[pos_] == ']') { ++pos_; return Value(std::move(arr)); }
    while (true) {
      arr.push_back(parseValue());
      skipWs();
      if (pos_ >= text_.size()) fail("unexpected EOF in array");
      if (text_[pos_] == ',') { ++pos_; continue; }
      if (text_[pos_] == ']') { ++pos_; break; }
      fail("expected ',' or ']'");
    }
    return Value(std::move(arr));
  }

  std::string parseString() {
    if (text_[pos_] != '"') fail("expected '\"'");
    ++pos_;
    std::string out;
    while (pos_ < text_.size()) {
      char c = text_[pos_++];
      if (c == '"') return out;
      if (c != '\\') { out += c; continue; }
      if (pos_ >= text_.size()) fail("unterminated escape");
      char e = text_[pos_++];
      switch (e) {
        case '"':  out += '"'; break;
        case '\\': out += '\\'; break;
        case '/':  out += '/'; break;
        case 'b':  out += '\b'; break;
        case 'f':  out += '\f'; break;
        case 'n':  out += '\n'; break;
        case 'r':  out += '\r'; break;
        case 't':  out += '\t'; break;
        case 'u': {
          if (text_.size() - pos_ < 4) fail("bad \\u escape");
          unsigned code = 0;
          for (int i = 0; i < 4; ++i) {
            char h = text_[pos_++];
            code <<= 4;
            if (h >= '0' && h <= '9') code |= (h - '0');
            else if (h >= 'a' && h <= 'f') code |= (h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') code |= (h - 'A' + 10);
            else fail("bad hex in \\u escape");
          }
          // Emit UTF-8; surrogate pairs ignored (fine for ASCII titles).
          if (code < 0x80) {
            out += static_cast<char>(code);
          } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
          } else {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
          }
          break;
        }
        default: fail(std::string("bad escape '\\") + e + "'");
      }
    }
    fail("unterminated string");
  }

  double parseNumber() {
    std::size_t start = pos_;
    if (text_[pos_] == '-') ++pos_;
    while (pos_ < text_.size()) {
      char c = text_[pos_];
      if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') ++pos_;
      else break;
    }
    std::string_view tok = text_.substr(start, pos_ - start);
    double v = 0.0;
    auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), v);
    if (ec != std::errc{}) fail("bad number '" + std::string(tok) + "'");
    return v;
  }
};

[[noreturn]] void badKind(char const* expected, Value::Kind got) {
  throw std::runtime_error(std::string("json: expected ") + expected +
                            " but got kind " + std::to_string(static_cast<int>(got)));
}

}  // namespace

bool Value::asBool() const {
  if (!isBool()) badKind("bool", kind_);
  return bool_;
}
double Value::asNumber() const {
  if (!isNumber()) badKind("number", kind_);
  return number_;
}
std::string const& Value::asString() const {
  if (!isString()) badKind("string", kind_);
  return string_;
}
Array const& Value::asArray() const {
  if (!isArray()) badKind("array", kind_);
  return array_;
}
Object const& Value::asObject() const {
  if (!isObject()) badKind("object", kind_);
  return object_;
}

Value const* Value::find(std::string_view key) const {
  if (!isObject()) return nullptr;
  auto it = object_.find(key);
  return it == object_.end() ? nullptr : &it->second;
}

Value const* Value::find(std::size_t idx) const {
  if (!isArray() || idx >= array_.size()) return nullptr;
  return &array_[idx];
}

Value const& Value::at(std::string_view key) const {
  Value const* v = find(key);
  if (!v) throw std::runtime_error("json: missing key '" + std::string(key) + "'");
  return *v;
}

Value const& Value::at(std::size_t idx) const {
  Value const* v = find(idx);
  if (!v) throw std::runtime_error("json: index " + std::to_string(idx) + " out of range");
  return *v;
}

Value const* Value::pick(std::string_view path) const {
  Value const* cur = this;
  std::size_t pos = 0;
  while (pos < path.size() && cur) {
    auto slash = path.find('/', pos);
    std::string_view seg = path.substr(pos, slash == std::string_view::npos ? path.size() - pos : slash - pos);
    pos = (slash == std::string_view::npos) ? path.size() : slash + 1;
    if (seg.empty()) continue;

    if (cur->isArray()) {
      std::size_t idx = 0;
      auto [ptr, ec] = std::from_chars(seg.data(), seg.data() + seg.size(), idx);
      if (ec != std::errc{}) return nullptr;
      cur = cur->find(idx);
    } else {
      cur = cur->find(seg);
    }
  }
  return cur;
}

Value parse(std::string_view text) {
  return Parser(text).parse();
}

}  // namespace mosaic::json
