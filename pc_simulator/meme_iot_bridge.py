#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Leo6662233/meme 专用物联网桥接服务 (Meme IoT Bridge)

工作原理:
1. 自动连接 http://127.0.0.1:5000 (即 Leo6662233/meme 的 Web 仪表盘)
2. 实时监测视觉打架状态 (/api/vision/state):
   - 检测到打架 (fighting=True) -> 立即触发 AI-VOX 板子红屏警报与鸣响
   - 打架平息 (fighting=False) -> 自动恢复 AI-VOX 板子为日常守护 ("一切正常(检测到 N 个人)")
3. 自动监测热梗排行榜 (/api/ranking):
   - 语音转写出新梗或热词次数更新 -> 立即触发 AI-VOX 板子播放和弦音并展示热梗 Top 5 卡片

使用方法:
在运行 Leo6662233/meme 的电脑上运行:
   pip install paho-mqtt requests
   python meme_iot_bridge.py
"""

import time
import json
import logging
import requests
from iot_bridge import ClassroomIotBridge

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("MemeIoTBridge")

WEBAPP_URL = "http://127.0.0.1:5000"
MQTT_BROKER = "broker.emqx.io"
EVENT_TOPIC = "classroom/grade4/events"
STATUS_TOPIC = "classroom/grade4/status"


def main():
    logger.info("=" * 60)
    logger.info("  Leo6662233/meme -> AI-VOX 物联网无缝联动网桥启动中...")
    logger.info("=" * 60)

    # 1. 启动 MQTT 连接
    bridge = ClassroomIotBridge(
        broker=MQTT_BROKER,
        port=1883,
        event_topic=EVENT_TOPIC,
        status_topic=STATUS_TOPIC,
    )
    if not bridge.connect(timeout=6):
        logger.error("连接 MQTT 云端失败，请检查网络！")
        return

    logger.info("✅ 已连接至 MQTT 云端平台，开始监听本地检测系统 (http://127.0.0.1:5000)...")

    last_fighting = False
    last_ranking_sig = ""
    last_ranking_check_time = 0

    try:
        while True:
            # -------------------------------------------------------------
            # 1. 轮询视觉打架检测状态 (/api/vision/state) - 高频(每0.5秒)
            # -------------------------------------------------------------
            try:
                resp = requests.get(f"{WEBAPP_URL}/api/vision/state", timeout=0.8)
                if resp.status_code == 200:
                    state = resp.json()
                    fighting = state.get("fighting", False)
                    persons = state.get("persons", 0)

                    # 状态突变: 触发打架
                    if fighting and not last_fighting:
                        logger.warning(f"🚨 [视觉系统报警] 检测到打架冲突！人数: {persons}")
                        bridge.client.publish(
                            EVENT_TOPIC,
                            json.dumps({
                                "fighting": True,
                                "persons": persons,
                                "location": "教室摄像头区域",
                                "message": f"检测到 {persons} 人推搡打架！",
                                "timestamp": int(time.time()),
                            }, ensure_ascii=False)
                        )

                    # 状态突变: 打架停止，恢复正常
                    elif not fighting and last_fighting:
                        logger.info(f"🟢 [视觉系统恢复] 打架已平息，当前人数: {persons}")
                        bridge.client.publish(
                            EVENT_TOPIC,
                            json.dumps({
                                "fighting": False,
                                "persons": persons,
                                "timestamp": int(time.time()),
                            }, ensure_ascii=False)
                        )

                    last_fighting = fighting
            except requests.exceptions.RequestException:
                # Web 页面未启动时静默等待
                pass

            # -------------------------------------------------------------
            # 2. 轮询热梗排行榜 (/api/ranking) - 低频(每 2.5 秒)
            # -------------------------------------------------------------
            now = time.time()
            if now - last_ranking_check_time > 2.5:
                last_ranking_check_time = now
                try:
                    r_resp = requests.get(f"{WEBAPP_URL}/api/ranking", timeout=1.5)
                    if r_resp.status_code == 200:
                        data = r_resp.json()
                        ranking_list = data.get("ranking", [])
                        # 取前 5 个有效梗作为签名
                        sig = "|".join([f"{item.get('name')}:{item.get('count')}" for item in ranking_list[:5]])

                        if sig and sig != last_ranking_sig:
                            logger.info(f"🔥 [热梗榜单更新] 检测到最新词频变动: {sig}")
                            last_ranking_sig = sig

                            # 直接将 Leo6662233/meme 的排行推送到板子
                            bridge.client.publish(
                                EVENT_TOPIC,
                                json.dumps({
                                    "type": "ranking",
                                    "title": "四年级热梗排行榜",
                                    "ranking": ranking_list[:5],
                                    "timestamp": int(time.time()),
                                }, ensure_ascii=False)
                            )
                except requests.exceptions.RequestException:
                    pass

            time.sleep(0.5)

    except KeyboardInterrupt:
        logger.info("网桥已停止运行。")
    finally:
        bridge.disconnect()


if __name__ == "__main__":
    main()
