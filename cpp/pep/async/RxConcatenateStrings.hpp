#pragma once

#include <memory>
#include <sstream>
#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

namespace pep {

/// Concatenates strings
struct RxConcatenateStrings {
  /// \return An observable emitting a single string
  template <typename SourceOperator>
  auto operator()(rxcpp::observable<std::string, SourceOperator> items) const {
    return items.reduce(
      std::make_shared<std::ostringstream>(), // Needs shared for now because otherwise rxcpp will make copies...
      [](std::shared_ptr<std::ostringstream> result, const std::string& item) {
      *result << item;
      return result;
    }, [](const std::shared_ptr<std::ostringstream>& result) {
      return std::move(*result).str();
    });
  }
};

}
