#pragma once

#include <pep/async/OnEmscriptenThread.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-observe_on.hpp>

#include <concepts>
#include <utility>

namespace pep {

/// Create ReadableStream that will be fed with items emitted by the observable.
/// \returns https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream
emscripten::val CreateReadableStream(rxcpp::observable<emscripten::val> data);

/// Create ReadableStream that will be fed with items emitted by the observable.
/// The values will be emitted on the current thread.
template <typename TChunk, typename TSourceOperator>
requires (!std::same_as<TChunk, emscripten::val>)
emscripten::val CreateReadableStream(rxcpp::observable<TChunk, TSourceOperator> data) {
  return CreateReadableStream(data
    .observe_on(observe_on_emscripten_main_thread())
    //TODO `new` is workaround to not copy chunk, see https://github.com/emscripten-core/emscripten/issues/25412
    .map([](TChunk chunk) { return emscripten::val(new TChunk(std::move(chunk)), emscripten::allow_raw_pointers{}); }));
}

}
