#!/usr/bin/env python3
"""
测试报告生成器
生成详细的测试报告，包括覆盖率、性能指标等
"""

import os
import sys
import json
import subprocess
import xml.etree.ElementTree as ET
from datetime import datetime
import argparse

class TestReportGenerator:
    def __init__(self, build_dir="build"):
        self.build_dir = build_dir
        self.report_data = {
            "timestamp": datetime.now().isoformat(),
            "test_results": {},
            "coverage": {},
            "performance": {},
            "static_analysis": {}
        }
    
    def run_tests(self):
        """运行所有测试并收集结果"""
        print("🧪 运行测试套件...")
        
        test_commands = [
            ("config", "./tests/test_config"),
            ("logger", "./tests/test_logger"),
            ("threadpool", "./tests/test_threadpool"),
            ("network_utils", "./tests/test_network_utils"),
            ("error_handling", "./tests/test_error_handling"),
            ("application", "./tests/test_application"),
            ("integration", "./tests/test_integration"),
        ]
        
        os.chdir(self.build_dir)
        
        for test_name, command in test_commands:
            try:
                result = subprocess.run(
                    command.split(),
                    capture_output=True,
                    text=True,
                    timeout=60
                )
                
                self.report_data["test_results"][test_name] = {
                    "status": "PASSED" if result.returncode == 0 else "FAILED",
                    "return_code": result.returncode,
                    "stdout": result.stdout,
                    "stderr": result.stderr,
                    "duration": "N/A"  # 可以通过解析输出获取
                }
                
                print(f"  ✅ {test_name}: {'PASSED' if result.returncode == 0 else 'FAILED'}")
                
            except subprocess.TimeoutExpired:
                self.report_data["test_results"][test_name] = {
                    "status": "TIMEOUT",
                    "return_code": -1,
                    "stdout": "",
                    "stderr": "Test timed out after 60 seconds",
                    "duration": "60+"
                }
                print(f"  ⏰ {test_name}: TIMEOUT")
            except Exception as e:
                self.report_data["test_results"][test_name] = {
                    "status": "ERROR",
                    "return_code": -1,
                    "stdout": "",
                    "stderr": str(e),
                    "duration": "N/A"
                }
                print(f"  ❌ {test_name}: ERROR - {e}")
    
    def collect_coverage(self):
        """收集代码覆盖率信息"""
        print("📊 收集覆盖率信息...")
        
        try:
            # 检查是否有覆盖率文件
            coverage_file = "../build_coverage/coverage_filtered.info"
            if os.path.exists(coverage_file):
                # 解析lcov输出
                result = subprocess.run(
                    ["lcov", "--summary", coverage_file],
                    capture_output=True,
                    text=True
                )
                
                if result.returncode == 0:
                    # 简单解析覆盖率信息
                    lines = result.stdout.split('\n')
                    for line in lines:
                        if "lines" in line and "%" in line:
                            # 提取行覆盖率
                            parts = line.split()
                            for i, part in enumerate(parts):
                                if part.endswith('%'):
                                    self.report_data["coverage"]["line_coverage"] = part
                                    break
                        elif "functions" in line and "%" in line:
                            # 提取函数覆盖率
                            parts = line.split()
                            for i, part in enumerate(parts):
                                if part.endswith('%'):
                                    self.report_data["coverage"]["function_coverage"] = part
                                    break
                
                print(f"  📈 行覆盖率: {self.report_data['coverage'].get('line_coverage', 'N/A')}")
                print(f"  📈 函数覆盖率: {self.report_data['coverage'].get('function_coverage', 'N/A')}")
            else:
                print("  ⚠️  未找到覆盖率文件")
                self.report_data["coverage"]["status"] = "No coverage data available"
                
        except Exception as e:
            print(f"  ❌ 覆盖率收集失败: {e}")
            self.report_data["coverage"]["error"] = str(e)
    
    def run_performance_tests(self):
        """运行性能测试"""
        print("🚀 运行性能测试...")
        
        try:
            if os.path.exists("./tests/performance_test"):
                result = subprocess.run(
                    ["./tests/performance_test"],
                    capture_output=True,
                    text=True,
                    timeout=300  # 5分钟超时
                )
                
                self.report_data["performance"] = {
                    "status": "COMPLETED" if result.returncode == 0 else "FAILED",
                    "output": result.stdout,
                    "stderr": result.stderr
                }
                
                # 解析性能数据（如果有的话）
                if "ops/sec" in result.stdout:
                    lines = result.stdout.split('\n')
                    for line in lines:
                        if "ops/sec" in line:
                            self.report_data["performance"]["throughput"] = line.strip()
                            break
                
                print(f"  🎯 性能测试: {'完成' if result.returncode == 0 else '失败'}")
            else:
                print("  ⚠️  未找到性能测试可执行文件")
                
        except Exception as e:
            print(f"  ❌ 性能测试失败: {e}")
            self.report_data["performance"]["error"] = str(e)
    
    def generate_html_report(self, output_file="test_report.html"):
        """生成HTML格式的测试报告"""
        print(f"📝 生成HTML报告: {output_file}")
        
        html_content = f"""
<!DOCTYPE html>
<html>
<head>
    <title>PRPC 测试报告</title>
    <meta charset="UTF-8">
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        .header {{ background-color: #f0f0f0; padding: 20px; border-radius: 5px; }}
        .section {{ margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }}
        .passed {{ color: green; }}
        .failed {{ color: red; }}
        .timeout {{ color: orange; }}
        table {{ border-collapse: collapse; width: 100%; }}
        th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
        th {{ background-color: #f2f2f2; }}
        .coverage-bar {{ background-color: #f0f0f0; height: 20px; border-radius: 10px; }}
        .coverage-fill {{ background-color: #4CAF50; height: 100%; border-radius: 10px; }}
    </style>
</head>
<body>
    <div class="header">
        <h1>🧪 PRPC 项目测试报告</h1>
        <p>生成时间: {self.report_data['timestamp']}</p>
    </div>
    
    <div class="section">
        <h2>📊 测试结果概览</h2>
        <table>
            <tr><th>测试模块</th><th>状态</th><th>返回码</th></tr>
"""
        
        for test_name, result in self.report_data["test_results"].items():
            status_class = result["status"].lower()
            html_content += f"""
            <tr>
                <td>{test_name}</td>
                <td class="{status_class}">{result["status"]}</td>
                <td>{result["return_code"]}</td>
            </tr>
"""
        
        html_content += """
        </table>
    </div>
"""
        
        # 覆盖率部分
        if self.report_data["coverage"]:
            html_content += f"""
    <div class="section">
        <h2>📈 代码覆盖率</h2>
        <p>行覆盖率: {self.report_data["coverage"].get("line_coverage", "N/A")}</p>
        <p>函数覆盖率: {self.report_data["coverage"].get("function_coverage", "N/A")}</p>
    </div>
"""
        
        # 性能测试部分
        if self.report_data["performance"]:
            html_content += f"""
    <div class="section">
        <h2>🚀 性能测试结果</h2>
        <p>状态: {self.report_data["performance"].get("status", "N/A")}</p>
        <p>吞吐量: {self.report_data["performance"].get("throughput", "N/A")}</p>
    </div>
"""
        
        html_content += """
</body>
</html>
"""
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(html_content)
        
        print(f"  ✅ HTML报告已生成: {output_file}")
    
    def generate_json_report(self, output_file="test_report.json"):
        """生成JSON格式的测试报告"""
        print(f"📝 生成JSON报告: {output_file}")
        
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(self.report_data, f, indent=2, ensure_ascii=False)
        
        print(f"  ✅ JSON报告已生成: {output_file}")
    
    def print_summary(self):
        """打印测试摘要"""
        print("\n" + "="*50)
        print("           📋 测试摘要")
        print("="*50)
        
        total_tests = len(self.report_data["test_results"])
        passed_tests = sum(1 for r in self.report_data["test_results"].values() if r["status"] == "PASSED")
        failed_tests = sum(1 for r in self.report_data["test_results"].values() if r["status"] == "FAILED")
        
        print(f"总测试数: {total_tests}")
        print(f"通过: {passed_tests}")
        print(f"失败: {failed_tests}")
        print(f"成功率: {(passed_tests/total_tests*100):.1f}%" if total_tests > 0 else "N/A")
        
        if self.report_data["coverage"]:
            print(f"代码覆盖率: {self.report_data['coverage'].get('line_coverage', 'N/A')}")
        
        print("="*50)

def main():
    parser = argparse.ArgumentParser(description="生成PRPC项目测试报告")
    parser.add_argument("--build-dir", default="build", help="构建目录路径")
    parser.add_argument("--output-html", default="test_report.html", help="HTML报告输出文件")
    parser.add_argument("--output-json", default="test_report.json", help="JSON报告输出文件")
    parser.add_argument("--no-coverage", action="store_true", help="跳过覆盖率收集")
    parser.add_argument("--no-performance", action="store_true", help="跳过性能测试")
    
    args = parser.parse_args()
    
    generator = TestReportGenerator(args.build_dir)
    
    # 运行测试
    generator.run_tests()
    
    # 收集覆盖率（如果需要）
    if not args.no_coverage:
        generator.collect_coverage()
    
    # 运行性能测试（如果需要）
    if not args.no_performance:
        generator.run_performance_tests()
    
    # 生成报告
    generator.generate_html_report(args.output_html)
    generator.generate_json_report(args.output_json)
    
    # 打印摘要
    generator.print_summary()

if __name__ == "__main__":
    main() 