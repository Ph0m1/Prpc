file(GLOB Client_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

file(GLOB PROTO_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../*.pb.cc)

add_executable(client ${Client_SRCS} ${PROTO_SRCS})

target_link_libraries(client prpc_provider ${PRPC_LIBS})

target_compile_options(client PRIVATE -std=c++20 -Wall)

set_target_properties(client PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)