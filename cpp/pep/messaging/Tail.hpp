#pragma once

#include <rxcpp/rx-lite.hpp>

namespace pep::messaging {

template <typename T>
using TailSegment = rxcpp::observable<T>;

template <typename T>
using Tail = rxcpp::observable<TailSegment<T>>;

template <typename T>
TailSegment<T> MakeTailSegment(T message) {
  return rxcpp::observable<>::just(std::move(message));
}

template <typename T>
Tail<T> MakeSingletonTail(T message) {
  return rxcpp::observable<>::just(MakeTailSegment(std::move(message)));
}

template <typename T>
Tail<T> MakeEmptyTail() {
  return rxcpp::observable<>::empty<messaging::TailSegment<T>>();
}

}
