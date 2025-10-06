# Async library: provides support for asynchronous programming
# using Boost ASIO and RX(CPP).

set(sources "")
if(DEFINED EMSCRIPTEN)
  list(APPEND sources
      EmscriptenThreadSwitch.cpp EmscriptenThreadSwitch.hpp
      EmscriptenValPtr.cpp EmscriptenValPtr.hpp
      OnEmscriptenThread.cpp OnEmscriptenThread.hpp
  )
endif()

add_library(${PROJECT_NAME}Asynclib STATIC
    ActivityMonitor.cpp ActivityMonitor.hpp
    CreateObservable.hpp
    FakeVoid.hpp
    IoContext_fwd.hpp
    IoContextThread.cpp IoContextThread.hpp
    ObservableAwaiter.hpp
    OnAsio.cpp OnAsio.hpp
    RxButFirst.hpp
    RxCache.hpp
    RxCartesianProduct.hpp
    RxDrain.hpp
    RxEnsureProgress.hpp
    RxFinallyExhaust.hpp
    RxIterate.hpp
    RxLazy.hpp
    RxParallelConcat.hpp
    RxTimeout.cpp RxTimeout.hpp
    RxUtils.cpp RxUtils.hpp
    SingleWorker.cpp SingleWorker.hpp
    WaitGroup.cpp WaitGroup.hpp
    WorkerPool.cpp WorkerPool.hpp
    WorkGuard.cpp WorkGuard.hpp
    ${sources}
)

target_link_libraries(${PROJECT_NAME}Asynclib
  ${PROJECT_NAME}Utilslib
  rxcpp  # This will be rxcpp::rxcpp if RxCpp will ever support C++20
)

verify_headers_standalone(${PROJECT_NAME}Asynclib)
add_unit_tests(Async)
