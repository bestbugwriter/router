# Log Pipeline 测试 Makefile
# 提供便捷的测试运行命令

.PHONY: help test test-quick test-unit test-integration test-performance test-backpressure clean

# 默认目标
help:
	@echo "Log Pipeline 测试框架"
	@echo ""
	@echo "可用命令:"
	@echo "  help              显示此帮助信息"
	@echo "  test              运行完整测试套件"
	@echo "  test-quick        运行快速测试（单元测试 + 集成测试）"
	@echo "  test-unit         仅运行单元测试"
	@echo "  test-integration  仅运行集成测试"
	@echo "  test-performance  仅运行性能测试"
	@echo "  test-backpressure 仅运行反压恢复测试"
	@echo "  clean             清理测试结果"
	@echo ""
	@echo "示例:"
	@echo "  make test-quick"
	@echo "  make test-performance"
	@echo ""

# 运行完整测试套件
test:
	@echo "运行完整测试套件..."
	@bash run_comprehensive_tests.sh

# 运行快速测试
test-quick:
	@echo "运行快速测试..."
	@bash run_comprehensive_tests.sh --quick

# 仅运行单元测试
test-unit:
	@echo "运行单元测试..."
	@bash run_comprehensive_tests.sh --unit-only

# 仅运行集成测试
test-integration:
	@echo "运行集成测试..."
	@bash run_comprehensive_tests.sh --integration-only

# 仅运行性能测试
test-performance:
	@echo "运行性能测试..."
	@bash run_comprehensive_tests.sh --performance-only

# 仅运行反压恢复测试
test-backpressure:
	@echo "运行反压恢复测试..."
	@bash run_comprehensive_tests.sh --backpressure-only

# 清理测试结果
clean:
	@echo "清理测试结果..."
	@rm -rf /tmp/comprehensive-test-results
	@rm -rf /tmp/integration-test-results
	@rm -rf /tmp/performance-test-results
	@rm -rf /tmp/backpressure-test-results
	@rm -rf /tmp/*test-logs*
	@echo "清理完成"

# 检查测试环境
check-env:
	@echo "检查测试环境..."
	@command -v docker >/dev/null 2>&1 || { echo "❌ Docker 未安装"; exit 1; }
	@command -v docker-compose >/dev/null 2>&1 || { echo "❌ Docker Compose 未安装"; exit 1; }
	@command -v python3 >/dev/null 2>&1 || { echo "❌ Python3 未安装"; exit 1; }
	@python3 -c "import kafka" 2>/dev/null || { echo "❌ kafka-python 库未安装"; exit 1; }
	@echo "✅ 测试环境检查通过"

# 显示测试结果目录
show-results:
	@echo "最近的测试结果:"
	@ls -la /tmp/comprehensive-test-results/ 2>/dev/null || echo "没有找到测试结果"
	@echo ""
	@echo "查看最新结果:"
	@ls -t /tmp/comprehensive-test-results/ | head -1 | xargs -I {} echo "  /tmp/comprehensive-test-results/{}"
