# Common files
add_library(${PROJECT_NAME}Webliblib
    Emscripten_fwd.hpp
    EmscriptenMapBinding.hpp
    EmscriptenValPtr.cpp EmscriptenValPtr.hpp
    EmscriptenVectorBinding.hpp
    ObservableByteStream.cpp ObservableByteStream.hpp
    ObservableStream.cpp ObservableStream.hpp
    ObservableStreamHelpers.cpp ObservableStreamHelpers.hpp
    OnEmscriptenThread.cpp OnEmscriptenThread.hpp
    ThreadPrintable.cpp ThreadPrintable.hpp
    WeblibApiPromise.cpp WeblibApiPromise.hpp
)
target_link_libraries(${PROJECT_NAME}Webliblib
    ${PROJECT_NAME}Asynclib
    nlohmann_json::nlohmann_json
    -lembind
)
verify_headers_standalone(${PROJECT_NAME}Webliblib)

# Executable
add_executable(${PROJECT_NAME}Weblib
    WeblibStructs.cpp WeblibStructs.hpp

    Weblib.cpp
)
find_package(nlohmann_json REQUIRED)
target_link_libraries(${PROJECT_NAME}Weblib
    # Make sure Embind types are registered by running all static constructors.
    # See https://emscripten.org/docs/porting/connecting_cpp_and_javascript/embind.html#binding-libraries
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,${PROJECT_NAME}Webliblib>"

    ${PROJECT_NAME}Applicationlib
    ${PROJECT_NAME}Clientlib
    ${PROJECT_NAME}OAuthClientlib

    -lidbfs.js
)
set(weblib_additional_files
    ${PROJECT_NAME}Weblib.d.mts
    ${PROJECT_NAME}Weblib.wasm
    ${PROJECT_NAME}Weblib.wasm.debug.wasm
    ${PROJECT_NAME}Weblib.wasm.map

    node_modules/
    package-lock.json
)
list(TRANSFORM weblib_additional_files PREPEND "$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/")
set_target_properties(${PROJECT_NAME}Weblib PROPERTIES
    ADDITIONAL_CLEAN_FILES "${weblib_additional_files}"
)

set_target_properties(${PROJECT_NAME}Weblib PROPERTIES
    SUFFIX .mjs
    PEP_SKIP_DEFAULT_EMSCRIPTEN_EXE_CONFIG ON
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/prejs.js"
)

set(PEP_WEBLIB_ENVIRONMENT "web,node" CACHE STRING "Environment(s) to build ${PROJECT_NAME}Weblib for")

# See https://emscripten.org/docs/tools_reference/settings_reference.html
target_link_options(${PROJECT_NAME}Weblib PRIVATE
    "-sENVIRONMENT=${PEP_WEBLIB_ENVIRONMENT}"
    -sSTRICT -sALLOW_UNIMPLEMENTED_SYSCALLS # Strict, but still allow stub syscalls
    LINKER:--no-demangle # Avoid huge .name section, see https://github.com/llvm/llvm-project/issues/126919
    -sEXIT_RUNTIME # Support running global destructors etc. We prevent exiting in main() via emscripten_exit_with_live_runtime()

    -sNO_PROXY_TO_PTHREAD # Default: We handle spawning a new thread ourselves, to make sure initialization has happened
    "-sPTHREAD_POOL_SIZE=Math.min(navigator.hardwareConcurrency, 8 /*WorkerPool*/ + 1 /*io_context*/ + 1 /*Boost async_resolve*/)"
    -sPTHREAD_POOL_DELAY_LOAD
    -sNO_PTHREAD_POOL_SIZE_STRICT # Disable this to allow spawning more threads, but we cannot immediately join them.

    -sINITIAL_MEMORY=209715200 # 200 MiB
    -sNO_ABORTING_MALLOC
    -sNO_SUPPORT_LONGJMP # We're not using this
    -sTEXTDECODER=2 # Require TextDecoder from browser
    -sFETCH
    -sNO_INVOKE_RUN # We call callMain ourselves
    # noExitRuntime required for -sNO_EXIT_RUNTIME
    -sINCOMING_MODULE_JS_API=thisProgram,noExitRuntime
    -sEXPORTED_RUNTIME_METHODS=FS,FS_createPreloadedFile,IDBFS,callMain,ExitStatus,getExceptionMessage,incrementExceptionRefcount,decrementExceptionRefcount
    "--emit-tsd=$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.d.mts"
    "$<$<CONFIG:Debug,RelWithDebInfo>:-Wno-error=limited-postlink-optimizations>"
    # -sRUNTIME_DEBUG: Debug logging for Emscripten runtime
    "$<$<CONFIG:Debug>:-sRUNTIME_DEBUG>"

    "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/prejs.js"
)

