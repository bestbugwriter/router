#!/bin/bash

# Exit on error, print commands and their arguments as they are executed.
set -ex
set -o pipefail

# --- Configuration ---
readonly SCRIPT_MODE=${1:-"--integration"} # Default to integration test
if [ "$SCRIPT_MODE" == "--stress" ]; then
    echo "===== Running Full Stress Test ====="
    readonly RUN_DURATION=60
    readonly LOG_GEN_MODE="--fast"
else
    echo "===== Running Full Integration Test ====="
    readonly RUN_DURATION=20
    readonly LOG_GEN_MODE=""
fi

readonly PROJECT_ROOT="$(dirname "$0")/.."
readonly DOCKER_COMPOSE_FILE="$PROJECT_ROOT/docker-compose.test.yml"
readonly ROUTER_EXEC="$PROJECT_ROOT/build/log-router"
readonly AGENT_EXEC="$PROJECT_ROOT/build/log-agent"
readonly ROUTER_CONFIG="$PROJECT_ROOT/config/router.properties.standalone"
readonly AGENT_CONFIG="$PROJECT_ROOT/config/agent.properties.standalone"
readonly TASK_CONFIG_FILE="$PROJECT_ROOT/config/task_config.json"
readonly LOG_GENERATOR_SCRIPT="$PROJECT_ROOT/tests/log_generator.sh"
readonly LOG_FILE="/tmp/test_app.log"

readonly ZK_HOST="127.0.0.1"
readonly ZK_PORT="2181"
readonly ZK_CONTAINER="zookeeper"
readonly KAFKA_TOPIC="log-test-topic"
readonly ZK_TASK_PATH="/log-pipeline/tasks/standalone-group"

# --- Cleanup Function ---
cleanup() {
    echo ""
    echo "--> Cleaning up resources..."
    
    # Stop background processes
    # The '|| true' prevents the script from exiting if the process is already dead
    [ ! -z "$AGENT_PID" ] && kill $AGENT_PID 2>/dev/null || true
    [ ! -z "$ROUTER_PID" ] && kill $ROUTER_PID 2>/dev/null || true
    [ ! -z "$LOG_GEN_PID" ] && kill $LOG_GEN_PID 2>/dev/null || true
    
    # Stop docker containers
    if [ -f "$DOCKER_COMPOSE_FILE" ]; then
        echo "--> Shutting down Docker containers..."
        docker compose -f "$DOCKER_COMPOSE_FILE" down -v --remove-orphans
    fi
    
    # Remove log file
    rm -f "$LOG_FILE"
    
    echo "--> Cleanup complete."
}

# Trap EXIT signal to ensure cleanup runs
trap cleanup EXIT

# --- Main Test Logic ---

# 1. Build the project
echo "--> (1/7) Building the project..."
cd "$PROJECT_ROOT"
./build.sh > /dev/null # Suppress verbose build output
echo "--> Build complete."

# 2. Start Docker environment
echo "--> (2/7) Starting Docker environment (Zookeeper & Kafka)..."
docker compose -f "$DOCKER_COMPOSE_FILE" up -d

# Wait for services to be healthy
echo "--> Waiting for Zookeeper to become healthy..."
if ! command -v nc &> /dev/null; then
    echo "    'nc' (netcat) command not found. Using a fixed 30-second sleep as a fallback."
    echo "    For a faster startup, please install netcat."
    sleep 30
else
    while ! nc -z "$ZK_HOST" "$ZK_PORT"; do
        echo "    Zookeeper port not ready yet, waiting..."
        sleep 2
    done
fi
echo "--> Zookeeper is healthy."
sleep 5 # Give Kafka a bit more time after ZK is up

