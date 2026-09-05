# AI-VOX 教室学生行为管理物联网终端 (四年级智慧教室)

基于 **AI-VOX (ESP32-S3)** 开发板构建的班级智能提醒与文化展示终端。
通过轻量级物联网（MQTT / Easy IoT）与老师电脑上的“教室学生行为管理系统”无缝联动，实现**打架推搡声光预警**与**班级流行语热词轮播**两大核心功能。

---

## 🌟 核心特性

1. **安全预警（打架冲突即刻通知）**：
   - 电脑端视觉/行为系统检测到肢体冲突时，终端在 1 秒内切换为**高对比度警戒红屏**；
   - 扬声器鸣响急促警报音（双音高频警报），板载 WS2812 RGB 强闪红光；
   - 现场当堂教师按下板载 **BOOT 键**即可一键消除警报，并向电脑端回传“老师已现场处理”状态。
2. **班级文化（流行语榜单）**：
   - 展示小学生流行语热词 Top 1~5，金、银、铜牌动态徽章与词频统计；
   - 收到新榜单时播放欢快清脆的“和弦提示音”，绿色/青色呼吸灯轻闪；
   - 展示 15 秒后自动柔和切回“日常守护主屏”。
3. **针对四年级小学生的视觉优化**：
   - 240x240 全彩 ST7789 IPS 屏，配合阿里普惠体汉字字库；
   - 日常状态显示可爱的机器人/守护笑脸徽章；
   - 声光适度，提示明了，避免过度惊吓学生。

---

## 📁 项目工程目录

```
/Users/joeygu/Documents/Arduino/IOT/
├── IOT.ino                 # Arduino 主程序 (状态机调度、按键响应、主循环)
├── config.h                # 统一参数配置 (WiFi名称密码、MQTT/Easy IoT服务器、引脚)
├── display_ui.h/.cpp       # 屏幕渲染引擎 (LVGL 9 驱动，守护/警报/榜单三大界面)
├── audio_player.h/.cpp     # 音频引擎 (ES8311 编解码器驱动，警报音、和弦音、消警音)
├── iot_client.h/.cpp       # 物联网通信 (WiFi自动重连、MQTT订阅、JSON事件解析)
├── partitions.csv          # 16MB Flash / 4MB App 分区配置表
│
└── pc_simulator/           # 电脑端集成工具包
    ├── iot_bridge.py       # Python 对接模块 (直接 import 到您现有的行为管理系统)
    ├── test_sender.py      # 交互式测试发送端控制台
    └── requirements.txt    # Python 依赖 (paho-mqtt)
```

---

## 🚀 快速上手与烧录步骤

### 步骤 1：修改 WiFi 与物联网配置
打开 [`config.h`](file:///Users/joeygu/Documents/Arduino/IOT/config.h)，修改您的无线网络信息：
```cpp
// 1. 修改为您现场的 WiFi
#define WIFI_SSID     "您的WiFi名称"
#define WIFI_PASSWORD "您的WiFi密码"

// 2. 选择物联网平台模式:
// 0: 使用公共 MQTT (broker.emqx.io，免注册直接用，推荐测试)
// 1: 使用 DFRobot Easy IoT (填写您的 Easy IoT ID、密码及Topic)
#define USE_EASY_IOT 0
```

### 步骤 2：编译与烧录到开发板

#### 方式 A：通过命令行一键烧录 (推荐，无需打开 IDE 配置参数)
在终端中执行：
```bash
# 1. 编译工程
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" compile -b esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi,FlashSize=16M,PartitionScheme=custom /Users/joeygu/Documents/Arduino/IOT/IOT.ino

# 2. 烧录上传至板子
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" upload -p /dev/cu.usbmodem311201 -b esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi,FlashSize=16M,PartitionScheme=custom /Users/joeygu/Documents/Arduino/IOT/IOT.ino
```

#### 方式 B：通过 Arduino IDE 图形界面
1. 用 Arduino IDE 打开 `/Users/joeygu/Documents/Arduino/IOT/IOT.ino`；
2. 菜单栏选择 **工具 (Tools)**：
   - 开发板 (Board)：`ESP32S3 Dev Module`
   - USB CDC On Boot：`Enabled`
   - PSRAM：`OPI PSRAM`
   - Flash Size：`16MB (128Mb)`
   - Partition Scheme：`Custom` (会自动加载同目录下的 `partitions.csv`)
   - 端口 (Port)：选择 `/dev/cu.usbmodem311201`
3. 点击顶部 **上传 (Upload)** 按钮。

---

## 💻 电脑端：模拟测试与对接指南

### 1. 运行交互式测试控制台
在老师电脑的终端运行：
```bash
cd /Users/joeygu/Documents/Arduino/IOT/pc_simulator
python3 test_sender.py
```
控制台将呈现操作菜单：
- 按 `1`：推送后排图书角【打架预警】 -> 板子即刻红屏并响警报；
- 按 `2`：推送走廊过道【冲突预警】 -> 板子显示详细位置提示；
- 按 `3` / `4`：推送【四年级流行语榜单】 -> 板子奏响欢快和弦并展示 Top 5 卡片；
- 按 `5`：推送【远程消警】 -> 板子播放消警音回到日常守护屏；
- 在板子上按下 **BOOT 键**：控制台将收到 `[终端实时反馈] 老师已现场消警` 的回执！

### 2. 嵌入到现有的“教室学生行为管理系统”代码中
只需 3 步即可将预警能力接入现有 Python 系统：

```python
from iot_bridge import ClassroomIotBridge

# 1. 初始化并连接
iot = ClassroomIotBridge(broker="broker.emqx.io")
iot.connect()

# 2. 当您的视觉识别模型检测到打架/肢体冲突时调用:
iot.send_conflict_alert(
    location="3组图书角",
    message="检测到学生推搡打架，请立即关注！"
)

# 3. 当您的自然语言/词频统计模块统计出流行语时调用:
iot.send_slang_ranking(
    title="四年级本周流行语榜",
    items=[
        {"rank": 1, "word": "绝绝子", "count": 68},
        {"rank": 2, "word": "泰裤辣", "count": 52},
        {"rank": 3, "word": "尊嘟假嘟", "count": 41},
        {"rank": 4, "word": "遥遥领先", "count": 28},
        {"rank": 5, "word": "City不City", "count": 23},
    ]
)
```

---

## 🔔 常见问题与排查

1. **板子开机提示“WiFi连接中”？**
   - 检查 `config.h` 中的 `WIFI_SSID` 和 `WIFI_PASSWORD` 是否正确（注意 ESP32 仅支持 2.4GHz WiFi 频段）。
2. **打架警报响了如何快速关闭？**
   - 按下板子侧面或正面的 **BOOT 按键 (GPIO 0)** 即可立刻消除警报与声音；
   - 默认 30 秒内无人按键也会自动恢复，避免影响课堂教学。
3. **如何调整扬声器音量？**
   - 修改 `config.h` 中的 `#define DEFAULT_SPEAKER_VOL 85`（数值范围 0~100）。
