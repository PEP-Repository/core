# Keep consistent with conanfile.py build_requirements.
# Use https://apt.kitware.com/ to get a recent CMake version if needed,
# or use the version from Conan via build/generators/conanbuild.*
cmake_minimum_required(VERSION 3.28...4.2)

# We don't use C++ modules yet and don't want to require clang-scan-deps,
# see https://discourse.cmake.org/t/cmake-3-28-cmake-cxx-compiler-clang-scan-deps-notfound-not-found/9244
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Make sure we do not get warnings about CMAKE_TOOLCHAIN_FILE being unused,
# but do error if it would be ignored (after first configure).
# See also https://gitlab.kitware.com/cmake/cmake/-/issues/17261#note_854453
if(CMAKE_TOOLCHAIN_FILE)
  if(NOT MY_CMAKE_TOOLCHAIN_FILE_IN_USE)
    set(MY_CMAKE_TOOLCHAIN_FILE_IN_USE ${CMAKE_TOOLCHAIN_FILE} CACHE INTERNAL "The CMake toolchain file in use")
  else()
    file(REAL_PATH ${CMAKE_TOOLCHAIN_FILE} toolchain_file_abs BASE_DIRECTORY ${CMAKE_BINARY_DIR})
    file(REAL_PATH ${MY_CMAKE_TOOLCHAIN_FILE_IN_USE} toolchain_file_in_use_abs BASE_DIRECTORY ${CMAKE_BINARY_DIR})
    if(NOT ${toolchain_file_abs} STREQUAL ${toolchain_file_in_use_abs})
      # Make sure the cache is not overwritten
      set(CMAKE_TOOLCHAIN_FILE ${MY_CMAKE_TOOLCHAIN_FILE_IN_USE} CACHE FILEPATH "The CMake toolchain file" FORCE)
      message(FATAL_ERROR "CMAKE_TOOLCHAIN_FILE cannot be changed after configure (${toolchain_file_in_use_abs} -> ${toolchain_file_abs}), delete CMakeCache.txt")
    endif()
  endif()
endif()


project(pep)

if(DEFINED EMSCRIPTEN)
  message(STATUS "Emscripten version: ${EMSCRIPTEN_VERSION}")
  # Use empty suffix instead of .js for -sEXECUTABLE behavior
  set(CMAKE_EXECUTABLE_SUFFIX "")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules/")

# Prevent build time switching between configurations (in Visual Studio), since
# builds that don't correspond with the CMAKE_BUILD_TYPE will fail or be inconsistent.
# See issue 499 (https://gitlab.pep.cs.ru.nl/pep/core/issues/499) for details.
# Moreover, we should make sure that CMAKE_BUILD_TYPE corresponds with the build_type passed to Conan,
# see https://gitlab.pep.cs.ru.nl/pep/core/-/merge_requests/1748#note_35123.
if(NOT CMAKE_BUILD_TYPE)
  message(FATAL_ERROR "We do not support multiconfig builds yet (see pep/core#499), please explicitly specify -DCMAKE_BUILD_TYPE=<...> to force consistent builds.")
endif()
SET(CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE} CACHE STRING "" FORCE)

if(DEFINED ENV{CI_COMMIT_REF_NAME})
    message("CI build for branch: $ENV{CI_COMMIT_REF_NAME}")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(INVOKE_SH powershell -ExecutionPolicy Bypass -File ${PROJECT_SOURCE_DIR}/scripts/invoke-sh.ps1)
else()
    set(INVOKE_SH "")
endif()

##

