include(Messages.cmake)

add_library(${PROJECT_NAME}Protolib OBJECT "${MESSAGES_PROTO_PATH}")

find_package(Protobuf REQUIRED)
target_link_libraries(${PROJECT_NAME}Protolib PUBLIC protobuf::libprotobuf)

set(PROTO_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(${PROJECT_NAME}Protolib SYSTEM PUBLIC "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>")

protobuf_generate(
  TARGET ${PROJECT_NAME}Protolib
  IMPORT_DIRS "${MESSAGES_PROTO_DIR}"
  PROTOC_OUT_DIR "${PROTO_BINARY_DIR}"
)

make_protobuf_checksum_target(${PROJECT_NAME}Protohash cpp ${PROTO_BINARY_DIR} h)
