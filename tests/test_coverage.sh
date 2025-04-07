#!/bin/bash

# 测试覆盖率分析脚本
echo "========================================="
echo "           测试覆盖率分析"
echo "========================================="

# 检查是否安装了gcov和lcov
if ! command -v gcov &> /dev/null; then
    echo "⚠️  gcov 未安装，无法生成覆盖率报告"
    echo "   安装命令: sudo apt-get install gcc"
fi

if ! command -v lcov &> /dev/null; then
    echo "⚠️  lcov 未安装，无法生成HTML报告"
    echo "   安装命令: sudo apt-get install lcov"
fi

# 创建覆盖率构建目录
mkdir -p ../build_coverage
cd ../build_coverage

echo "🔧 配置覆盖率构建..."
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="--coverage -g -O0" \
      -DCMAKE_C_FLAGS="--coverage -g -O0" \
      -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
      -DCMAKE_SHARED_LINKER_FLAGS="--coverage" \
      ..

echo "🔨 构建项目..."
make -j$(nproc)

echo "🧪 运行测试..."
# 运行所有测试
./tests/test_config
./tests/test_logger  
./tests/test_threadpool
./tests/test_network_utils
./tests/test_error_handling
./tests/test_application
./tests/test_integration

if command -v lcov &> /dev/null; then
    echo "📊 生成覆盖率报告..."
    
    # 收集覆盖率数据
    lcov --capture --directory . --output-file coverage.info
    
    # 过滤系统文件和测试文件
    lcov --remove coverage.info '/usr/*' '*/tests/*' '*/build/*' --output-file coverage_filtered.info
    
    # 生成HTML报告
    genhtml coverage_filtered.info --output-directory coverage_html
    
    echo "✅ 覆盖率报告已生成: build_coverage/coverage_html/index.html"
    echo "📈 覆盖率统计:"
    lcov --summary coverage_filtered.info
else
    echo "⚠️  无法生成HTML报告，请安装lcov"
fi

echo "🎯 覆盖率分析完成！" 