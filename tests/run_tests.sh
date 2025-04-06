#!/bin/bash

# PRPC 测试运行脚本

echo "========================================="
echo "           PRPC Test Suite"
echo "========================================="

# 检查是否在正确的目录
if [ ! -f "../CMakeLists.txt" ]; then
    echo "Error: Please run this script from the tests directory"
    exit 1
fi

# 创建构建目录
BUILD_DIR="../build"
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

# 进入构建目录
cd "$BUILD_DIR"

# 构建项目
echo "Building project..."
cmake .. && make

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Build successful!"
echo ""

# 运行测试
echo "========================================="
echo "           Running Tests"
echo "========================================="

# 测试列表
TESTS=(
    "test_config"
    "test_logger" 
    "test_threadpool"
    "test_network_utils"
    "test_error_handling"
    "test_application"
    "test_integration"
)

PASSED=0
FAILED=0
FAILED_TESTS=()

# 运行每个测试
for test in "${TESTS[@]}"; do
    echo ""
    echo "-----------------------------------------"
    echo "Running $test..."
    echo "-----------------------------------------"
    
    if [ -f "tests/$test" ]; then
        ./tests/$test
        if [ $? -eq 0 ]; then
            echo "✅ $test PASSED"
            ((PASSED++))
        else
            echo "❌ $test FAILED"
            ((FAILED++))
            FAILED_TESTS+=("$test")
        fi
    else
        echo "⚠️  $test executable not found"
        ((FAILED++))
        FAILED_TESTS+=("$test (not found)")
    fi
done

# 运行性能测试（可选）
echo ""
echo "-----------------------------------------"
echo "Running performance test..."
echo "-----------------------------------------"
if [ -f "tests/performance_test" ]; then
    ./tests/performance_test
    if [ $? -eq 0 ]; then
        echo "✅ performance_test COMPLETED"
    else
        echo "⚠️  performance_test had issues (non-critical)"
    fi
else
    echo "⚠️  performance_test executable not found"
fi

# 显示测试结果摘要
echo ""
echo "========================================="
echo "           Test Results Summary"
echo "========================================="
echo "Total tests: $((PASSED + FAILED))"
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ $FAILED -gt 0 ]; then
    echo ""
    echo "Failed tests:"
    for failed_test in "${FAILED_TESTS[@]}"; do
        echo "  - $failed_test"
    done
    echo ""
    echo "❌ Some tests failed!"
    exit 1
else
    echo ""
    echo "🎉 All tests passed!"
    exit 0
fi 