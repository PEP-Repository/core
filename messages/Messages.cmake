# This is an includable CMake file (as opposed to a standalone CMakeLists.txt)
# so that it doesn't introduce a separate scope. Thus, it can provide a function
# to its includer (as opposed to having it available only in this file).

set(MESSAGES_PROTO_DIR ${CMAKE_CURRENT_LIST_DIR})
set(MESSAGES_PROTO_PATH ${MESSAGES_PROTO_DIR}/Messages.proto)
set(MESSAGES_PROTO_SHA512TOSRC ${MESSAGES_PROTO_DIR}/Messages.proto.sha512tosrc.sh)
set(Messages_DEPENDENCIES ${MESSAGES_PROTO_PATH} ${MESSAGES_PROTO_SHA512TOSRC})

function(make_protobuf_checksum_target name language dest_dir checksum_file_ext)
  set(checksum_file_name Messages.pb.checksum.${checksum_file_ext})
  add_custom_command(
    OUTPUT ${dest_dir}/${checksum_file_name}
    COMMAND ${INVOKE_SH} ${MESSAGES_PROTO_SHA512TOSRC} ${language} ${MESSAGES_PROTO_PATH} ${dest_dir} ${checksum_file_name}
    DEPENDS ${Messages_DEPENDENCIES}
  )
  add_custom_target(${name} DEPENDS ${dest_dir}/${checksum_file_name})
endfunction()

#TODO Remove when Go code is gone
function(make_protobuf_target name language out_dir out_subdir)
  set(pb_file_ext ${language})
  # Default to language, e.g. `.go`
  set(checksum_file_ext ${language})
  if("${language}" STREQUAL "cpp")
    set(pb_file_ext "cc")
    set(checksum_file_ext "h")
  endif()
  set(dest_dir ${out_dir})
  if(NOT "${out_subdir}" STREQUAL "")
    set(dest_dir ${dest_dir}/${out_subdir})
  endif()

  find_package(Protobuf REQUIRED)
  set(generated_pb_file_name Messages.pb.${pb_file_ext})
  add_custom_command(
    OUTPUT ${dest_dir}/${generated_pb_file_name}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE} -I${MESSAGES_PROTO_DIR} --${language}_out=${out_dir} ${MESSAGES_PROTO_PATH}
    DEPENDS ${Messages_DEPENDENCIES}
  )

  make_protobuf_checksum_target(${name}_checksum ${language} ${dest_dir} ${checksum_file_ext})
  add_custom_target(${name} DEPENDS ${dest_dir}/${generated_pb_file_name} ${name}_checksum)
endfunction()
