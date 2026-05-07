#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
小智 IoT MQTT 自动化调试脚本
模拟：从机上线 -> 主机发现 -> 主机控制 -> 从机响应
"""
import paho.mqtt.client as mqtt
import time
import threading
import sys
import io

# 确保输出使用 UTF-8
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

BROKER = "127.0.0.1"
PORT = 1883

events = []

def log(msg):
    ts = time.strftime("%H:%M:%S")
    events.append(f"[{ts}] {msg}")
    print(f"[{ts}] {msg}", flush=True)

# 模拟从机（一楼设备）
slave_client = mqtt.Client(client_id="slave_1", protocol=mqtt.MQTTv311)
slave_messages = []
slave_ready = threading.Event()

def slave_on_connect(client, userdata, flags, rc):
    if rc == 0:
        log("[从机] 连接成功")
        client.subscribe("xiaozhi/iot/cmd/1")
        client.subscribe("xiaozhi/iot/cmd/broadcast")
        log("[从机] 已订阅 cmd/1 和 cmd/broadcast")
        slave_ready.set()
    else:
        log(f"[从机] 连接失败，错误码: {rc}")

def slave_on_message(client, userdata, msg):
    payload = msg.payload.hex()
    log(f"[从机] 收到消息: topic={msg.topic}, hex={payload}")
    slave_messages.append((msg.topic, payload))

slave_client.on_connect = slave_on_connect
slave_client.on_message = slave_on_message

# 模拟主机（中控）
master_client = mqtt.Client(client_id="xiaozhi_master", protocol=mqtt.MQTTv311)
master_messages = []
master_ready = threading.Event()

def master_on_connect(client, userdata, flags, rc):
    if rc == 0:
        log("[主机] 连接成功")
        client.subscribe("xiaozhi/iot/resp/#")
        client.subscribe("xiaozhi/iot/heartbeat/#")
        client.subscribe("xiaozhi/iot/announce/#")
        log("[主机] 已订阅 resp/#, heartbeat/#, announce/#")
        master_ready.set()
    else:
        log(f"[主机] 连接失败，错误码: {rc}")

def master_on_message(client, userdata, msg):
    payload = msg.payload.hex()
    log(f"[主机] 收到消息: topic={msg.topic}, hex={payload}")
    master_messages.append((msg.topic, payload))

master_client.on_connect = master_on_connect
master_client.on_message = master_on_message

def wait_for_message(client, messages_list, topic_contains, timeout=5):
    start = time.time()
    while time.time() - start < timeout:
        for topic, _ in messages_list:
            if topic_contains in topic:
                return True
        time.sleep(0.1)
    return False

def run_test():
    log("=" * 50)
    log("小智 IoT MQTT 自动化调试开始")
    log("=" * 50)

    # 1. 连接
    log("\n--- 步骤 1: 连接 Broker ---")
    slave_client.connect(BROKER, PORT, 60)
    slave_client.loop_start()

    master_client.connect(BROKER, PORT, 60)
    master_client.loop_start()

    slave_ready.wait(timeout=5)
    master_ready.wait(timeout=5)
    time.sleep(0.5)

    # 2. 从机上线（ANNOUNCE_V2）
    log("\n--- 步骤 2: 从机上线（ANNOUNCE_V2）---")
    mac = "AA1122334455"
    announce_payload = bytes([0x08, 0x01, 0x04, 0x00]) + "一楼设备".encode("utf-8")
    slave_client.publish(f"xiaozhi/iot/announce/{mac}", announce_payload)
    log(f"[从机] 发布上线公告: MAC={mac}, name=一楼设备")

    if wait_for_message(master_client, master_messages, "announce/"):
        log("[主机] [OK] 收到从机上线公告")
    else:
        log("[主机] [FAIL] 未收到上线公告！")

    # 3. 从机发送心跳
    log("\n--- 步骤 3: 从机发送心跳 ---")
    heartbeat_payload = bytes([0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    slave_client.publish(f"xiaozhi/iot/heartbeat/{mac}", heartbeat_payload)
    log(f"[从机] 发布心跳: MAC={mac}")

    if wait_for_message(master_client, master_messages, "heartbeat/"):
        log("[主机] [OK] 收到从机心跳")
    else:
        log("[主机] [FAIL] 未收到心跳！")

    # 4. 主机控制：开灯
    log("\n--- 步骤 4: 主机发送开灯命令 ---")
    gpio_cmd = bytes([0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00])
    master_client.publish("xiaozhi/iot/cmd/1", gpio_cmd)
    log("[主机] 发布开灯命令: SET_GPIO device=1 gpio=0 level=1")

    if wait_for_message(slave_client, slave_messages, "cmd/1"):
        log("[从机] [OK] 收到开灯命令")
    else:
        log("[从机] [FAIL] 未收到开灯命令！")

    # 5. 从机响应
    log("\n--- 步骤 5: 从机回复响应 ---")
    resp_payload = bytes([0x05, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00])
    slave_client.publish(f"xiaozhi/iot/resp/{mac}", resp_payload)
    log("[从机] 发布响应: RESPONSE device=1 gpio=0 level=1")

    if wait_for_message(master_client, master_messages, "resp/"):
        log("[主机] [OK] 收到从机响应")
    else:
        log("[主机] [FAIL] 未收到响应！")

    # 6. 主机控制：舵机开 180 度
    log("\n--- 步骤 6: 主机发送舵机命令 ---")
    servo_cmd = bytes([0x10, 0x01, 0x00, 180, 0x00, 0x00, 0x00, 0x00])
    master_client.publish("xiaozhi/iot/cmd/1", servo_cmd)
    log("[主机] 发布舵机命令: SET_SERVO device=1 servo=0 angle=180")

    slave_messages.clear()
    if wait_for_message(slave_client, slave_messages, "cmd/1"):
        log("[从机] [OK] 收到舵机命令")
    else:
        log("[从机] [FAIL] 未收到舵机命令！")

    # 7. 广播命令测试
    log("\n--- 步骤 7: 主机发送广播命令（全关）---")
    broadcast_cmd = bytes([0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    master_client.publish("xiaozhi/iot/cmd/broadcast", broadcast_cmd)
    log("[主机] 发布广播命令: BROADCAST_ALL_OFF")

    slave_messages.clear()
    if wait_for_message(slave_client, slave_messages, "cmd/broadcast"):
        log("[从机] [OK] 收到广播命令")
    else:
        log("[从机] [FAIL] 未收到广播命令！")

    # 8. 测试隔离性
    log("\n--- 步骤 8: 测试隔离性（发给设备2，设备1不应收到）---")
    cmd_device2 = bytes([0x01, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00])
    master_client.publish("xiaozhi/iot/cmd/2", cmd_device2)
    log("[主机] 发布 cmd/2 命令（一楼设备不应收到）")

    slave_messages.clear()
    time.sleep(1)
    if not any("cmd/2" in t for t, _ in slave_messages):
        log("[从机] [OK] 正确忽略：没有收到 cmd/2 的消息")
    else:
        log("[从机] [FAIL] 错误：收到了不该收到的 cmd/2！")

    # 清理
    slave_client.loop_stop()
    slave_client.disconnect()
    master_client.loop_stop()
    master_client.disconnect()

    # 总结
    log("\n" + "=" * 50)
    log("调试完成！")
    log("=" * 50)

if __name__ == "__main__":
    run_test()
