function(stage_s3proxy target_name)
  get_build_output_directories(destination_dirs .)
  FOREACH(destination ${destination_dirs})
    # DEPENDS clauses below are adapted/copied from the "copy_pki_files" function that's defined in pki.cmake.
    # We don't want to use the "copy_pki_files" function here because the "s3proxy.sh stage" command is (and
    # must be) standalone: we want to (be able to) stage the script and all its dependencies without requiring
    # a (CMake-bound) function call beforehand, e.g. as part of the "integration.sh" script.
    set(pki_dir ${PROJECT_BINARY_DIR}/pki)
    add_custom_command(
      # TODO: also OUTPUT other stuff that "s3proxy.sh" stages
      OUTPUT ${destination}/s3proxy.sh
      COMMAND ${INVOKE_SH} ${PROJECT_SOURCE_DIR}/s3proxy/s3proxy.sh stage ${destination} ${pki_dir}/s3certs
      DEPENDS pki ${pki_dir}/s3certs/private.key ${pki_dir}/s3certs/public.crt
    )
    list(APPEND destination_files "${destination}/s3proxy.sh")
  ENDFOREACH()
  
  add_custom_target (${target_name} DEPENDS ${destination_files})
endfunction()