option(PEP_WEBLIB_NO_DYNAMIC_EXECUTION "Don't use eval & Function")
if(PEP_WEBLIB_NO_DYNAMIC_EXECUTION)
  target_link_options(${PROJECT_NAME}Weblib PRIVATE
      -sNO_DYNAMIC_EXECUTION
  )
endif()

# Avoid calling very slow wasm-emscripten-finalize during build
option(PEP_WEBLIB_NO_STACK_OVERFLOW_CHECK "Disable stack overflow check" ON)
if(PEP_WEBLIB_NO_STACK_OVERFLOW_CHECK)
  target_link_options(${PROJECT_NAME}Weblib PRIVATE
      -sNO_STACK_OVERFLOW_CHECK
  )
endif()

option(PEP_WEBLIB_NO_SEPARATE_DWARF "Don't put WebAssembly DWARF debug info in a separate .wasm.debug.wasm file, instead of in the .wasm file")
if(NOT PEP_WEBLIB_NO_SEPARATE_DWARF)
  target_link_options(${PROJECT_NAME}Weblib PRIVATE
      # -gseparate-dwarf: Separate debug info into .wasm.debug.wasm file
      "$<$<CONFIG:Debug,RelWithDebInfo>:-gseparate-dwarf>"
  )
else()
  # Prevent old debug info from remaining
  add_custom_command(
      TARGET ${PROJECT_NAME}Weblib POST_BUILD
      COMMENT "Removing any leftover -gseparate-dwarf"
      COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.wasm.debug.wasm"
      VERBATIM
  )
endif()

set(source_map_default ON)
if(EMSCRIPTEN_VERSION VERSION_GREATER_EQUAL "4.0.22" AND EMSCRIPTEN_VERSION VERSION_LESS "5.0.0")
  # Work around huge source map in debug mode: https://github.com/emscripten-core/emscripten/issues/26100
  set(source_map_default OFF)
endif()
option(PEP_WEBLIB_SOURCE_MAP "Generate source map for pepWeblib" ${source_map_default})
if(PEP_WEBLIB_SOURCE_MAP)
  target_link_options(${PROJECT_NAME}Weblib PRIVATE -gsource-map)
else()
  # Prevent old source map from remaining
  add_custom_command(
      TARGET ${PROJECT_NAME}Weblib POST_BUILD
      COMMENT "Removing any leftover source map"
      COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.wasm.map"
      VERBATIM
  )
endif()

add_custom_command(
    TARGET ${PROJECT_NAME}Weblib PRE_LINK
    COMMENT "Make sure typescript is installed for --emit-tsd"
    COMMAND ${CMAKE_COMMAND} -E env "EMSCRIPTEN=${EMSCRIPTEN_ROOT_PATH}" -- ${INVOKE_SH} "${CMAKE_CURRENT_SOURCE_DIR}/install-emscripten-typescript.sh"
    VERBATIM
)

file(GENERATE OUTPUT "$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/package.json" INPUT package.in.json)

# Unit tests
add_unit_tests(Weblib)
if(WITH_TESTS)
  target_link_options(${PROJECT_NAME}WeblibUnitTests PRIVATE
      -sPROXY_TO_PTHREAD # See ./tests/PromiseHelpers.cpp
  )
endif()
