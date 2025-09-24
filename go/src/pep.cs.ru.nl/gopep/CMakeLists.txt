set(OUT_DIR ${PEP_GOPATH}/src/generated.pep.cs.ru.nl/gopep)

include(${PEP_GO_ROOT_PATH}/../messages/Messages.cmake)
make_protobuf_target(${PROJECT_NAME}gopep-messages go ${PEP_GOPATH}/src/generated.pep.cs.ru.nl/gopep pep_proto)
ADD_DEPENDENCIES(${PROJECT_NAME}gopep-messages go-vendor)
