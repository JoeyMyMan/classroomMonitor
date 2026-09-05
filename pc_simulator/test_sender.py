#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI-VOX 教室学生行为管理系统 - 模拟发送端控制台
用于在电脑端快速向 AI-VOX 硬件终端模拟推送【打架预警】与【流行语排行】。
"""

import sys
import time
from iot_bridge import ClassroomIotBridge

SAMPLE_RANKINGS_1 = [
    {"rank": 1, "word": "绝绝子", "count": 68},
    {"rank": 2, "word": "泰裤辣", "count": 52},
    {"rank": 3, "word": "尊嘟假嘟", "count": 41},
    {"rank": 4, "word": "遥遥领先", "count": 28},
    {"rank": 5, "word": "City不City", "count": 23},
]

SAMPLE_RANKINGS_2 = [
    {"rank": 1, "word": "硬控我五秒", "count": 75},
    {"rank": 2, "word": "笑发财了", "count": 61},
    {"rank": 3, "word": "绝绝子", "count": 49},
    {"rank": 4, "word": "尊嘟假嘟", "count": 36},
    {"rank": 5, "word": "显眼包", "count": 30},
]


def on_terminal_feedback(payload):
    print("\n" + "=" * 50)
    print("📢 [终端实时反馈] 收到 AI-VOX 板载事件上报:")
    event = payload.get("event", "未知")
    if event == "alert_dismissed_by_teacher":
        print("✅ 老师已在现场按下 BOOT 键确认并解除了打架警报！")
    else:
        print(f"详情: {payload}")
    print("=" * 50)
    print("请输入选项 (1-5, q退出): ", end="", flush=True)


def main():
    print("=" * 60)
    print("   AI-VOX 教室学生行为管理系统 - IoT 模拟发送终端")
    print("=" * 60)

    # 默认连接公共 EMQX 服务器 (与 config.h 中默认设置一致)
    bridge = ClassroomIotBridge(
        broker="broker.emqx.io",
        port=1883,
        event_topic="classroom/grade4/events",
        status_topic="classroom/grade4/status",
    )
    bridge.on_terminal_status_callback = on_terminal_feedback

    if not bridge.connect(timeout=6):
        print("❌ 连接 MQTT 服务器超时，请检查电脑网络连接！")
        return

    print("✅ 已成功连接至 IoT 云端平台！")
    print("提示：板子连接同一 WiFi 后即可实时接收如下指令：\n")

    menu = """
------------------------------------------------------------
[1] 模拟发送：后排图书角【打架/推搡预警】 (紧急红色警报)
[2] 模拟发送：讲台走廊【肢体冲突预警】 (紧急红色警报)
[3] 模拟发送：本周【四年级流行语排行榜 A】 (欢乐提示音)
[4] 模拟发送：本周【四年级流行语排行榜 B】 (榜单变动刷新)
[5] 模拟发送：【远程消警/复位】 (恢复日常守护主屏)
[q] 退出测试程序
------------------------------------------------------------
"""
    try:
        while True:
            print(menu)
            choice = input("请输入操作编号: ").strip()

            if choice == "1":
                bridge.send_conflict_alert(location="图书角后排", message="检测到疑似推搡打架行为！")
                print(">> [发送成功] 终端屏幕应变红、高频鸣叫警报、红灯闪烁。")
            elif choice == "2":
                bridge.send_conflict_alert(location="2组与3组过道", message="检测到学生发生推搡冲突！")
                print(">> [发送成功] 终端屏幕应显示走廊冲突，提醒当堂老师即刻前往。")
            elif choice == "3":
                bridge.send_slang_ranking("四年级热词榜·第1周", SAMPLE_RANKINGS_1)
                print(">> [发送成功] 终端应播放欢快和弦音，展示前5名流行语卡片。")
            elif choice == "4":
                bridge.send_slang_ranking("四年级热词榜·第2周", SAMPLE_RANKINGS_2)
                print(">> [发送成功] 终端应播放欢快和弦音，展示刷新后的热词。")
            elif choice == "5":
                bridge.send_clear_alert()
                print(">> [发送成功] 终端应消除警报，播放消警提示音并回到守护主屏。")
            elif choice.lower() in ["q", "quit", "exit"]:
                print("正在退出...")
                break
            else:
                print("无效输入，请输入数字 1-5 或 q。")

            time.sleep(0.5)

    except KeyboardInterrupt:
        print("\n程序终止。")
    finally:
        bridge.disconnect()


if __name__ == "__main__":
    main()
