#pragma once

#include <pep/application/CommandLineHelp.hpp>
#include <pep/application/CommandLineValue.hpp>
#include <pep/application/CommandLineValueParser.hpp>

#include <filesystem>
#include <cassert>
#include <type_traits>

namespace pep {
namespace commandline {

enum class ArgValueType {
  String, // Generic
  File,
  Directory,
};

class ValueSpecificationBase {
protected:
  bool mPositional = false;
  bool mRequired = false;
  bool mMultiple = false;
  bool mEatAll = false;

public:
  ValueSpecificationBase() = default;
  ValueSpecificationBase(const ValueSpecificationBase& other) = default;
  ValueSpecificationBase& operator=(const ValueSpecificationBase& other) = default;
  ValueSpecificationBase(ValueSpecificationBase&& other) = default;
  ValueSpecificationBase& operator=(ValueSpecificationBase&& other) = default;
  virtual ~ValueSpecificationBase() = default;

  inline bool isPositional() const noexcept { return mPositional; }
  inline bool isRequired() const noexcept { return mRequired; }
  inline bool allowsMultiple() const noexcept { return mMultiple; }
  inline bool eatsAll() const noexcept { return mEatAll; }

  virtual ArgValueType getType() const noexcept { return ArgValueType::String; }
  /*!
   * \brief Get string representation of default value and its description
   */
  virtual std::optional<std::pair<std::string, std::optional<std::string>>> getDefault() const noexcept { return {}; }
  virtual std::vector<std::string> getSuggested() const noexcept { return {}; }

  virtual std::any parse(const std::string& specified) const = 0;
  virtual void finalize(Values& destination) const = 0;

  virtual void writeHelpText(std::ostream& destination) const = 0;
};


namespace detail {
template <typename T, typename = void>
struct format {
  std::string operator()(const T& v) {
    std::ostringstream ss;
    ss << v;
    return std::move(ss).str();
  }
};

template <typename R, typename P>
struct format<std::chrono::duration<R, P>> {
  std::string operator()(const std::chrono::duration<R, P>& v) {
    return chrono::ToString(v);
  }
};

template <>
struct format<std::filesystem::path> {
  inline std::string operator()(const std::filesystem::path& v) {
    return v.string(); // Format without quotes
  }
};

// Shared functionality between ValueSpecification<T> and specialized versions of ValueSpecification
template <typename Derived, typename T>
class ValueSpecificationTemplate : public ValueSpecificationBase {
  friend Derived;
  explicit ValueSpecificationTemplate(ArgValueType type) : mType{type} {
    static_assert(std::is_base_of_v<ValueSpecificationTemplate, Derived>);
  }

protected:
  ArgValueType mType;
  std::optional<T> mDefault;
  std::optional<std::string> mDefaultDescription;
  std::optional<std::vector<T>> mAllowed;
  std::vector<T> mSuggested;

  [[nodiscard]] Derived enableFlag(bool ValueSpecificationTemplate::*field, const std::string& description) const;
  bool allows(const T& value) const;
  void addAllowedValue(const T& value);

public:
  ArgValueType getType() const noexcept override { return mType; }

  std::optional<std::pair<std::string, std::optional<std::string>>> getDefault() const noexcept override {
    return mDefault.has_value() ? std::optional{std::pair{detail::format<T>()(*mDefault), mDefaultDescription}} : std::nullopt;
  }
  std::vector<std::string> getSuggested() const noexcept override;

  void validate() const;

  [[nodiscard]] Derived required() const;
  [[nodiscard]] Derived positional() const { return this->enableFlag(&ValueSpecificationTemplate::mPositional, "positional"); }
  [[nodiscard]] Derived multiple() const { return this->enableFlag(&ValueSpecificationTemplate::mMultiple, "multiple"); }
  [[nodiscard]] Derived eatAll() const { return this->positional().enableFlag(&ValueSpecificationTemplate::mEatAll, "eat all"); }

  [[nodiscard]] Derived defaultsTo(const T& value, const std::optional<std::string>& description = std::nullopt) const;

  [[nodiscard]] Derived allow(const T& value) const;

  template <typename TContainer>
  [[nodiscard]] Derived allow(const TContainer& values) const;

  [[nodiscard]] Derived suggest(const T& value) const;

  std::any parse(const std::string& specified) const override;
  void finalize(Values& parsed) const override;