# 3. Setup Kafka Topic and Zookeeper Task
echo "--> (3/7) Setting up Kafka topic and Zookeeper task..."
# Create Kafka Topic if it doesn't exist
EXISTING_TOPIC=$(docker exec kafka kafka-topics --list --bootstrap-server localhost:9092 | grep -w "$KAFKA_TOPIC" || true)
if [ -z "$EXISTING_TOPIC" ]; then
    echo "--> Topic '$KAFKA_TOPIC' not found, creating it..."
    docker exec kafka kafka-topics --create \
        --topic "$KAFKA_TOPIC" \
        --bootstrap-server localhost:9092 \
        --replication-factor 1 \
        --partitions 1
else
    echo "--> Topic '$KAFKA_TOPIC' already exists, skipping creation."
fi

# Publish task config to Zookeeper
# First, ensure the parent path exists
docker exec "$ZK_CONTAINER" zookeeper-shell "${ZK_HOST}:${ZK_PORT}" create /log-pipeline ""
docker exec "$ZK_CONTAINER" zookeeper-shell "${ZK_HOST}:${ZK_PORT}" create /log-pipeline/tasks ""
# Now create the node with the task data
TASK_JSON=$(cat "$TASK_CONFIG_FILE" | tr -d '\n' | tr -d ' ')
docker exec "$ZK_CONTAINER" zookeeper-shell "${ZK_HOST}:${ZK_PORT}" create "$ZK_TASK_PATH" "$TASK_JSON"
echo "--> Setup complete."

# 4. Start Log Generator
echo "--> (4/7) Starting log generator..."
$LOG_GENERATOR_SCRIPT $LOG_GEN_MODE &
LOG_GEN_PID=$!
sleep 1 # Give it a moment to create the file

# 5. Start Router and Agent
echo "--> (5/7) Starting log-router and log-agent..."
$ROUTER_EXEC $ROUTER_CONFIG &
ROUTER_PID=$!
sleep 1
$AGENT_EXEC $AGENT_CONFIG &
AGENT_PID=$!
echo "--> Services started."

# 6. Run and Verify
echo "--> (6/7) Running pipeline for $RUN_DURATION seconds..."
sleep $RUN_DURATION

echo "--> Verifying results..."
# Get the number of lines generated
GENERATED_COUNT=$(wc -l < "$LOG_FILE")

# Get the number of messages in the Kafka topic
# Use --timeout-ms to prevent hanging if no messages are present
CONSUMED_COUNT=$(docker exec kafka kafka-console-consumer \
    --bootstrap-server localhost:9092 \
    --topic "$KAFKA_TOPIC" \
    --from-beginning \
    --timeout-ms 5000 | wc -l)

echo "--> Generated log lines: $GENERATED_COUNT"
echo "--> Consumed Kafka messages: $CONSUMED_COUNT"

if [ "$SCRIPT_MODE" == "--stress" ]; then
    THROUGHPUT=$((CONSUMED_COUNT / RUN_DURATION))
    echo "--> Average throughput: $THROUGHPUT messages/sec"
    if [ "$THROUGHPUT" -lt "1000" ]; then # Set a baseline expectation for stress test
        echo "✗ Test Failed: Throughput is below the 1000 msg/sec baseline."
        exit 1
    fi
else
    # Verification logic for integration test
    TOLERANCE=5
    DIFFERENCE=$((GENERATED_COUNT - CONSUMED_COUNT))
    DIFFERENCE=${DIFFERENCE#-} # Absolute value

    if [ "$DIFFERENCE" -gt "$TOLERANCE" ]; then
        echo "✗ Test Failed: Mismatch between generated and consumed messages is too high ($DIFFERENCE)."
        exit 1
    fi

    if [ "$CONSUMED_COUNT" -lt "10" ]; then
        echo "✗ Test Failed: Very few messages were consumed, pipeline is likely not working."
        exit 1
    fi
fi

# 7. Success
echo "--> (7/7) Verification successful."
echo ""
if [ "$SCRIPT_MODE" == "--stress" ]; then
    echo "===== Stress Test PASSED! ====="
else
    echo "===== Integration Test PASSED! ====="
fi

# Cleanup will be handled by the EXIT trap
exit 0
