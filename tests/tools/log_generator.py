#!/usr/bin/env python3
"""
日志生成工具
用于生成各种格式的测试日志，支持不同速度和大小
"""

import argparse
import json
import logging
import os
import random
import sys
import time
from datetime import datetime, timedelta
from pathlib import Path

# 设置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class LogGenerator:
    def __init__(self, output_dir: str, log_level: str = "INFO"):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.log_level = log_level
        self.start_time = time.time()
        self.generated_count = 0
        
        # 预定义的日志模板
        self.log_templates = [
            "[{timestamp}] {level} Request processed: method={method}, path={path}, status={status}, latency={latency}ms",
            "[{timestamp}] {level} Database query: query={query}, duration={duration}ms, rows={rows}",
            "[{timestamp}] {level} Cache operation: operation={operation}, key={key}, hit={hit}",
            "[{timestamp}] {level} User action: user_id={user_id}, action={action}, ip={ip}",
            "[{timestamp}] {level} System metric: cpu={cpu}%, memory={memory}%, disk={disk}%",
            "[{timestamp}] {level} Network traffic: src={src}, dst={dst}, bytes={bytes}, protocol={protocol}",
            "[{timestamp}] {level} Error occurred: error_type={error_type}, message={error_msg}, stack={stack}",
            "[{timestamp}] {level} Authentication: user={user}, result={result}, method={auth_method}"
        ]
        
        # 预定义的变量值
        self.methods = ["GET", "POST", "PUT", "DELETE", "PATCH"]
        self.paths = ["/api/users", "/api/orders", "/api/products", "/health", "/metrics", "/login", "/logout"]
        self.status_codes = [200, 201, 400, 401, 403, 404, 500, 502, 503]
        self.levels = ["DEBUG", "INFO", "WARN", "ERROR", "FATAL"]
        self.operations = ["GET", "SET", "DEL", "EXPIRE", "INCR"]
        self.user_actions = ["login", "logout", "view_page", "click_button", "submit_form", "search"]
        self.error_types = ["TimeoutError", "ConnectionError", "ValidationError", "AuthError", "SystemError"]
        
    def generate_log_entry(self) -> str:
        """生成单条日志记录"""
        template = random.choice(self.log_templates)
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        level = random.choice(self.levels)
        
        # 根据模板生成对应的变量值
        variables = {
            "timestamp": timestamp,
            "level": level,
            "method": random.choice(self.methods),
            "path": random.choice(self.paths),
            "status": random.choice(self.status_codes),
            "latency": random.randint(1, 1000),
            "query": f"SELECT * FROM table_{random.randint(1, 100)} WHERE id = {random.randint(1, 10000)}",
            "duration": random.randint(1, 500),
            "rows": random.randint(0, 1000),
            "operation": random.choice(self.operations),
            "key": f"cache:key:{random.randint(1, 10000)}",
            "hit": random.choice(["true", "false"]),
            "user_id": f"user_{random.randint(1000, 9999)}",
            "action": random.choice(self.user_actions),
            "ip": f"{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}",
            "cpu": random.randint(1, 100),
            "memory": random.randint(1, 100),
            "disk": random.randint(1, 100),
            "src": f"{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}",
            "dst": f"{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}",
            "bytes": random.randint(64, 8192),
            "protocol": random.choice(["TCP", "UDP", "HTTP", "HTTPS"]),
            "error_type": random.choice(self.error_types),
            "error_msg": f"Error message {random.randint(1, 1000)}",
            "stack": f"stack_trace_{random.randint(1, 100)}",
            "user": f"user_{random.randint(1, 1000)}",
            "result": random.choice(["success", "failure"]),
            "auth_method": random.choice(["password", "token", "oauth", "saml"])
        }
        
        try:
            log_entry = template.format(**variables)
            self.generated_count += 1
            return log_entry
        except KeyError as e:
            logger.warning(f"Missing template variable: {e}")
            return f"[{timestamp}] {level} Default log entry {self.generated_count}"
    
    def generate_json_log_entry(self) -> str:
        """生成 JSON 格式的日志记录"""
        timestamp = datetime.now().isoformat()
        level = random.choice(self.levels)
        
        log_data = {
            "timestamp": timestamp,
            "level": level,
            "message": f"Test log message {self.generated_count}",
            "service": f"service-{random.randint(1, 10)}",
            "host": f"host-{random.randint(1, 5)}",
            "request_id": f"req-{random.randint(100000, 999999)}",
            "user_id": f"user_{random.randint(1000, 9999)}",
            "duration_ms": random.randint(1, 1000),
            "status_code": random.choice(self.status_codes),
            "metadata": {
                "cpu_usage": random.randint(1, 100),
                "memory_usage": random.randint(1, 100),
                "disk_usage": random.randint(1, 100)
            }
        }
        
        self.generated_count += 1
        return json.dumps(log_data)
    
    def write_logs(self, duration_seconds: int, rate_per_second: int, 
                   log_format: str = "text", file_rotation: bool = True):
        """
        生成日志文件
        
        Args:
            duration_seconds: 生成持续时间（秒）
            rate_per_second: 每秒生成速率
            log_format: 日志格式 (text/json)
            file_rotation: 是否启用文件轮转
        """
        logger.info(f"开始生成日志: 持续时间={duration_seconds}秒, 速率={rate_per_second}条/秒, 格式={log_format}")
        
        start_time = time.time()
        interval = 1.0 / rate_per_second
        current_file_index = 0
        current_file = None
        
        try:
            while time.time() - start_time < duration_seconds:
                # 文件轮转（每小时或每100MB）
                if file_rotation and (current_file is None or 
                    time.time() - start_time > current_file_index * 3600 or
                    (current_file and current_file.stat().st_size > 100 * 1024 * 1024)):
                    
                    if current_file:
                        current_file.close()
                    
                    current_file_index += 1
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    filename = f"test_log_{timestamp}_{current_file_index}.{'json' if log_format == 'json' else 'log'}"
                    filepath = self.output_dir / filename
                    current_file = open(filepath, 'w', encoding='utf-8')
                    logger.info(f"创建新日志文件: {filepath}")
                
                # 生成日志条目
                if log_format == "json":
                    log_entry = self.generate_json_log_entry()
                else:
                    log_entry = self.generate_log_entry()
                
                # 写入文件
                if current_file:
                    current_file.write(log_entry + '\n')
                    current_file.flush()
                else:
                    # 如果不启用文件轮转，直接写入单个文件
                    if not current_file:
                        filename = f"test_log.{'json' if log_format == 'json' else 'log'}"
                        filepath = self.output_dir / filename
                        current_file = open(filepath, 'w', encoding='utf-8')
                    
                    current_file.write(log_entry + '\n')
                    current_file.flush()
                
                # 控制生成速率
                time.sleep(interval)
                
        except KeyboardInterrupt:
            logger.info("接收到中断信号，停止生成日志")
        finally:
            if current_file:
                current_file.close()
        
        total_time = time.time() - start_time
        actual_rate = self.generated_count / total_time if total_time > 0 else 0
        
        logger.info(f"日志生成完成:")
        logger.info(f"  总条数: {self.generated_count}")
        logger.info(f"  总时间: {total_time:.2f}秒")
        logger.info(f"  实际速率: {actual_rate:.2f}条/秒")
        logger.info(f"  输出目录: {self.output_dir}")

def main():
    parser = argparse.ArgumentParser(description="日志生成工具")
    parser.add_argument("--output-dir", "-o", default="/tmp/test-logs", 
                       help="日志输出目录 (默认: /tmp/test-logs)")
    parser.add_argument("--duration", "-d", type=int, default=60,
                       help="生成持续时间（秒，默认: 60）")
    parser.add_argument("--rate", "-r", type=int, default=100,
                       help="每秒生成速率（条/秒，默认: 100）")
    parser.add_argument("--format", "-f", choices=["text", "json"], default="text",
                       help="日志格式 (默认: text)")
    parser.add_argument("--no-rotation", action="store_true",
                       help="禁用文件轮转")
    
    args = parser.parse_args()
    
    # 创建输出目录
    os.makedirs(args.output_dir, exist_ok=True)
    
    # 创建日志生成器
    generator = LogGenerator(args.output_dir)
    
    # 生成日志
    generator.write_logs(
        duration_seconds=args.duration,
        rate_per_second=args.rate,
        log_format=args.format,
        file_rotation=not args.no_rotation
    )

if __name__ == "__main__":
    main()
