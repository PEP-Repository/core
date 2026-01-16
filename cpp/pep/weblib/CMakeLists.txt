# Common files
add_library(${PROJECT_NAME}Webliblib
    EmscriptenJsonConversion.cpp EmscriptenJsonConversion.hpp
    EmscriptenMapBinding.hpp
    EmscriptenValPtr.cpp EmscriptenValPtr.hpp
    EmscriptenVectorBinding.hpp
    ObservableByteStream.cpp ObservableByteStream.hpp
    ObservableStream.cpp ObservableStream.hpp
    ObservableStreamHelpers.cpp ObservableStreamHelpers.hpp
    OnEmscriptenThread.cpp OnEmscriptenThread.hpp
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
    "$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.d.mts"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.wasm"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.wasm.debug.wasm"
)
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
    -sTEXTDECODER=2 # Require TextDecoder from browser
    -sFETCH
    -sNO_INVOKE_RUN # We call callMain ourselves
    # noExitRuntime required for -sNO_EXIT_RUNTIME
    -sINCOMING_MODULE_JS_API=thisProgram,noExitRuntime
    -sEXPORTED_RUNTIME_METHODS=FS,FS_createPreloadedFile,IDBFS,callMain,ExitStatus,getExceptionMessage,incrementExceptionRefcount,decrementExceptionRefcount
    "--emit-tsd=$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.d.mts"
    # -gseparate-dwarf: Separate debug info into .wasm.debug.wasm file
    # -sRUNTIME_DEBUG: Debug logging for Emscripten runtime
    "$<$<CONFIG:Debug,RelWithDebInfo>:-gseparate-dwarf;-Wno-error=limited-postlink-optimizations>"
    "$<$<CONFIG:Debug>:-sRUNTIME_DEBUG>"

    "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/prejs.js"
)

# Disabled by default because of https://github.com/emscripten-core/emscripten/issues/26100
option(PEP_WEBLIB_SOURCE_MAP "Generate source map for pepWeblib")
if(PEP_WEBLIB_SOURCE_MAP)
  target_link_options(${PROJECT_NAME}Weblib PRIVATE -gsource-map)
else()
  # Prevent old source map from remaining and being picked up
  add_custom_command(
      TARGET ${PROJECT_NAME}Weblib POST_BUILD
      COMMENT "Removing any leftover source map"
      COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.wasm.map"
      VERBATIM
  )
endif()

# tsc isn't installed by default as it's in EMSDK devDependencies but we need it for --emit-tsd,
# see related https://github.com/emscripten-core/emsdk/issues/1370
find_program(NPM_PROGRAM npm)
add_custom_command(
    TARGET ${PROJECT_NAME}Weblib PRE_BUILD
    COMMENT "Make sure typescript is installed"
    COMMAND ${NPM_PROGRAM} install --no-audit --omit=dev --save-prod typescript
    WORKING_DIRECTORY "$ENV{EMSDK}/upstream/emscripten/"
    VERBATIM
)

file(GENERATE OUTPUT "$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/package.json" INPUT package.in.json)

# Unit tests
add_unit_tests(Weblib)
target_link_options(${PROJECT_NAME}WeblibUnitTests PRIVATE
    -sPROXY_TO_PTHREAD # See ./tests/PromiseHelpers.cpp
)
if(NOT EMSCRIPTEN_BROWSER)
  set_target_properties(${PROJECT_NAME}WeblibUnitTests PROPERTIES
      SUFFIX .cjs # Override type:module in package.json. Ideally tests would be in a separate directory.
  )
endif()
