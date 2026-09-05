# 教室学生行为管理系统 - 针对 Leo6662233/meme 的专属接入指南

本指南专门针对 GitHub 开源检测系统 [**Leo6662233/meme**](https://github.com/Leo6662233/meme) 编写。
硬件终端固件已**深度适配该仓库的数据协议与工作逻辑**，支持以下两大特性：

1. **视觉打架检测联动**：
   - 适配 `fight_engine.py` 中的 `{"fighting": true, "persons": N}` 格式；
   - 打架发生时自动红屏声光报警；
   - 打架平息后自动消警，并在屏幕上展示 `一切正常（检测到 N 个人）`。
2. **校园热梗排行榜联动**：
   - 适配 `webapp/app.py` 中 `/api/ranking` 吐出的 `{"ranking": [{"name": "...", "count": ...}]}` 格式；
   - 直接支持该词库中的热梗（如“你个老六”、“牛来”、“666”、“鸡你太美”、“我的刀盾”等）并在屏幕排版展示。

---

## 🚀 方式一：零侵入式无缝网桥 (推荐，最简单)

如果您不想修改 `Leo6662233/meme` 现有的任何源码，只需使用我们为您编写的专用物联网网桥 [`meme_iot_bridge.py`](file:///Users/joeygu/Documents/Arduino/IOT/pc_simulator/meme_iot_bridge.py)。

### 1. 将文件复制到 `Leo6662233/meme` 目录下
将以下两个文件拷贝到另一台电脑的 `Leo6662233/meme` 根目录下：
- [`iot_bridge.py`](file:///Users/joeygu/Documents/Arduino/IOT/pc_simulator/iot_bridge.py)
- [`meme_iot_bridge.py`](file:///Users/joeygu/Documents/Arduino/IOT/pc_simulator/meme_iot_bridge.py)

### 2. 在另一台电脑上安装依赖
```bash
pip install paho-mqtt requests
```

### 3. 一起运行
1. **启动检测系统 Web 仪表盘**：
   ```bash
   cd meme/webapp
   python app.py
   ```
2. **在另一个终端窗口启动物联网网桥**：
   ```bash
   cd meme
   python meme_iot_bridge.py
   ```
- **效果**：网桥会自动监听 `http://127.0.0.1:5000`：
  - 当摄像头在网页中检测到打架时，板子**1秒内变红并响警报**；
  - 打架停止后，板子**自动恢复绿光并显示当前人数**；
  - 录音转写完成、热梗出现变动时，板子**自动响起和弦音并刷新展示最新的前 5 名热梗**！

---

## 🛠️ 方式二：直接嵌入现有代码中

如果您希望将发送逻辑直接写进 `Leo6662233/meme` 的代码里：

### 1. 接入打架预警 (`fight_engine.py`)
在 `fight_engine.py` 的 `FightEngine._loop()` 中找到判定打架后存状态的地方（约 145 行）：

```python
# 导入物联网桥接模块
from iot_bridge import ClassroomIotBridge

class FightEngine:
    def __init__(self, ...):
        ...
        self.iot = ClassroomIotBridge(broker="broker.emqx.io", event_topic="classroom/grade4/events")
        self.iot.connect()
        self._last_fighting = False

    def _loop(self):
        ...
        # 原有代码：self.state["fighting"] = fighting
        # 增加联动：
        if fighting and not self._last_fighting:
            self.iot.send_conflict_alert(
                location="摄像头监控区",
                message=f"检测到 {len(persons)} 人推搡打架！"
            )
        elif not fighting and self._last_fighting:
            self.iot.send_clear_alert()
        self._last_fighting = fighting
```

### 2. 接入热梗榜单 (`webapp/app.py`)
在 `webapp/app.py` 中的 `run_transcribe()` 处理完成或者 `/api/ranking` 被请求时，调用：

```python
from iot_bridge import ClassroomIotBridge

iot = ClassroomIotBridge(broker="broker.emqx.io", event_topic="classroom/grade4/events")
iot.connect()

# 在转写完成或需要推送到板子时调用：
def push_ranking_to_board():
    # 按照已有 ranking() 的逻辑读取榜单
    ranked_memes = [
        {"name": "你个老六", "count": 12},
        {"name": "牛来", "count": 8},
        {"name": "666", "count": 7},
        {"name": "鸡你太美", "count": 5},
        {"name": "我的刀盾", "count": 3}
    ]
    iot.send_slang_ranking("四年级热梗排行榜", ranked_memes)
```

---

## 📡 硬件终端现已支持的原生数据格式对照

固件已烧录入板，支持以下直接下发的 JSON 格式：

### 1. 视觉检测状态同步 (原生对接 `FightEngine.state`)
```json
{
  "fighting": true,
  "persons": 2,
  "message": "检测到 2 人推搡打架！"
}
```
- `fighting: true` -> 触发全屏红色警报，鸣响警笛；
- `fighting: false` -> 触发自动复位，屏幕展示 `一切正常（检测到 N 个人）`。

### 2. 热梗排行榜同步 (原生对接 `/api/ranking`)
```json
{
  "type": "ranking",
  "title": "四年级热梗排行榜",
  "ranking": [
    {"name": "你个老六", "count": 15},
    {"name": "牛来", "count": 11},
    {"name": "666", "count": 9},
    {"name": "鸡你太美", "count": 8},
    {"name": "我的刀盾", "count": 5}
  ]
}
```
- 收到后播放和弦音，按 1~5 名金银铜牌排版展示梗名称与出现次数。
