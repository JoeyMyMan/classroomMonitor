# 教室学生行为管理系统 - 另一台电脑接入 AI-VOX 终端指南

本文档用于指导如何将运行在**另一台电脑上的“教室学生行为管理系统”**与已运行就绪的 **AI-VOX 智能硬件终端** 进行物联网打通。

---

## 📌 核心连接参数清单（请提供给另一台电脑的开发者/系统）

另一台电脑只需要知道以下 **5 项物联网核心配置**，即可跨网络向板子发送指令：

| 配置项 | 参数值 | 说明 |
| :--- | :--- | :--- |
| **MQTT 服务器 (Broker)** | `broker.emqx.io` | 公共免费 MQTT 服务器（已在板子中配置就绪） |
| **服务器端口 (Port)** | `1883` | 标准 MQTT 端口（TCP明文） |
| **账号 / 密码** | *(留空即可)* | 公共测试服务器无需身份认证 |
| **事件发送主题 (Event Topic)** | `classroom/grade4/events` | 电脑端向板子**发送预警与排行榜**的主题 |
| **终端反馈主题 (Status Topic)** | `classroom/grade4/status` | *(可选)* 板子向电脑**回传老师现场消警状态**的主题 |

> [!NOTE]
> **网络环境要求说明**：
> 1. 板子与另一台电脑**不需要在同一个局域网**！只要双方都能连接互联网（例如电脑连校园网，板子连手机热点或路由器），就可以通过云端 Broker 实时通信。
> 2. 如果后期需要在**纯断网/局域网**环境下运行，只需在电脑上安装一个 Mosquitto 本地 Broker，然后将两边的 Broker 地址统一修改为电脑的局域网 IP（如 `192.168.1.xxx`）即可。

---

## 📦 需要拷贝到另一台电脑的文件

我们已经在本项目中为您封装好了即插即用的 Python 通信模块。您只需要将以下文件通过 U盘 / 微信 / 局域网发送给另一台电脑：

1. **核心通信模块**：
   - 路径：[`/Users/joeygu/Documents/Arduino/IOT/pc_simulator/iot_bridge.py`](file:///Users/joeygu/Documents/Arduino/IOT/pc_simulator/iot_bridge.py)
   - 作用：封装了自动重连、JSON 消息格式化与发送接口，另一台电脑无需手写复杂的 MQTT 底层代码。
2. **测试脚本（可选）**：
   - 路径：[`/Users/joeygu/Documents/Arduino/IOT/pc_simulator/test_sender.py`](file:///Users/joeygu/Documents/Arduino/IOT/pc_simulator/test_sender.py)
   - 作用：可在另一台电脑上直接双击或命令行运行，验证另一台电脑是否能成功控制板子。

---

## 💻 另一台电脑上的安装与集成步骤

### 第一步：安装依赖库（在另一台电脑终端执行）
另一台电脑只需要安装标准的 Python MQTT 客户端库：
```bash
pip install paho-mqtt
```

### 第二步：将 `iot_bridge.py` 放到检测系统代码同级目录下
将 `iot_bridge.py` 复制到您的检测系统工程文件夹中（例如与您的 `main.py` 或 `detect.py` 放在一起）。

### 第三步：在检测系统代码中调用（仅需 3 步）

#### 场景 1：检测到学生打架/肢体冲突时发送预警
在您的视觉识别处理循环（如 YOLO / OpenCV 检测到打架标签）中调用：

```python
from iot_bridge import ClassroomIotBridge

# 1. 在程序启动时初始化并建立连接 (建议作为全局对象)
iot = ClassroomIotBridge(
    broker="broker.emqx.io",
    port=1883,
    event_topic="classroom/grade4/events",
    status_topic="classroom/grade4/status"
)
iot.connect()

# 2. 当算法判定发生打架冲突时，调用发送：
def on_fight_detected(location_name):
    iot.send_conflict_alert(
        location=location_name,              # 冲突发生区域，如 "图书角"、"第3组后排"、"讲台旁"
        message="检测到学生推搡打架！"        # 提醒内容文本
    )

# 示例触发：
on_fight_detected("图书角")
# --> 触发后，AI-VOX 板子将在 1 秒内切换红屏、响起急促警报音、红灯高频爆闪
```

---

#### 场景 2：定期更新学生流行语热词排行榜
当您的词频统计模块或日常分析脚本统计出最新的热词排名后调用：

```python
# 当统计出最新词频榜单时调用 (最多展示前 5 项):
hot_words_data = [
    {"rank": 1, "word": "绝绝子", "count": 78},
    {"rank": 2, "word": "泰裤辣", "count": 62},
    {"rank": 3, "word": "尊嘟假嘟", "count": 45},
    {"rank": 4, "word": "遥遥领先", "count": 31},
    {"rank": 5, "word": "City不City", "count": 26}
]

iot.send_slang_ranking(
    title="四年级本周热词榜",
    items=hot_words_data
)
# --> 触发后，AI-VOX 板子将播放欢快和弦音，展示金银铜牌排位卡片，15秒后自动柔和归位
```

---

#### 场景 3：监听板子回传的“老师现场消警状态”（可选）
当老师在板子上按下 **BOOT 键** 解除警报后，电脑端可以接收到通知并自动记录日志或更新界面：

```python
def on_teacher_dismissed(data):
    print("【系统日志】现场老师已按下终端按键确认并解除了警报！", data)

# 注册回调函数
iot.on_terminal_status_callback = on_teacher_dismissed
```

---

## 📡 附录：原始 JSON 数据包格式规范 (如不使用 Python)

如果另一台电脑上的管理系统是用 **C++ / Java / C# / Node.js / Go** 等其他语言编写的，只需向主题 `classroom/grade4/events` 发布以下格式的 JSON 字符串即可：

### 1. 打架冲突预警数据包
```json
{
  "type": "alert",
  "event": "fight",
  "level": "danger",
  "location": "图书角后排",
  "message": "检测到疑似打架冲突！",
  "timestamp": 1725508800
}
```

### 2. 流行语热词排行数据包
```json
{
  "type": "ranking",
  "title": "四年级热词榜",
  "items": [
    {"rank": 1, "word": "绝绝子", "count": 68},
    {"rank": 2, "word": "泰裤辣", "count": 52},
    {"rank": 3, "word": "尊嘟假嘟", "count": 41},
    {"rank": 4, "word": "遥遥领先", "count": 28},
    {"rank": 5, "word": "City不City", "count": 23}
  ],
  "timestamp": 1725508800
}
```

### 3. 电脑端远程消警数据包
```json
{
  "type": "clear",
  "action": "dismiss",
  "timestamp": 1725508800
}
```