  void writeHelpText(std::ostream& destination) const override;
};

template <typename Derived, typename T>
Derived ValueSpecificationTemplate<Derived, T>::enableFlag(bool ValueSpecificationTemplate<Derived, T>::*field, const std::string& description) const {
  Derived result = static_cast<const Derived&>(*this);
  assert(!(result.*field) && "Flag already set");
  result.*field = true;
  return result;
}

template <typename Derived, typename T>
std::vector<std::string> ValueSpecificationTemplate<Derived, T>::getSuggested() const noexcept {
  std::vector<std::string> result;
  if (mDefault) {
    result.emplace_back(detail::format<T>()(*mDefault));
  }
  for (const auto& v : mSuggested) {
    std::string s = detail::format<T>()(v);
    if (std::find(result.cbegin(), result.cend(), s) == result.end()) {
      result.emplace_back(s);
    }
  }
  if (mAllowed) {
    for (const auto& v : *mAllowed) {
      std::string s = detail::format<T>()(v);
      if (std::find(result.cbegin(), result.cend(), s) == result.end()) {
        result.emplace_back(s);
      }
    }
  }
  return result;
}

template <typename Derived, typename T>
void ValueSpecificationTemplate<Derived, T>::validate() const {
  if (mDefault.has_value()) {
    if (!this->allows(*mDefault)) {
      throw std::runtime_error("Value specification does not allow its own default value");
    }
  }
  for (const auto& suggest : mSuggested) {
    if (!this->allows(suggest)) {
      throw std::runtime_error("Value specification does not allow one of its suggested values");
    }
  }
}

template <typename Derived, typename T>
Derived ValueSpecificationTemplate<Derived, T>::required() const {
  assert(!mDefault.has_value());
  return this->enableFlag(&ValueSpecificationTemplate::mRequired, "required");
}

template <typename Derived, typename T>
Derived ValueSpecificationTemplate<Derived, T>::defaultsTo(const T& value, const std::optional<std::string>& description) const {
  Derived result = static_cast<const Derived&>(*this);

  assert(!result.isRequired());
  assert(!result.mDefault.has_value());
  assert(!result.mDefaultDescription.has_value());

  result.mDefault = value;
  result.mDefaultDescription = description;
  return result;
}

template <typename Derived, typename T>
bool ValueSpecificationTemplate<Derived, T>::allows(const T& value) const {
  if (!mAllowed.has_value()) {
    return true;
  }
  auto end = mAllowed->cend();
  return std::find(mAllowed->cbegin(), end, value) != end;
}

template <typename Derived, typename T>
void ValueSpecificationTemplate<Derived, T>::addAllowedValue(const T& value) {
  if (!mAllowed.has_value()) {
    mAllowed = { value };
  }
  else {
    assert(!this->allows(value));
    mAllowed->push_back(value);
  }
}

template <typename Derived, typename T>
Derived ValueSpecificationTemplate<Derived, T>::allow(const T& value) const {
  Derived result = static_cast<const Derived&>(*this);
  result.addAllowedValue(value);
  return result;
}

template <typename Derived, typename T>
template <typename TContainer>
Derived ValueSpecificationTemplate<Derived, T>::allow(const TContainer& values) const {
  Derived result = static_cast<const Derived&>(*this);
  auto end = values.end();
  for (auto i = values.begin(); i != end; ++i) {
    result.addAllowedValue(*i);
  }
  return result;
}

template <typename Derived, typename T>
Derived ValueSpecificationTemplate<Derived, T>::suggest(const T& value) const {
  Derived result = static_cast<const Derived&>(*this);
  result.mSuggested.emplace_back(value);
  return result;
}

template <typename Derived, typename T>
std::any ValueSpecificationTemplate<Derived, T>::parse(const std::string& specified) const {
  auto result = ValueParser<T>()(specified); // TODO: raise something decent if parsing fails
  if (!this->allows(result)) {
    throw std::runtime_error("Value \"" + specified + "\" is not allowed");
  }
  return result;
}


template <typename Derived, typename T>
void ValueSpecificationTemplate<Derived, T>::finalize(Values& destination) const {
  if (destination.empty()) {
    if (mDefault.has_value()) {
      assert(this->allows(*mDefault));
      destination.add(*mDefault);
    }
    else if (this->isRequired()) {
      throw std::runtime_error("No value specified");
    }
  }
}

template <typename Derived, typename T>
void ValueSpecificationTemplate<Derived, T>::writeHelpText(std::ostream& destination) const {
  if (mAllowed.has_value()) {

    // boost::algorithm::join won't work if T is not std::string
    std::string delimiter;
    std::ostringstream values;
    for (const auto& entry : *mAllowed) {
      values << delimiter << format<T>()(entry);
      delimiter = ", ";
    }
    WriteHelpItemSupplement(destination, "Value must be one of: " + std::move(values).str());
  }
  if (mDefault.has_value()) {
    std::ostringstream text;
    if (mDefaultDescription.has_value()) {
      text << *mDefaultDescription << " (" << format<T>()(*mDefault) << ')';
    }
    else {
      text << format<T>()(*mDefault);
    }
    WriteHelpItemSupplement(destination, "Value defaults to " + std::move(text).str());
  }
}

} // namespace detail


// Specification of the value(s) supported/expected by a command line switch
template <typename T>
class ValueSpecification : public detail::ValueSpecificationTemplate<ValueSpecification<T>, T> {
public:
  ValueSpecification() : detail::ValueSpecificationTemplate<ValueSpecification, T>(ArgValueType::String) {}
};

// Specialization for files
template <>
class ValueSpecification<std::filesystem::path> : public detail::ValueSpecificationTemplate<ValueSpecification<std::filesystem::path>, std::filesystem::path> {
public:
  ValueSpecification() : detail::ValueSpecificationTemplate<ValueSpecification, std::filesystem::path>(ArgValueType::File) {}

  ValueSpecification directory() const {
    ValueSpecification result = *this;
    assert(result.mType != ArgValueType::Directory && "Already marked as directory");
    result.mType = ArgValueType::Directory;
    return result;
  }
};

// Convenience alias
template <typename T>
using Value = ValueSpecification<T>;

}
}
