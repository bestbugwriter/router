.PHONY: help build build-static test test-integration clean docker-build docker-test

# 颜色定义
BLUE := \033[0;34m
GREEN := \033[0;32m
YELLOW := \033[1;33m
RED := \033[0;31m
NC := \033[0m

help:
	@echo "$(BLUE)Log Pipeline - 快速命令参考$(NC)"
	@echo ""
	@echo "构建命令:"
	@echo "  $(GREEN)make build$(NC)               - 编译项目（动态链接）"
	@echo "  $(GREEN)make build-static$(NC)        - 编译项目（静态链接）"
	@echo "  $(GREEN)make docker-build$(NC)        - 构建 Docker 镜像"
	@echo ""
	@echo "测试命令:"
	@echo "  $(GREEN)make test$(NC)                - 运行完整测试"
	@echo "  $(GREEN)make test-integration$(NC)    - 运行集成测试（推荐）"
	@echo "  $(GREEN)make test-quick$(NC)          - 运行快速测试"
	@echo "  $(GREEN)make docker-test$(NC)         - 使用 Docker 运行集成测试（推荐，无需本地 Python 环境）"
	@echo ""
	@echo "清理命令:"
	@echo "  $(GREEN)make clean$(NC)               - 清理构建文件"
	@echo "  $(GREEN)make clean-docker$(NC)        - 清理 Docker 容器和结果"
	@echo ""
	@echo "示例:"
	@echo "  $(YELLOW)make docker-test$(NC)          # 最简单的方法，一键运行测试"
	@echo "  $(YELLOW)make build && make test$(NC)    # 构建后运行所有测试"
	@echo ""

build:
	@echo "$(BLUE)===> 编译项目$(NC)"
	@./build.sh

build-static:
	@echo "$(BLUE)===> 编译项目（静态链接）$(NC)"
	@./build.sh --static

docker-build:
	@echo "$(BLUE)===> 构建 Docker 镜像$(NC)"
	@if command -v docker-compose >/dev/null 2>&1; then \
		docker-compose -f docker-compose.test.yml build; \
	else \
		docker compose -f docker-compose.test.yml build; \
	fi

docker-test:
	@echo "$(BLUE)===> 运行集成测试（容器化，无需本地 Python 环境）$(NC)"
	@echo ""
	@./run_integration_test.sh
	@echo ""

test:
	@echo "$(BLUE)===> 运行完整测试$(NC)"
	@cd tests && make test

test-quick:
	@echo "$(BLUE)===> 运行快速测试$(NC)"
	@cd tests && make test-quick

test-integration:
	@echo "$(BLUE)===> 运行集成测试$(NC)"
	@cd tests && make test-integration

clean:
	@echo "$(BLUE)===> 清理构建文件$(NC)"
	@rm -rf build/
	@rm -f CMakeCache.txt
	@echo "$(GREEN)✓ 清理完成$(NC)"

clean-docker:
	@echo "$(BLUE)===> 清理 Docker 容器和结果$(NC)"
	@if command -v docker-compose >/dev/null 2>&1; then \
		docker-compose -f docker-compose.yml down --volumes 2>/dev/null || true; \
		docker-compose -f docker-compose.test.yml down --volumes 2>/dev/null || true; \
	else \
		docker compose -f docker-compose.yml down --volumes 2>/dev/null || true; \
		docker compose -f docker-compose.test.yml down --volumes 2>/dev/null || true; \
	fi
	@rm -rf /tmp/integration-test-results-* 2>/dev/null || true
	@echo "$(GREEN)✓ Docker 清理完成$(NC)"

.DEFAULT_GOAL := help
