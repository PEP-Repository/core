#pragma once

#include <pep/async/FakeVoid.hpp>
#include <pep/async/RxGetOne.hpp>
#include <boost/core/demangle.hpp>

namespace pep::messaging {

template <bool lossy = false>
struct ResponseToVoid {
  template <typename TResponse, typename SourceOperator>
  rxcpp::observable<FakeVoid> operator()(rxcpp::observable<TResponse, SourceOperator> items) const {
    static_assert(lossy || sizeof(TResponse) == sizeof(FakeVoid), "Losing information from non-empty response message");

    return items
      .op(RxGetOne(boost::core::demangle(typeid(TResponse).name())))
      .map([](const TResponse&) {return FakeVoid(); });
  }
};

}
