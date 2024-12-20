# Async library: provides support for asynchronous programming
# using Boost ASIO and RX(CPP).

add_library(${PROJECT_NAME}Asynclib STATIC
    ActivityMonitor.cpp ActivityMonitor.hpp
    AsioReadBuffer.cpp AsioReadBuffer.hpp
    CreateObservable.hpp
    FakeVoid.hpp
    IOServiceThread.cpp IOServiceThread.hpp
    OnAsio.cpp OnAsio.hpp
    RxButFirst.hpp
    RxCache.hpp
    RxCartesianProduct.hpp
    RxEnsureProgress.hpp
    RxFinallyExhaust.hpp
    RxIterate.hpp
    RxLazy.hpp
    RxParallelConcat.hpp
    RxTestUtils.hpp
    RxTimeout.cpp RxTimeout.hpp
    RxUtils.cpp RxUtils.hpp
    SingleWorker.cpp SingleWorker.hpp
    WaitGroup.cpp WaitGroup.hpp
    WorkerPool.cpp WorkerPool.hpp
)

target_link_libraries(${PROJECT_NAME}Asynclib
  ${PROJECT_NAME}Utilslib
  rxcpp  # This will be rxcpp::rxcpp if RxCpp will ever support C++20
)

add_unit_tests(Async)
