#!/usr/bin/env python3
"""
Kafka 数据验证工具
用于验证从 Agent -> Router -> Kafka 的数据完整性和正确性
"""

import argparse
import json
import logging
import sys
import time
from collections import defaultdict, Counter
from datetime import datetime, timedelta
from pathlib import Path

try:
    from kafka import KafkaConsumer, KafkaAdminClient
    from kafka.admin import NewTopic
    KAFKA_AVAILABLE = True
except ImportError:
    KAFKA_AVAILABLE = False
    print("警告: kafka-python 库未安装，无法使用 Kafka 功能")
    print("请安装: pip install kafka-python")

# 设置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class KafkaValidator:
    def __init__(self, bootstrap_servers: str, topic: str):
        if not KAFKA_AVAILABLE:
            raise ImportError("kafka-python 库未安装")
            
        self.bootstrap_servers = bootstrap_servers
        self.topic = topic
        self.consumer = None
        self.admin_client = None
        
        # 统计信息
        self.total_messages = 0
        self.valid_messages = 0
        self.invalid_messages = 0
        self.duplicate_count = 0
        self.message_hashes = set()
        
        # 性能统计
        self.start_time = None
        self.end_time = None
        self.bytes_received = 0
        
        # 错误统计
        self.error_types = Counter()
        self.late_messages = 0  # 延迟消息
        
    def connect(self):
        """连接到 Kafka 集群"""
        try:
            logger.info(f"连接到 Kafka 集群: {self.bootstrap_servers}")
            
            # 创建 AdminClient
            self.admin_client = KafkaAdminClient(
                bootstrap_servers=self.bootstrap_servers,
                client_id='kafka-validator-admin'
            )
            
            # 创建 Consumer
            self.consumer = KafkaConsumer(
                self.topic,
                bootstrap_servers=self.bootstrap_servers,
                auto_offset_reset='earliest',
                enable_auto_commit=False,
                value_deserializer=lambda m: m.decode('utf-8') if m else None,
                key_deserializer=lambda k: k.decode('utf-8') if k else None,
                consumer_timeout_ms=10000,
                client_id='kafka-validator'
            )
            
            logger.info("Kafka 连接成功")
            return True
            
        except Exception as e:
            logger.error(f"连接 Kafka 失败: {e}")
            return False
    
    def create_topic_if_not_exists(self, num_partitions: int = 3, replication_factor: int = 1):
        """如果 topic 不存在则创建"""
        try:
            existing_topics = self.admin_client.list_topics()
            
            if self.topic not in existing_topics:
                logger.info(f"创建 topic: {self.topic}")
                topic_list = [NewTopic(
                    name=self.topic,
                    num_partitions=num_partitions,
                    replication_factor=replication_factor
                )]
                
                self.admin_client.create_topics(topic_list)
                logger.info(f"Topic {self.topic} 创建成功")
            else:
                logger.info(f"Topic {self.topic} 已存在")
                
        except Exception as e:
            logger.warning(f"创建 topic 失败: {e}")
    
    def validate_message(self, message) -> tuple[bool, str]:
        """
        验证单条消息
        
        Returns:
            (is_valid, error_message)
        """
        try:
            # 检查消息是否为空
            if not message.value:
                return False, "消息内容为空"
            
            # 尝试解析 JSON（如果是 JSON 格式）
            try:
                data = json.loads(message.value)
                
                # 检查必需字段
                required_fields = ['timestamp', 'level', 'message']
                for field in required_fields:
                    if field not in data:
                        return False, f"缺少必需字段: {field}"
                
                # 检查时间戳格式
                try:
                    if 'timestamp' in data:
                        datetime.fromisoformat(data['timestamp'].replace('Z', '+00:00'))
                except ValueError:
                    return False, "时间戳格式无效"
                
            except json.JSONDecodeError:
                # 不是 JSON 格式，检查是否为文本格式
                if len(message.value.strip()) == 0:
                    return False, "消息内容为空白"
            
            # 检查消息重复
            message_hash = hash(message.value)
            if message_hash in self.message_hashes:
                self.duplicate_count += 1
                return False, "重复消息"
            
            self.message_hashes.add(message_hash)
            
            # 检查消息大小
            if len(message.value) > 1024 * 1024:  # 1MB
                return False, "消息过大"
            
            return True, ""
            
        except Exception as e:
            return False, f"验证异常: {str(e)}"
    
    def validate_messages(self, duration_seconds: int = 60, max_messages: int = None) -> dict:
        """
        验证消息流
        
        Args:
            duration_seconds: 验证持续时间（秒）
            max_messages: 最大验证消息数
            
        Returns:
            验证结果字典
        """
        logger.info(f"开始验证消息: 持续时间={duration_seconds}秒, 最大消息数={max_messages}")
        
        self.start_time = time.time()
        processed_messages = 0
        
        try:
            for message in self.consumer:
                current_time = time.time()
                
                # 检查是否超时
                if current_time - self.start_time > duration_seconds:
                    break
                
                # 检查是否达到最大消息数
                if max_messages and processed_messages >= max_messages:
                    break
                
                self.total_messages += 1
                processed_messages += 1
                
                # 更新字节数
                if message.value:
                    self.bytes_received += len(message.value.encode('utf-8'))
                
                # 验证消息
                is_valid, error_msg = self.validate_message(message)
                
                if is_valid:
                    self.valid_messages += 1
                else:
                    self.invalid_messages += 1
                    self.error_types[error_msg] += 1
                    logger.warning(f"无效消息 #{self.total_messages}: {error_msg}")
                
                # 检查消息延迟（基于时间戳）
                if message.value:
                    try:
                        data = json.loads(message.value)
                        if 'timestamp' in data:
                            msg_time = datetime.fromisoformat(data['timestamp'].replace('Z', '+00:00'))
                            latency = current_time - msg_time.timestamp()
                            if latency > 5:  # 5秒延迟
                                self.late_messages += 1
                    except:
                        pass
                
                # 进度报告
                if self.total_messages % 1000 == 0:
                    logger.info(f"已处理 {self.total_messages} 条消息，有效: {self.valid_messages}, 无效: {self.invalid_messages}")
                
        except KeyboardInterrupt:
            logger.info("接收到中断信号，停止验证")
        except Exception as e:
            logger.error(f"验证过程中发生错误: {e}")
        finally:
            self.end_time = time.time()
        
        return self.generate_report()
    
    def generate_report(self) -> dict:
        """生成验证报告"""
        duration = self.end_time - self.start_time if self.end_time and self.start_time else 0
        throughput = self.total_messages / duration if duration > 0 else 0
        bytes_per_second = self.bytes_received / duration if duration > 0 else 0
        
        report = {
            "validation_summary": {
                "total_messages": self.total_messages,
                "valid_messages": self.valid_messages,
                "invalid_messages": self.invalid_messages,
                "duplicate_messages": self.duplicate_count,
                "late_messages": self.late_messages,
                "success_rate": (self.valid_messages / self.total_messages * 100) if self.total_messages > 0 else 0
            },
            "performance_metrics": {
                "duration_seconds": duration,
                "throughput_msgs_per_sec": throughput,
                "bytes_received": self.bytes_received,
                "bytes_per_sec": bytes_per_second,
                "avg_msg_size": self.bytes_received / self.total_messages if self.total_messages > 0 else 0
            },
            "error_breakdown": dict(self.error_types),
            "validation_timestamp": datetime.now().isoformat()
        }
        
        return report
    
    def print_report(self, report: dict):
        """打印验证报告"""
        print("\n" + "="*60)
        print("KAFKA 数据验证报告")
        print("="*60)
        
        summary = report["validation_summary"]
        print(f"总消息数: {summary['total_messages']}")
        print(f"有效消息: {summary['valid_messages']}")
        print(f"无效消息: {summary['invalid_messages']}")
        print(f"重复消息: {summary['duplicate_messages']}")
        print(f"延迟消息: {summary['late_messages']}")
        print(f"成功率: {summary['success_rate']:.2f}%")
        
        perf = report["performance_metrics"]
        print(f"\n性能指标:")
        print(f"  持续时间: {perf['duration_seconds']:.2f} 秒")
        print(f"  吞吐量: {perf['throughput_msgs_per_sec']:.2f} 条/秒")
        print(f"  接收字节数: {perf['bytes_received']:,} 字节")
        print(f"  字节速率: {perf['bytes_per_sec']:.2f} 字节/秒")
        print(f"  平均消息大小: {perf['avg_msg_size']:.2f} 字节")
        
        errors = report["error_breakdown"]
        if errors:
            print(f"\n错误统计:")
            for error_type, count in errors.items():
                print(f"  {error_type}: {count}")
        
        print("="*60)
    
    def close(self):
        """关闭连接"""
        if self.consumer:
            self.consumer.close()
        if self.admin_client:
            self.admin_client.close()