function(get_build_output_directories variable subdir)
    if("${CMAKE_GENERATOR}" MATCHES "Visual Studio.*")
        # The Visual Studio (VS) build system allows the configuration to be switched at build time.
        # Each supported build configuration uses a separate output directory (where the .exe will be put).
        # Make associated (companion) files available in each of these output directories to allow the .exe
        # to find those files.
        FOREACH(config ${CMAKE_CONFIGURATION_TYPES})
            list(APPEND result "${CMAKE_CURRENT_BINARY_DIR}/${config}/${subdir}")
        ENDFOREACH()
    elseif(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        list(APPEND result "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif()

    # For non-VS build systems, the configuration-to-build is determined at CMake time.
    # The generated build system then has a single output directory where the executable will be placed,
    # and hence where associated (companion) files must be available.
    # For VS build systems, we'll also place companion files there to support e.g. custom Go
    # targets that produce their binaries directly into the binary dir (i.e. without using
    # subdirectories named after configurations).
    list(APPEND result "${CMAKE_CURRENT_BINARY_DIR}/${subdir}")

    set(${variable} ${result} PARENT_SCOPE)
endfunction()

# Given a file or directory that must be placed next to the (project output) executable, produces 1..n paths where the entry should be copied.
function(get_executable_companion_destinations variable subdir inputfile)
    get_filename_component(filename ${inputfile} NAME)
    get_build_output_directories(directories ${subdir})
    FOREACH(dir ${directories})
        list(APPEND result "${dir}/${filename}")
    ENDFOREACH()
    set(${variable} ${result} PARENT_SCOPE)
endfunction()

# configure_file replacement that also supports directories
function(configure_file_or_directory input output)
    if(IS_DIRECTORY "${input}")
        get_filename_component(dirname "${input}" NAME)
        set(destination "${output}/${dirname}")
        file(MAKE_DIRECTORY ${destination})
        file(GLOB entries RELATIVE "${input}" "${input}/*")
        foreach(entry ${entries})
            configure_file_or_directory("${input}/${entry}" "${destination}")
        endforeach()
    else()
        configure_file(${input} ${output} ${ARGN})
    endif()
endfunction()

# Given a file or directory that must be placed next to the (project output) executable, copies that entry to all applicable locations.
function(configure_executable_companion_to_subdir subdir sourcepath)
    get_executable_companion_destinations(destination_paths ${subdir} ${sourcepath})
    if(EXISTS "${sourcepath}")
        if(NOT ARGN MATCHES "REPLACE_VARIABLES")
            set(copy_option COPYONLY)
        endif()
        foreach(destination ${destination_paths})
            configure_file_or_directory(${sourcepath} ${destination} ${copy_option})
        endforeach()
    elseif(NOT ARGN MATCHES "OPTIONAL")
        message(FATAL_ERROR "Cannot find executable companion file at ${sourcepath}")
    else() # Source file is (optional and) absent: remove any destination files left behind from a previous invocation with possibly different settings
        foreach(destination ${destination_paths})
            file(REMOVE "${destination}")
        endforeach()
    endif()
endfunction()

# Given a file that must be placed next to the (project output) executable, copies that file to all applicable locations.
function(configure_executable_companion path)
    configure_executable_companion_to_subdir(. ${path} ${ARGN})
endfunction()

# Given a directory with files that must be placed next to the (project output) executable, copies those files to all applicable locations.
function(configure_executable_companions_to_subdir_from subdir abspath)
    file(GLOB files "${abspath}/*")
    foreach(file ${files})
        configure_executable_companion_to_subdir(${subdir} ${file})
    endforeach()
endfunction()

# When using Visual Studio, places a .user file that makes the debugger pass the specified command line parameter(s) to the process
function(configure_debug_command_line_parameter projectName)
    if("${CMAKE_GENERATOR}" MATCHES "Visual Studio.*")
        string(REPLACE ";" " " LOCAL_DEBUGGER_COMMAND_ARGUMENTS "${ARGN}")
        configure_file(${PROJECT_SOURCE_DIR}/visual_studio_project_user_settings_template ${CMAKE_CURRENT_BINARY_DIR}/${projectName}.vcxproj.user @ONLY)
    endif()
endfunction()

function(escape_cache_var_name var_containing_name)
  string(REGEX REPLACE [[([^A-Za-z0-9/_\.+-])]] _ out "${${var_containing_name}}")
  set(${var_containing_name} "${out}" PARENT_SCOPE)
endfunction()

# Adds provided flags to non-link compile options for language if supported.
# Sets boolean cache variable FLAG_SUPPORTED_<language>_<escape_cache_var_name(flag)>.
function(add_supported_compile_options language)
    include(CheckCompilerFlag)
    # Speedup: skip linking
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY) # Set locally

    foreach(flag IN LISTS ARGN)
        set(supported_var "FLAG_SUPPORTED_${language}_${flag}")
        # Escape '=' etc.
        escape_cache_var_name(supported_var)

        check_compiler_flag("${language}" "${flag}" ${supported_var})

        if(${supported_var})
            add_compile_options("$<$<COMPILE_LANGUAGE:${language}>:${flag}>")
        endif()
    endforeach()
endfunction()

# Add a dependency on the version.json file so when the file changes, cmake will reconfigure.
# see: https://www.reddit.com/r/cmake/comments/iokem9/comment/g4gewpo/
set_property(
  DIRECTORY
  APPEND
  PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/version.json
)

# Read semver parts
file(READ ${CMAKE_CURRENT_LIST_DIR}/version.json VERSION_JSON)

string(JSON MAJOR_VERSION GET ${VERSION_JSON} major)
string(JSON MINOR_VERSION GET ${VERSION_JSON} minor)

# For local builds, we don't care what the pipeline and job id are, but they must be set to some numeric value
if("$ENV{CI_PIPELINE_ID}" STREQUAL "")
    set(CI_PIPELINE_ID 0)
else()
    set(CI_PIPELINE_ID "$ENV{CI_PIPELINE_ID}")
endif()

if("$ENV{CI_JOB_ID}" STREQUAL "")
    set(CI_JOB_ID 0)
else()
    set(CI_JOB_ID "$ENV{CI_JOB_ID}")
endif()

##

if(DEFINED EMSCRIPTEN)
  if(NOT DEFINED CMAKE_CROSSCOMPILING_EMULATOR)
    find_program(NODE_PROGRAM node)
    set(CMAKE_CROSSCOMPILING_EMULATOR ${NODE_PROGRAM})
  endif()

  # Excludes weblib
  option(PEP_EMSCRIPTEN_BROWSER "Build Emscripten executables for browser instead of Node.js")

  define_property(TARGET PROPERTY PEP_SKIP_DEFAULT_EMSCRIPTEN_EXE_CONFIG
      BRIEF_DOCS "Do not add default config for Emscripten executables"
  )

  # Default Emscripten config for CLI executables
  # See https://emscripten.org/docs/tools_reference/settings_reference.html
  set(default_emscripten_exe_config
      -sSTRICT -sALLOW_UNIMPLEMENTED_SYSCALLS # Strict, but still allow stub syscalls
      LINKER:--no-demangle  # https://github.com/llvm/llvm-project/issues/126919
      -gsource-map
      -sEXIT_RUNTIME  # Run global destructors after main()
      "$<$<CONFIG:Debug,RelWithDebInfo>:-Wno-error=limited-postlink-optimizations>"
      # -sRUNTIME_DEBUG: Debug logging for Emscripten runtime
      "$<$<CONFIG:Debug>:-sRUNTIME_DEBUG>"
  )

  if(PEP_EMSCRIPTEN_BROWSER)
    set(CMAKE_EXECUTABLE_SUFFIX .html)  # Create HTML shell
    list(APPEND default_emscripten_exe_config
        -sENVIRONMENT=web
        -sPROXY_TO_PTHREAD  # Put main thread in separate worker to prevent blocking UI thread
    )

    option(PEP_EMSCRIPTEN_EMRUN "Enable emrun support" OFF)
    if(PEP_EMSCRIPTEN_EMRUN)
      list(APPEND default_emscripten_exe_config
          --emrun
          -sINCOMING_MODULE_JS_API=print,canvas,setStatus,monitorRunDependencies,arguments
      )
    endif()
  else()
    list(APPEND default_emscripten_exe_config
        -sENVIRONMENT=node
        -sNODERAWFS # Use host file system
    )
  endif()

  add_link_options("$<$<NOT:$<BOOL:$<TARGET_PROPERTY:PEP_SKIP_DEFAULT_EMSCRIPTEN_EXE_CONFIG>>>:${default_emscripten_exe_config}>")
endif()

# Add shebang to JS file, if target is such a file
#XXX Remove for version 5.0.0+ (which has -sEXECUTABLE; implicit for targets without extension)
function(make_js_file_executable target)
  if (DEFINED EMSCRIPTEN AND NOT PEP_EMSCRIPTEN_BROWSER AND EMSCRIPTEN_VERSION VERSION_LESS "5.0.0")
    get_target_property(skip ${target} PEP_SKIP_DEFAULT_EMSCRIPTEN_EXE_CONFIG)
    get_target_property(suffix ${target} SUFFIX)
    if(NOT skip AND (NOT suffix OR suffix MATCHES "^\.?js$"))
      add_custom_command(
          TARGET ${target} POST_BUILD
          # Add node shebang
          COMMAND sed --in-place "1 i #!/usr/bin/env node" "$<TARGET_FILE:${target}>"
          # Make file executable
          COMMAND chmod +x "$<TARGET_FILE:${target}>"
          VERBATIM
          COMMENT "Make JS file executable"
      )
    endif()
  endif()
endfunction()

option(ENABLE_TEST_DISCOVERY "Make tests discoverable, to be able to run them from e.g. Visual Studio Test Explorer" OFF)
if(ENABLE_TEST_DISCOVERY)
  include(CTest) #This allows discovery of our tests with e.g. Visual Studio

  set(integration_test_flags
      --local
      --no-docker
      --build-dir ${CMAKE_BINARY_DIR}
  )
  if("${CMAKE_GENERATOR}" MATCHES "Visual Studio.*")
    list(APPEND integration_test_flags --build-mode ${CMAKE_BUILD_TYPE})
  endif()
  add_test(
      NAME integration
      COMMAND ${INVOKE_SH} ${CMAKE_SOURCE_DIR}/tests/integration.sh ${integration_test_flags}
  )
endif()

# Provide project wide targets
include(config/ConfigFiles.cmake)
add_subdirectory(s3proxy)
option(GENERATE_PKI "Generate a PKI for running PEP locally" ON)
if(GENERATE_PKI)
    add_subdirectory(pki)
endif(GENERATE_PKI)

# Include C++ targets in our build
add_subdirectory(cpp)

if(DEFINED EMSCRIPTEN)
  add_subdirectory(weblib)

else()
  # Include Go targets in our build (if applicable)
  option(BUILD_GO_SERVERS "Build the auxiliary servers written in Go" OFF)
  if(BUILD_GO_SERVERS)
    add_subdirectory(go)
  endif(BUILD_GO_SERVERS)

  # Replace binary name placeholders in autocompletion scripts
  # cli_executables_list is populated by add_cli_executable

  get_property(cli_executables_list GLOBAL PROPERTY cli_executables)
  list(JOIN cli_executables_list " " cli_executables)

  configure_file(./autocomplete/autocomplete_pep.bash ./autocomplete/autocomplete_pep.bash @ONLY)
  configure_file(./autocomplete/autocomplete_pep.zsh  ./autocomplete/autocomplete_pep.zsh @ONLY)
  configure_file(./autocomplete/autocomplete_pep.fish ./autocomplete/autocomplete_pep.fish @ONLY)
  configure_file(./autocomplete/autocomplete_pep.ps1  ./autocomplete/autocomplete_pep.ps1 @ONLY)
  configure_file(./autocomplete/install_autocomplete_pep.sh  ./autocomplete/install_autocomplete_pep.sh @ONLY)
  configure_file(./autocomplete/install_autocomplete_pep_windows.ps1  ./autocomplete/install_autocomplete_pep_windows.ps1 @ONLY)

  if(UNIX)
    install(CODE "execute_process(COMMAND \"${CMAKE_BINARY_DIR}/autocomplete/install_autocomplete_pep.sh\")")
  endif()
endif()
