# 教室学生行为管理系统 - 接入 AI-VOX 硬件终端指导手册 (方案 2 专属)

> 💡 **给另一台电脑上负责代码修改的 AI 助手的提示**：
> 请仔细阅读本指南。你正在修改当前 Python 仓库（`Leo6662233/meme`），目标是将本系统检测到的**【学生打架预警】**与**【热梗排行榜】**通过 MQTT 物联网协议实时推送到教室的 **AI-VOX 硬件终端**（ESP32-S3 全彩屏 + 扬声器）。
> 请按照下方具体的修改清单，逐步完成依赖添加、文件放置与核心代码注入。

---

## 📋 整体架构与通信参数

本系统通过 MQTT 协议向云端发布事件，硬件终端订阅同一主题实时响应：

- **MQTT Broker**: `broker.emqx.io` (公共免费服务器)
- **Port**: `1883`
- **用户名 / 密码**: *(留空)*
- **事件推送主题 (Topic)**: `classroom/grade4/events`
- **终端状态反馈主题 (Topic)**: `classroom/grade4/status`

---

## 🛠️ 具体实施步骤清单

### 步骤 1：安装依赖与放置通信类

1. **安装 MQTT 客户端库**：
   在 `requirements.txt` 末尾添加一行：
   ```text
   paho-mqtt>=1.6.1,<3.0.0
   ```
   并在终端执行安装：
   ```bash
   pip install paho-mqtt
   ```

2. **放置通信模块 `iot_bridge.py`**：
   将随本指南附带的 `iot_bridge.py` 文件放置在仓库**根目录**下（即与 `fight_engine.py` 同级）。

---

### 步骤 2：修改视觉打架检测引擎 (`fight_engine.py`)

目标：在 YOLO 关键点判定打架（`fighting=True`）的瞬间向板子发送声光警报；打架平息（`fighting=False`）后通知板子恢复正常。

#### 📝 修改说明：
打开 `fight_engine.py`：

1. **顶部引入通信类**（在 `import time` 等语句后添加）：
   ```python
   try:
       from iot_bridge import ClassroomIotBridge
   except ImportError:
       ClassroomIotBridge = None
   ```

2. **在 `FightEngine.__init__` 中初始化 IoT 客户端**：
   ```python
       def __init__(self, model_path="yolo11n-pose.pt", camera_index=0):
           self.model_path = model_path
           self.camera_index = camera_index
           self._thread = None
           self._running = False
           self._lock = threading.Lock()
           self._jpeg = None
           self.state = {"fighting": False, "persons": 0, "running": False}
           
           # === 新增：AI-VOX 物联网终端连接 ===
           self.iot = None
           self._last_fighting = False
           if ClassroomIotBridge:
               try:
                   self.iot = ClassroomIotBridge(
                       broker="broker.emqx.io",
                       port=1883,
                       event_topic="classroom/grade4/events"
                   )
                   self.iot.connect()
               except Exception as e:
                   print(f"[IoT] 连接 MQTT 终端失败: {e}")
   ```

3. **在 `FightEngine._loop` 判定打架状态处注入推送逻辑**：
   定位到判定 `fighting` 并更新 `self.state["fighting"] = fighting` 的位置（约 145~155 行），在其后添加：
   ```python
               # ---- 存最新状态 + 最新帧 ----
               self.state["fighting"] = fighting
               self.state["persons"] = len(persons)

               # === 新增：状态变化时推送至 AI-VOX 终端 ===
               if self.iot:
                   # 状态突变：由正常变为打架
                   if fighting and not self._last_fighting:
                       self.iot.client.publish(
                           "classroom/grade4/events",
                           json.dumps({
                               "fighting": True,
                               "persons": len(persons),
                               "location": "视觉监控区",
                               "message": f"检测到 {len(persons)} 人推搡打架！",
                               "timestamp": int(time.time()),
                           }, ensure_ascii=False)
                       )
                   # 状态突变：打架结束，恢复正常
                   elif not fighting and self._last_fighting:
                       self.iot.client.publish(
                           "classroom/grade4/events",
                           json.dumps({
                               "fighting": False,
                               "persons": len(persons),
                               "timestamp": int(time.time()),
                           }, ensure_ascii=False)
                       )
               self._last_fighting = fighting
   ```

