#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Classroom IoT Bridge for AI-VOX Terminal
教室学生行为管理系统 - 物联网传输接口模块

可在老师电脑上的行为管理系统中直接引用:
    from iot_bridge import ClassroomIotBridge

    iot = ClassroomIotBridge(broker="broker.emqx.io")
    iot.connect()
    
    # 检测到打架冲突时发送:
    iot.send_conflict_alert(location="图书角", message="检测到推搡打架！")
    
    # 周期性更新流行语榜单时发送:
    iot.send_slang_ranking("四年级热词榜", [
        {"rank": 1, "word": "绝绝子", "count": 58},
        {"rank": 2, "word": "泰裤辣", "count": 46},
        {"rank": 3, "word": "尊嘟假嘟", "count": 35},
        {"rank": 4, "word": "遥遥领先", "count": 21},
        {"rank": 5, "word": "City不City", "count": 19}
    ])
"""

import json
import time
import logging
from typing import List, Dict, Optional, Callable

try:
    import paho.mqtt.client as mqtt
except ImportError:
    mqtt = None

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("ClassroomIotBridge")


class ClassroomIotBridge:
    def __init__(
        self,
        broker: str = "broker.emqx.io",
        port: int = 1883,
        client_id: str = "classroom_pc_system",
        username: Optional[str] = None,
        password: Optional[str] = None,
        event_topic: str = "classroom/grade4/events",
        status_topic: str = "classroom/grade4/status",
        is_easy_iot: bool = False,
        easy_iot_alert_topic: Optional[str] = None,
        easy_iot_rank_topic: Optional[str] = None,
    ):
        if mqtt is None:
            raise ImportError(
                "请先安装 paho-mqtt 依赖库: pip install paho-mqtt"
            )

        self.broker = broker
        self.port = port
        self.client_id = client_id
        self.username = username
        self.password = password
        self.event_topic = event_topic
        self.status_topic = status_topic
        self.is_easy_iot = is_easy_iot
        self.easy_iot_alert_topic = easy_iot_alert_topic
        self.easy_iot_rank_topic = easy_iot_rank_topic

        self.on_terminal_status_callback: Optional[Callable[[dict], None]] = None

        # 兼容 paho-mqtt v1 和 v2 API
        try:
            self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=self.client_id)
        except AttributeError:
            self.client = mqtt.Client(client_id=self.client_id)

        if self.username and self.password:
            self.client.username_pw_set(self.username, self.password)

        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self._connected = False

    def _on_connect(self, client, userdata, flags, rc, *args):
        if rc == 0:
            self._connected = True
            logger.info(f"成功连接至 IoT Broker: {self.broker}:{self.port}")
            if not self.is_easy_iot and self.status_topic:
                client.subscribe(self.status_topic)
                logger.info(f"已订阅终端反馈主题: {self.status_topic}")
        else:
            logger.error(f"连接 IoT Broker 失败，返回码: {rc}")

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
            logger.info(f"收到 AI-VOX 终端上报状态: {payload}")
            if self.on_terminal_status_callback:
                self.on_terminal_status_callback(payload)
        except Exception as e:
            logger.debug(f"收到原始消息: {msg.payload}")

    def connect(self, timeout: int = 5) -> bool:
        """连接至 MQTT 服务器并启动后台循环"""
        logger.info(f"正在连接至 MQTT 服务器: {self.broker}:{self.port} ...")
        self.client.connect(self.broker, self.port, keepalive=60)
        self.client.loop_start()

        start = time.time()
        while not self._connected and (time.time() - start) < timeout:
            time.sleep(0.1)

        return self._connected

    def disconnect(self):
        """断开连接"""
        self.client.loop_stop()
        self.client.disconnect()
        self._connected = False
        logger.info("已断开 IoT 连接")

    def send_conflict_alert(self, location: str = "教室后排", message: str = "检测到推搡打架！") -> bool:
        """
        向 AI-VOX 终端推送学生打架/冲突警报
        """
        payload = {
            "type": "alert",
            "event": "fight",
            "level": "danger",
            "location": location,
            "message": message,
            "timestamp": int(time.time()),
        }
        topic = self.easy_iot_alert_topic if self.is_easy_iot else self.event_topic
        data_str = json.dumps(payload, ensure_ascii=False)
        result = self.client.publish(topic, data_str, qos=1)
        logger.info(f"已推送打架预警 -> [{topic}]: {data_str}")
        return result.rc == mqtt.MQTT_ERR_SUCCESS

    def send_slang_ranking(self, title: str, items: List[Dict]) -> bool:
        """
        向 AI-VOX 终端推送学生流行语热词排行榜
        :param title: 榜单标题 (如: '四年级热词榜')
        :param items: 列表，例如 [{'rank': 1, 'word': '绝绝子', 'count': 58}, ...]
        """
        payload = {
            "type": "ranking",
            "title": title,
            "items": items,
            "timestamp": int(time.time()),
        }
        topic = self.easy_iot_rank_topic if self.is_easy_iot else self.event_topic
        data_str = json.dumps(payload, ensure_ascii=False)
        result = self.client.publish(topic, data_str, qos=1)
        logger.info(f"已推送流行语榜单 -> [{topic}]: {data_str}")
        return result.rc == mqtt.MQTT_ERR_SUCCESS

    def send_clear_alert(self) -> bool:
        """
        向 AI-VOX 终端推送远程消警指令
        """
        payload = {
            "type": "clear",
            "action": "dismiss",
            "timestamp": int(time.time()),
        }
        topic = self.event_topic
        data_str = json.dumps(payload, ensure_ascii=False)
        result = self.client.publish(topic, data_str, qos=1)
        logger.info(f"已推送远程消警 -> [{topic}]: {data_str}")
        return result.rc == mqtt.MQTT_ERR_SUCCESS
