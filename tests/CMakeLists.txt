# 测试模块的CMakeLists.txt

# 启用测试
enable_testing()

# 查找Google Test框架（可选）
find_package(GTest QUIET)

# 如果没有找到GTest，使用简单的assert测试
if(NOT GTest_FOUND)
    message(STATUS "Google Test not found, using simple assert-based tests")
endif()

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

# 测试可执行文件列表
set(TEST_SOURCES
    test_config.cc
    test_logger.cc
    test_threadpool.cc
    test_network_utils.cc
    test_error_handling.cc
    test_application.cc
    test_integration.cc
)

# 为每个测试文件创建可执行文件
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    
    target_link_libraries(${TEST_NAME}
        prpc_provider
        ${PRPC_LIBS}
    )
    
    # 添加到CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    
    # 设置测试属性
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 30
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

# 创建一个运行所有测试的目标
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${TEST_SOURCES}
    COMMENT "Running all tests"
)

# 性能测试（可选）
add_executable(performance_test performance_test.cc)
target_link_libraries(performance_test
    prpc_provider
    ${PRPC_LIBS}
)

# 集成测试
add_executable(integration_test test_integration.cc)
target_link_libraries(integration_test
    prpc_provider
    ${PRPC_LIBS}
)

# Provider测试
add_executable(test_provider test_provider.cc)
target_link_libraries(test_provider
    prpc_provider
    ${PRPC_LIBS}
)

# Zookeeper测试
add_executable(test_zookeeper test_zookeeper.cc)
target_link_libraries(test_zookeeper
    prpc_provider
    ${PRPC_LIBS}
)

# 基准测试（如果有Google Benchmark）
find_package(benchmark QUIET)
if(benchmark_FOUND)
    add_executable(benchmark_test benchmark_test.cc)
    target_link_libraries(benchmark_test
        prpc_provider
        ${PRPC_LIBS}
        benchmark::benchmark
    )
    message(STATUS "Google Benchmark found, benchmark tests enabled")
else()
    message(STATUS "Google Benchmark not found, benchmark tests disabled")
endif()

# 模糊测试（如果使用Clang）
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(fuzz_test fuzz_test.cc)
    target_compile_options(fuzz_test PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_test PRIVATE -fsanitize=fuzzer,address)
    target_link_libraries(fuzz_test
        prpc_provider
        ${PRPC_LIBS}
    )
    message(STATUS "Clang compiler detected, fuzzing tests enabled")
endif() 