# Async library: provides support for asynchronous programming
# using Boost ASIO and RX(CPP).

add_library(${PROJECT_NAME}Asynclib STATIC
    ActivityMonitor.cpp ActivityMonitor.hpp
    CreateObservable.hpp
    FakeVoid.hpp
    IoContext_fwd.hpp
    IoContextThread.cpp IoContextThread.hpp
    OnAsio.cpp OnAsio.hpp
    RxButFirst.hpp
    RxCache.hpp
    RxCartesianProduct.hpp
    RxDrain.hpp
    RxEnsureProgress.hpp
    RxFinallyExhaust.hpp
    RxLazy.hpp
    RxParallelConcat.hpp
    RxTimeout.cpp RxTimeout.hpp
    RxUtils.cpp RxUtils.hpp
    SingleWorker.cpp SingleWorker.hpp
    WaitGroup.cpp WaitGroup.hpp
    WorkerPool.cpp WorkerPool.hpp
    WorkGuard.cpp WorkGuard.hpp
)

target_link_libraries(${PROJECT_NAME}Asynclib
  ${PROJECT_NAME}Utilslib
  rxcpp  # This will be rxcpp::rxcpp if RxCpp will ever support C++20
)

verify_headers_standalone(${PROJECT_NAME}Asynclib)
add_unit_tests(Async)
