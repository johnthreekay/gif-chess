#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace mosaic::json {

class Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value, std::less<>>;

class Value {
public:
  enum class Kind { Null, Bool, Number, String, Array, Object };

  Value() = default;
  explicit Value(bool b) : kind_(Kind::Bool), bool_(b) {}
  explicit Value(double n) : kind_(Kind::Number), number_(n) {}
  explicit Value(std::string s) : kind_(Kind::String), string_(std::move(s)) {}
  explicit Value(Array a) : kind_(Kind::Array), array_(std::move(a)) {}
  explicit Value(Object o) : kind_(Kind::Object), object_(std::move(o)) {}

  Kind kind() const { return kind_; }
  bool isNull()   const { return kind_ == Kind::Null; }
  bool isBool()   const { return kind_ == Kind::Bool; }
  bool isNumber() const { return kind_ == Kind::Number; }
  bool isString() const { return kind_ == Kind::String; }
  bool isArray()  const { return kind_ == Kind::Array; }
  bool isObject() const { return kind_ == Kind::Object; }

  bool               asBool()   const;
  double             asNumber() const;
  std::string const& asString() const;
  Array  const&      asArray()  const;
  Object const&      asObject() const;

  // Safe accessors. Return nullptr when missing or wrong kind.
  Value const* find(std::string_view key) const;
  Value const* find(std::size_t idx) const;

  // Throwing accessors.
  Value const& at(std::string_view key) const;
  Value const& at(std::size_t idx) const;

  // Walk a "/"-separated path; integer segments index into arrays.
  // Example: "results/0/media_formats/mp4/url"
  Value const* pick(std::string_view path) const;

private:
  Kind kind_ = Kind::Null;
  bool bool_ = false;
  double number_ = 0.0;
  std::string string_;
  Array array_;
  Object object_;
};

// Parses `text` into a Value tree. Throws std::runtime_error on syntax errors.
Value parse(std::string_view text);

}  // namespace mosaic::json
