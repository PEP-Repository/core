add_executable(${PROJECT_NAME}Weblib
    EmscriptenJsonConversion.cpp EmscriptenJsonConversion.hpp
    EmscriptenMapBinding.hpp
    EmscriptenVectorBinding.hpp
    Weblib.cpp
    WeblibStructs.cpp WeblibStructs.hpp
    EmscriptenBuffer.cpp EmscriptenBuffer.hpp
    ObservableStream.cpp ObservableStream.hpp
    WeblibApiPromise.cpp WeblibApiPromise.hpp
)
find_package(nlohmann_json REQUIRED)
target_link_libraries(${PROJECT_NAME}Weblib
    ${PROJECT_NAME}Applicationlib
    ${PROJECT_NAME}Clientlib
    ${PROJECT_NAME}OAuthClientlib
    nlohmann_json::nlohmann_json
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
#TODO limit -sENVIRONMENT?
target_link_options(${PROJECT_NAME}Weblib PRIVATE
    -sSTRICT -sALLOW_UNIMPLEMENTED_SYSCALLS # Strict, but still allow stub syscalls
    -Wl,--no-demangle  # https://github.com/llvm/llvm-project/issues/126919
    -gsource-map
    -sNO_PROXY_TO_PTHREAD: # Default: We handle spawning a new thread ourselves, to make sure initialization has happened
    -sNO_EXIT_RUNTIME # Default: Do not destruct globals after main, because we want to keep calling methods
    -sNO_PTHREAD_POOL_SIZE_STRICT # We don't block the main thread # TODO preallocate some threads in the pool?
    -sINITIAL_MEMORY=209715200 # 200 MiB
    -sNO_ABORTING_MALLOC
    -sFETCH
    -lembind
    -lidbfs.js
    -sNO_INVOKE_RUN # We call callMain ourselves
    # noExitRuntime required for -sNO_EXIT_RUNTIME
    -sINCOMING_MODULE_JS_API=thisProgram,print,printErr,noExitRuntime
    -sEXPORTED_RUNTIME_METHODS=FS,FS_createPreloadedFile,IDBFS,callMain
    "--emit-tsd=$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.d.mts"
    # -gseparate-dwarf: Separate debug info into .wasm.debug.wasm file
    # -sRUNTIME_DEBUG: Debug logging for Emscripten runtime
    "$<$<CONFIG:Debug>:-gseparate-dwarf;-sRUNTIME_DEBUG>"

    "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/prejs.js"
)

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

file(GENERATE OUTPUT "$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/package.json" INPUT package.json)

verify_headers_standalone(${PROJECT_NAME}Weblib)