def main():
    parser = argparse.ArgumentParser(description="Kafka 数据验证工具")
    parser.add_argument("--bootstrap-servers", "-b", default="localhost:9092",
                       help="Kafka bootstrap servers (默认: localhost:9092)")
    parser.add_argument("--topic", "-t", default="test-logs",
                       help="Kafka topic (默认: test-logs)")
    parser.add_argument("--duration", "-d", type=int, default=60,
                       help="验证持续时间（秒，默认: 60）")
    parser.add_argument("--max-messages", "-m", type=int,
                       help="最大验证消息数")
    parser.add_argument("--create-topic", action="store_true",
                       help="如果 topic 不存在则创建")
    parser.add_argument("--output-file", "-o",
                       help="将报告保存到文件")
    
    args = parser.parse_args()
    
    if not KAFKA_AVAILABLE:
        print("错误: kafka-python 库未安装")
        print("请安装: pip install kafka-python")
        sys.exit(1)
    
    # 创建验证器
    validator = KafkaValidator(args.bootstrap_servers, args.topic)
    
    try:
        # 连接到 Kafka
        if not validator.connect():
            sys.exit(1)
        
        # 创建 topic（如果需要）
        if args.create_topic:
            validator.create_topic_if_not_exists()
        
        # 验证消息
        report = validator.validate_messages(
            duration_seconds=args.duration,
            max_messages=args.max_messages
        )
        
        # 打印报告
        validator.print_report(report)
        
        # 保存报告到文件
        if args.output_file:
            with open(args.output_file, 'w') as f:
                json.dump(report, f, indent=2, ensure_ascii=False)
            logger.info(f"报告已保存到: {args.output_file}")
        
        # 根据成功率返回退出码
        success_rate = report["validation_summary"]["success_rate"]
        if success_rate >= 95:
            print(f"\n✓ 验证通过 (成功率: {success_rate:.2f}%)")
            sys.exit(0)
        else:
            print(f"\n✗ 验证失败 (成功率: {success_rate:.2f}%)")
            sys.exit(1)
            
    except Exception as e:
        logger.error(f"验证过程中发生错误: {e}")
        sys.exit(1)
    finally:
        validator.close()

if __name__ == "__main__":
    main()
