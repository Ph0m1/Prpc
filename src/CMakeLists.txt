
find_package(Protobuf REQUIRED)
set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/header.proto)
set(PROTO_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(PROTO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(PROTO_GENERATED_SRCS "")
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_BASENAME ${PROTO_FILE} NAME_WE)

    set(PROTO_SRC_OUTPUT ${PROTO_SRC_DIR}/${PROTO_BASENAME}.pb.cc)
    set(PROTO_HDR_OUTPUT ${PROTO_INCLUDE_DIR}/${PROTO_BASENAME}.pb.h)

    list(APPEND PROTO_GENERATED_SRCS ${PROTO_SRC_OUTPUT})

    add_custom_command(
        OUTPUT ${PROTO_SRC_OUTPUT} ${PROTO_HDR_OUTPUT}
        # 当 .proto 文件本身发生变化时，此命令会重新运行
        COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
                --cpp_out=${CMAKE_CURRENT_SOURCE_DIR}
                --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
                ${PROTO_FILE}

        COMMAND ${CMAKE_COMMAND} -E rename
                ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_BASENAME}.pb.h
                ${PROTO_HDR_OUTPUT}
        DEPENDS ${PROTO_FILE}
        COMMENT "Running protoc on ${PROTO_FILE}"
    )
endforeach()

# 4. 创建一个自定义目标
# 这个目标依赖于所有生成的 protobuf 文件，我们可以让主目标依赖它
add_custom_target(proto_gen ALL DEPENDS ${PROTO_GENERATED_SRCS})


aux_source_directory(. SRC_LIST)

add_library(prpc_provider STATIC
    ${SRC_LIST}
    ${PROTO_GENERATED_SRCS}
)

add_dependencies(prpc_provider proto_gen)

target_link_libraries(prpc_provider
    ${PRPC_LIBS}
)