---

### 步骤 3：修改 Web 仪表盘与热梗广播 (`webapp/app.py`)

目标：当录音转写完成或热梗榜单产生新数据时，将最新的前 5 项热梗排行榜推送至 AI-VOX 硬件屏幕展示。

#### 📝 修改说明：
打开 `webapp/app.py`：

1. **顶部引入与初始化 IoT 客户端**（约 20~25 行）：
   ```python
   try:
       from iot_bridge import ClassroomIotBridge
       iot_client = ClassroomIotBridge(
           broker="broker.emqx.io",
           port=1883,
           event_topic="classroom/grade4/events"
       )
       iot_client.connect()
   except Exception as e:
       print(f"[IoT] App 物联网模块初始化失败: {e}")
       iot_client = None
   ```

2. **添加热梗同步辅助函数 `broadcast_top_memes()`**（可放在 `load_meme_details()` 之后）：
   ```python
   def broadcast_top_memes():
       """将当前最新的热词排行榜推送到 AI-VOX 硬件终端"""
       if not iot_client:
           return
       try:
           memes = load_memes()
           text_total = {name: 0 for name in memes}
           voice_total = {name: 0 for name in memes}
           files = [f for f in glob.glob(os.path.join(DATA_DIR, "*.txt"))
                    if not os.path.basename(f).startswith("report_")]
           for f in files:
               with open(f, encoding="utf-8") as fp:
                   counter = count_memes(fp.read(), memes)
               for name, count in counter.items():
                   text_total[name] += count
           for f in glob.glob(os.path.join(DATA_DIR, "*.voice.json")):
               with open(f, encoding="utf-8") as fp:
                   for hit in json.load(fp).get("hits", []):
                       if hit.get("meme") in voice_total:
                           voice_total[hit["meme"]] += 1
           ranked = sorted(memes.keys(), key=lambda n: text_total[n] + voice_total[n], reverse=True)
           ranking_data = [{"name": n, "count": text_total[n] + voice_total[n]} for n in ranked[:5]]
           
           # 推送到硬件终端
           iot_client.send_slang_ranking("四年级热梗排行榜", ranking_data)
           print(f"[IoT] 已将最新前5名热梗同步至 AI-VOX 硬件终端: {ranking_data}")
       except Exception as e:
           print(f"[IoT] 推送热词排行榜异常: {e}")
   ```

3. **在转写任务完成时触发广播**：
   定位到 `run_transcribe(paths)` 函数末尾（约 145 行）：
   ```python
           with job_lock:
               job.update(status="done", message=f"处理完成，共 {len(paths)} 段")
           
           # === 新增：转写完成后触发广播 ===
           broadcast_top_memes()
   ```

4. **在手动点击/访问排行榜接口时也可选同步**：
   在 `@app.get("/api/ranking")` 的 `return jsonify(...)` 前加入：
   ```python
       # 如果有新的有效转写，则顺便同步
       if ranked and (text_total[ranked[0]] + voice_total[ranked[0]]) > 0:
           ranking_top5 = [{"name": n, "count": text_total[n] + voice_total[n]} for n in ranked[:5]]
           if iot_client:
               iot_client.send_slang_ranking("四年级热梗排行榜", ranking_top5)
   ```

---

## ✅ 验证与效果预期

完成上述修改后，在另一台电脑上启动应用：
```bash
python webapp/app.py
```

1. **测试打架预警**：
   - 打开浏览器访问 `http://127.0.0.1:5000/vision` 并点击“启动视觉识别”；
   - 两人在摄像头前做出肢体推搡冲突动作；
   - 终端现象：AI-VOX 开发板**1秒内变红、高频鸣叫警报、红灯暴闪**；
   - 两人分开后，板子**自动消除红屏，显示“一切正常（检测到 2 个人）”**。
2. **测试热梗排行榜**：
   - 打开浏览器访问 `http://127.0.0.1:5000/meme`，点击录音并说话（例如说出：“你个老六”、“牛来”、“666”）；
   - 点击停止录音，等待转写完成；
   - 终端现象：AI-VOX 开发板**响起清脆欢快的和弦提示音，全彩屏幕上以金、银、铜牌卡片形式展示热词排行榜 Top 5**。
