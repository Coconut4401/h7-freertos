# STM32H743_5inch_GT911_Test 项目 - FreeRTOS 实战应用笔记

> 本笔记是基于项目实际代码的通俗版讲解总结，覆盖 FILES、LOGS、DRAW、MONITOR、MUSIC 五大模块，以及 busy 标志、看门狗启动等关键问题。
> 所有内容都用生活类比+代码拆解的方式讲解，方便理解和复习。

---

## 📖 目录

1. [FILES 文件管理模块](#一files-文件管理模块)
2. [LOGS 日志系统模块](#二logs-日志系统模块)
3. [DRAW 画图模块](#三draw-画图模块)
4. [MONITOR 系统监控模块](#四monitor-系统监控模块)
5. [核心概念：busy 标志详解](#五核心概念busy-标志详解)
6. [实战问题：看门狗启动失败排查](#六实战问题看门狗启动失败排查)
7. [综合总结：FreeRTOS 设计哲学](#七综合总结freertos-设计哲学)
8. [MUSIC 音乐播放模块](#八music-音乐播放模块)

---

## 一、FILES 文件管理模块

### 🎯 核心架构：餐厅点餐模型

把整个系统想象成一个餐厅：

```
顾客（GuiTask）
  - 看菜单（FILES界面）
  - 点菜（发送请求到队列）
  - 等菜上桌（接收响应队列）
       ↓ 通过"点菜单"（请求队列）
厨师（StorageTask）
  - 独占厨房（FatFs + SD卡）
  - 按顺序做菜（串行化处理）
  - 做好后送到对应桌号（响应队列）
       ↓ 独家使用
厨房设备（FatFs → SDMMC → SD卡）
```

**核心设计原则**：这个资源，只能由那一个任务碰。从架构层消除并发，比用锁保护更优雅。

### 📦 四层洋葱模型

```
1. app_files.c (业务逻辑)      ← GuiTask运行
   "我要打开NOTE0001.TXT"
2. FreeRTOS队列 (传话筒)       ← 跨任务通信
   请求队列 → [打开NOTE0001]
   响应队列 ← [内容：Hello World]
3. app_storage.c (存储服务)    ← StorageTask运行
   "收到，我来操作SD卡"
4. FatFs → diskio → SDMMC (硬件)  ← 物理层
```

**关键设计**：GuiTask 永远不直接碰 SD 卡！

### 🎬 完整任务状态转换（打开文件示例）

**第1阶段：GuiTask发送请求**
```c
GuiTask (Running) {
    用户点击OPEN按钮
    → 准备请求：request.operation = READ
    → xQueueSend(请求队列, &request, 0);  // 超时=0，不等待
    → g_files.busy = 1;
    → 显示 "READING..."
    → 继续渲染界面
}
```

**第2阶段：StorageTask被唤醒**
```c
StorageTask (Blocked → Ready → Running) {
    xQueueReceive(请求队列, &req, 500ms);
    → 请求到达，被唤醒
    → f_open/f_read/f_close
    → xQueueSend(响应队列, &response, 100ms);
    → 继续睡眠 (Blocked)
}
```

**第3阶段：GuiTask接收响应**
```c
GuiTask (每一帧都会轮询) {
    if (xQueueReceive(响应队列, &resp, 0) == pdTRUE) {
        g_files.busy = 0;
        if (resp.success) {
            切换到内容页;
            显示文件内容;
        }
    }
}
```

### 🔑 关键设计点

**1. 队列超时参数决定任务性格**

| 写法 | 行为 | 谁用 |
|---|---|---|
| `xQueueSend(q, &d, 0)` | 不等，满了立刻返回 | GuiTask发请求 |
| `xQueueReceive(q, &d, portMAX_DELAY)` | 永远等 | StorageTask等请求 |
| `xQueueReceive(q, &d, 0)` | 不等，没有就返回 | GuiTask轮询响应 |

**2. 独立响应队列（防止响应错乱）**

```
错误设计：一个响应队列
  → 多个任务的响应混在一起，谁也不知道是给谁的

正确设计：每个模块独立响应队列
  FILES响应队列、DRAW响应队列、AUDIO响应队列...
  → 各自取各自的
```

**3. StorageTask优先级4的合理性**

- 99%时间在 xQueueReceive 睡眠，不占CPU
- 一有请求立即处理（用户不想等太久）
- 处理完又睡眠，不霸占CPU
- **优先级不是重要性，而是"响应延迟敏感度 × 执行时间短"**

### 🚨 隐患

1. **SDMMC关闭总中断** → 关中断期间SysTick、触摸、音频DMA全被屏蔽
2. **超时路径未恢复中断** → 一旦超时系统永久冻结
3. **请求队列容量太小（4）** → 突发请求容易丢失

---

## 二、LOGS 日志系统模块

### 🎯 核心理念：三种日志，三种命运

**日志就像三种不同的记录本**：

```
1. app_logs（结构化日志）= 便利贴墙
   贴在墙上（RAM），最多64张，满了从最旧的开始撕
   可以随时看，也可以拍照存档（导出SYSTEM.LOG）

2. printf（串口调试）= 对着窗外喊话
   喊完就没了，只有开发者拿着接收器（串口）能听到

3. app_fault（崩溃日志）= 黑匣子
   飞机坠毁前的最后录音（Backup SRAM）
   重启后取出来，写成CRASH.LOG
```

**关键区别**：

| 特性 | app_logs | printf | app_fault |
|---|---|---|---|
| 存储位置 | RAM（掉电丢失） | 串口发送即消失 | Backup SRAM（复位保留） |
| 触发时机 | 正常运行 | 开发调试 | 系统崩溃 |
| 用户可见 | LOGS页面 | 需串口工具 | CRASH.LOG文件 |
| 持久化 | 手动导出 | 无 | 自动 |

### 📦 日志数据结构

```c
typedef struct {
    uint32_t uptime_seconds;   // 4字节：开机后多少秒
    app_log_level_t level;     // 4字节：INFO/WARN/ERROR
    char module[9];            // 9字节：谁写的
    char message[49];          // 49字节：写了什么
} app_log_entry_t;             // 总共约64字节
```

**64条 × 64字节 = 4 KiB**，静态分配，永不malloc。

### 🔄 环形缓冲区（Ring Buffer）

想象一个**旋转的转盘**，有64个格子：

```
写第1条：entries[0]，next=1
写第64条：entries[63]，next=0
写第65条：覆盖 entries[0]，next=1（覆盖最旧）
```

**为什么用环形而不是队列？**
- 最新日志最重要（老的可以丢）
- 内存必须有上限（否则会耗尽）
- 环形缓冲 + 覆盖策略 = 完美匹配

### 🔒 临界区保护（Critical Section）

**问题场景**：多任务同时写日志

```
时间线（没有保护）：
GuiTask读next=5，准备写...
【AudioTask抢占】读next=5，写entries[5]="WAV START"，next=6
【切换回】GuiTask继续，写entries[5]="USER LOGIN"（覆盖！）
结果：AudioTask的日志丢失
```

**解决方案**：

```c
void app_logs_add(...) {
    taskENTER_CRITICAL();  // 🔒 上锁
    {
        写 entries[next]
        next = (next + 1) % 64
        if (count < 64) count++
        generation++
    }
    taskEXIT_CRITICAL();   // 🔓 解锁
}
```

**为什么用临界区而不是互斥量？**

| 方式 | 开销 | 适合的操作时长 |
|---|---|---|
| taskENTER_CRITICAL() | 极小（μs级） | 几十字节内存拷贝 ✅ |
| xSemaphoreTake (Mutex) | 中等 | 毫秒级操作（如SD卡） |
| xQueueSend | 中等 | 需要传数据 |

**黄金法则**：
- 临界区极短（几微秒） → taskENTER_CRITICAL
- 临界区较长（毫秒级） → 互斥量
- 临界区超长（几十毫秒） → 串行化任务

### 📸 快照机制（Snapshot）

**问题**：UI渲染时数据在变化，会出现"半旧半新"撕裂。

**解决方案**：先拷贝，再绘制

```c
app_logs_take_snapshot() {
    taskENTER_CRITICAL();
    for (i=0; i<count; i++) {
        snapshot[i] = entries[...];  // 复制到线性数组
    }
    taskEXIT_CRITICAL();
}
```

**generation版本号**：智能刷新
- 每次app_logs_add() → generation++
- GuiTask检查版本号变化，才重新拍快照
- 避免每帧都重新拷贝4KB

### 💾 SD卡导出（复用StorageTask）

```
用户点EXPORT
    ↓ InputTask生成DOWN事件
    ↓ TouchEvents队列 → GuiTask
GuiTask调用 app_logs_export() {
    1. 拍快照（临界区）
    2. 格式化到6144字节静态缓冲
    3. 打包成APP_STORAGE_OP_WRITE_LOG请求
    4. busy=1，按钮变灰
}
    ↓ StorageRequests队列 → StorageTask
StorageTask执行事务写入 {
    写 ~LOG.TMP → f_sync()
    → SYSTEM.LOG → ~LOG.BAK
    → ~LOG.TMP → SYSTEM.LOG
    → 删除 ~LOG.BAK
}
    ↓ StorageLogResponses队列 → GuiTask
GuiTask处理响应 {
    busy=0
    添加日志 "SYSTEM.LOG EXPORTED"
    刷新页面
}
```

### 🚨 故障日志的特殊路径

**崩溃时不能调用 app_logs_add()**，因为：
1. xTaskGetTickCount() 在HardFault上下文行为未定义
2. taskENTER_CRITICAL() 在异常上下文不安全
3. FreeRTOS内核可能已损坏

**解决方案：Backup SRAM + 重启**

```
崩溃时 → 写Backup SRAM（VBAT保留）
系统重启 → GuiTask读Backup SRAM
       → 添加日志 "RESET TYPE=HARDFAULT"
       → 提交APP_STORAGE_OP_WRITE_CRASH
       → 生成CRASH.LOG
       → 清除Backup SRAM
```

### 🎓 三种并发保护的选择哲学

| 数据操作耗时 | 保护方式 | 本项目例子 |
|---|---|---|
| 微秒级（几十字节内存拷贝） | taskENTER_CRITICAL | app_logs写入 |
| 毫秒级（SD卡读写） | 串行化任务 + 队列 | StorageTask |
| 异常级（HardFault） | Backup SRAM | app_fault |

---

## 三、DRAW 画图模块

### 🎯 核心理念：保存的不是"图片"，而是"动作"

**用视频类比**：

```
笨办法（保存图片）：
  把整个屏幕拍照 → 800×480个像素点全存下来
  = 750 KB的文件

聪明办法（本项目用的）：
  记录你"是怎么画的"：
    第1步：在(100,100)按下笔，用黑色
    第2步：滑到(110,105)
    ...
  = 只需要8 KB
  = 就像存一段"如何画花"的教学视频
```

**打开文件时**：不是显示一张图片，而是**"照着当初的动作再画一遍"**（叫做"重放"）。

**好处**：文件小94倍、保存快、支持缩放和撤销。

### 📦 "点"的数据结构

```c
struct point {
    uint16_t x;         // 2字节：X坐标
    uint16_t y;         // 2字节：Y坐标
    uint16_t color;     // 2字节：RGB565颜色
    uint8_t  start;     // 1字节：是不是新笔画的起点？
    uint8_t  reserved;  // 1字节：预留（对齐）
};                      // 总共8字节
```

**start标志的巧妙用法**：

```
点1: (100,100) 黑色 🚩新笔画开始
点2: (110,105) 黑色    连着上一个点
点3: (120,110) 黑色    连着上一个点
点4: (200,200) 红色 🚩新笔画开始  ← 手指抬过、又按下
点5: (210,205) 红色    连着上一个点
```

**重放时的逻辑**：
```c
for (i = 0; i < point_count; i++) {
    if (points[i].start == 1) {
        画一个圆点(points[i]);  // 新笔画只画点
    } else {
        画线(points[i-1] → points[i]);  // 连接上一个点
    }
}
```

### 🎬 完整数据流（8层瀑布）

```
用户手指触摸屏幕
    ↓ ①硬件层 GT911触摸芯片检测到接触
    ↓ ②中断/轮询
InputTask每5ms扫描一次（优先级4）
    ↓ ③事件生成
生成 DOWN / MOVE / UP 事件
    ↓ ④队列传递
TouchEvents队列（容量32）
    ↓ ⑤消费者唤醒
GuiTask（优先级3）被唤醒
    ↓ ⑥业务逻辑
app_draw_handle_event()判断
    ↓ ⑦记录+渲染
append_point() 记录到内存
draw_stroke() 画到屏幕
    ↓ ⑧硬件呈现
写入SDRAM帧缓冲 @ 0xC0000000
LTDC控制器持续扫描 → LCD显示
```

**为什么InputTask优先级要高（4）？**
- 触摸采样有严格的5ms周期要求
- 错过一次采样 → 丢失手指位置 → 画出断裂的线

**为什么GuiTask不能设更高？**
- GuiTask渲染一帧可能要20~50ms
- 如果GuiTask阻塞InputTask，触摸就完全失灵

### 🖱️ 触摸和鼠标的统一

```
GT911触摸 ────┐
              ├──→ InputTask ──→ 统一事件 ──→ GuiTask
CH9350鼠标 ───┘   (DOWN/MOVE/UP + 坐标)
```

**GuiTask根本不知道输入来自哪里**！这就是"输入抽象层"。

**鼠标合并优化**：5ms内多次MOVE报告 → 只保留最新位置。

**灵敏度阈值**：
```
LOW:    移动>=6像素才发MOVE
NORMAL: 移动>=3像素才发MOVE
HIGH:   移动>=1像素才发MOVE
```
避免手指静止时的抖动噪声。

**队列压力保护（背压）**：
```c
if (TouchEvents队列剩余空间 < 4) {
    ❌ 不再产生MOVE事件
    ✅ 但保留 UP / BACK / 连接状态 事件
}
```

优先级：**关键状态事件 > 位置更新事件**
- UP丢了 = 灾难（系统状态错乱）
- MOVE丢了 = 轻伤（线条稀疏一点）

### 🎨 为什么线条又粗又圆滑？

**技巧**：一像素基础线 + 沿线密集画圆点

```c
void draw_stroke(x1, y1, x2, y2, color) {
    lcd_draw_line(x1, y1, x2, y2, color);  // 基础线
    int steps = distance(x1, y1, x2, y2) / 2;
    for (i = 0; i <= steps; i++) {
        int cx = x1 + (x2-x1) * i / steps;
        int cy = y1 + (y2-y1) * i / steps;
        lcd_fill_circle(cx, cy, 2, color);
    }
}
```

**效果**：从1像素细线变成5像素粗、边缘圆润的笔画。

### 🔒 DRAW完全没有锁！

```
谁访问 g_draw？
- 处理触摸事件时：GuiTask
- 渲染画布时：GuiTask
- 保存文件时：GuiTask
- 收到响应时：GuiTask

答案：全都是GuiTask！
```

**单一线程访问，天然无竞争** → 不需要互斥量、临界区、任何同步原语。

### 📤 StorageTask怎么读画图数据？

**独立的I/O缓冲区 + 快照复制**：

```c
static uint8_t g_io_document[8208];  // 静态缓冲

void app_draw_save() {
    g_draw.busy = 1;              // 🔒 软锁
    g_draw.drawing = 0;           // 停止画线
    // 拷贝到独立缓冲（快照）
    memcpy(g_io_document, g_draw.points, ...);
    // 发送请求（只传指针）
    request.binary_data = g_io_document;
    xQueueSend(StorageRequests, &request, 0);
}
```

**为什么用静态g_io_document而不是栈变量？**

```c
❌ 栈变量：
   uint8_t buffer[8208];  // 8KB栈空间！GuiTask栈才4KB → 栈溢出
   函数返回后buffer被销毁 → StorageTask拿到悬空指针

✅ 静态变量：
   static uint8_t g_io_document[8208];  // 放在.bss段
   永远不销毁，永远有效
   代价：永久占用8KB RAM
```

**结合笔记模块6**：大缓冲区永远不放栈上，放静态区或堆。

### 🛡️ DRW1文件格式与验证

**文件格式**：
```
偏移0:  "DRW1"      (4字节魔数)
偏移4:  version=1   (1字节)
偏移5:  point_size=8 (1字节)
偏移6:  point_count (2字节)
偏移8:  reserved   (4字节)
偏移12: payload_crc (4字节)
偏移16: 点数据      (point_count × 8字节)
```

**打开时的11道验证**：
```
① 文件长度 >= 16
② 魔数 == "DRW1"
③ version == 1
④ point_size == 8
⑤ point_count <= 1024
⑥ 实际长度 == 16+count*8
⑦ CRC32 匹配
⑧ 所有点在画布内
⑨ 颜色合法
⑩ start只能是0或1
⑪ 首点必须start=1
```

**这就是"防御式编程"**：假设外部数据全是坏的，逐层筛选。

### 🚨 隐患

1. **跨页面重入竞态** → busy清零后旧请求仍在使用g_io_document
2. **响应丢失导致永久busy** → 需要超时机制
3. **1024个点上限** → 快速涂鸦几秒钟就能耗尽

---

## 四、MONITOR 系统监控模块

### 🏥 类比：医院体检中心

```
工厂里的工人：
- InputTask（门卫）         = 看门、收快递
- GuiTask（前台接待）       = 接待客户、显示信息
- StorageTask（仓库管理员）  = 搬货、记账
- AudioTask（广播员）        = 放音乐
- MonitorTask（体检医生）    = 👈 就是这个！
```

**MONITOR就是工厂的"体检医生"**：
- 每隔500毫秒巡视一圈
- 问每个工人："最近累不累？有没有丢东西？"
- 记录下来，做成"体检报告"
- 贴在墙上（LCD）或对讲机广播（串口）

**它不干活，只负责看别人干活的情况**。

### 📋 六大类"体检项目"

1. **系统运行时间和内存健康**：uptime、当前堆、历史最低堆
2. **输入系统**：事件总数、丢弃数、队列深度
3. **鼠标状态（CH9350）**：在线状态、报告数、错误数
4. **各任务栈余量**：InputTask/GuiTask/StorageTask等
5. **任务健康状态**：哪些任务还活着、看门狗是否启动
6. **存储系统**：队列深度、FS错误、响应错误

### 🚶 每500毫秒的巡视流程

```
MonitorTask醒来（每500ms）
    ↓
① app_health_beat() 报告心跳
② LED1_TOGGLE() 硬件心跳指示
③ 采集数据：
   ├─ 问InputTask：丢了多少事件？队列多长？
   ├─ 问CH9350：鼠标在线吗？错误多少？
   ├─ 问HealthTask：各任务栈剩多少？
   ├─ 问StorageTask：队列排队吗？
   └─ 问系统：堆还剩多少？
④ 打包成snapshot
⑤ 临界区保护写入 g_monitor_snapshot
⑥ 严重问题写日志
⑦ 串口printf输出（如开启）
⑧ vTaskDelayUntil 睡500ms
```

### 🎯 为什么MonitorTask优先级最低（1）？

```
优先级排序：
6 = HealthTask     ← 必须及时喂狗
5 = AudioTask      ← 音乐不能断
4 = InputTask      ← 触摸不能丢
4 = StorageTask    ← SD卡操作
3 = GuiTask        ← UI渲染
2 = TimerTask
1 = MonitorTask    ← 最低
```

**因为他不干正事！** 体检晚几秒钟没关系，重要的是别打断正在干活的人。

### 💾 关键数据详解

**1. 堆（Heap）监控**
```
HEAP NOW / MIN = 92500 / 78120
```
- NOW = 当前剩余
- MIN = 历史最低（更重要！暴露曾经的内存峰值）

**为什么MIN更重要？**
- 平时剩90KB → 你以为一切正常
- 但历史最低78KB → 说明曾经差点耗尽 → 内存泄漏风险

**2. 栈（Stack）监控**
```
STACK WORDS I/G/S/A/M = 380 / 412 / 295 / 510 / 401
```
- 单位是words（1 word = 4字节）
- 警戒线：<128 words 红色警告
- 剩余最少的任务最危险

**3. 输入丢弃统计**
```
DROP TOTAL / CRITICAL = 12 / 0
```
- TOTAL = 全部丢弃（12个）
- CRITICAL = 关键事件丢弃（0个）← 很好！
- 颜色规则：全0绿色，CRITICAL>0红色，其他黄色

**4. 队列深度**
```
INPUT QUEUE NOW / PEAK = 0 / 12
```
- NOW = 当前排队
- PEAK = 历史峰值
- PEAK接近容量（32）说明系统曾经很繁忙

**5. 健康掩码**
```
HEALTH 31/0
```
- 31 = 0b11111 = 5个任务全部健康
- 0 = 没有任务超时
- 如果GuiTask挂了：HEALTH 29/2

### 💓 任务心跳机制

```c
// 每个任务的主循环
while(1) {
    app_health_beat(APP_HEALTH_GUI);  // 报告心跳
    处理事件;
    渲染界面;
    vTaskDelay(20);
}
```

**如果忘了心跳**：
- HealthTask等1.5秒还没收到心跳
- 判断"GuiTask超时了！"
- 停止喂看门狗
- 看门狗复位系统

**各任务超时阈值**：

| 任务 | 超时时间 |
|---|---|
| InputTask | 1500 ms |
| GuiTask | 1500 ms |
| StorageTask | 10000 ms |
| AudioTask | 5000 ms |
| MonitorTask | 2000 ms |

### 🐕 看门狗（Watchdog）

**类比：监工带着秒表**
```
老板雇了监工："每5秒喊我一次'老板好'，
              超过5秒不喊，我就重启所有人"
HealthTask每250ms喂一次狗
如果任务挂了 → HealthTask不喂 → 5秒后自动复位
```

**启动条件**：所有任务首次全部健康后才启动IWDG。

### 🔒 快照发布模式

```c
// MonitorTask写入
taskENTER_CRITICAL();  // 🔒 关中断
g_monitor_snapshot = snapshot;  // 复制整个结构体
taskEXIT_CRITICAL();

// GuiTask读取
taskENTER_CRITICAL();
my_copy = g_monitor_snapshot;
taskEXIT_CRITICAL();
```

**为什么要锁？** 防止读到"半新半旧"的数据。
**临界区必须短**：只做结构体复制，不做LCD、printf等慢操作。

### 💡 LED硬件心跳

```c
LED1_TOGGLE();  // 每500ms翻转
```

**价值**：
- LCD卡住时还能看LED判断MonitorTask是否活着
- 不接屏幕时也能观察系统状态
- 软件监控可能失效，LED是最后防线

---

## 五、核心概念：busy 标志详解

### 🎯 一句话理解busy

> **busy 就是一个"正在办事中"的标志位，用来告诉自己"上一个请求还没完成，别再发新的了"。**

它本质上是一个 uint8_t 变量（0或1），承担着**流量控制**和**状态同步**的重要职责。

### 🍔 生活场景类比：餐厅点菜

```
你（GuiTask）           服务员（队列）        厨师（StorageTask）
─────────                ────────           ──────────────
1. 点了一份牛排 ──→ 传单到厨房 ──→ 开始做菜（10分钟）
   busy = 1
2. 想再点一份鸡肉？
   看看busy = 1
   → "算了，先等牛排上来"
   → 显示"请等待"
3. 十分钟后，牛排上桌
   busy = 0
4. 现在可以点鸡肉了
```

### 💻 代码中的完整生命周期

**三步曲**：

```c
// 1. 发请求前检查
if (g_files.busy) {
    显示 "STORAGE BUSY";
    return;
}

// 2. 发送成功后设置
if (xQueueSend(StorageRequests, &request, 0) == pdPASS) {
    g_files.busy = 1;
    显示 "READING...";
}

// 3. 收到响应后清除
if (xQueueReceive(StorageResponses, &resp, 0) == pdTRUE) {
    g_files.busy = 0;
    if (resp.success) {
        显示内容;
    }
}
```

### 🎯 busy的四大作用

**1. 防止请求泛滥（流量控制）**

```
❌ 没有busy：用户狂点OPEN
   → 每次点击都发请求
   → 队列爆满，请求丢失

✅ 有busy：
   → 只有1个请求在处理，秩序井然
```

**2. 简化响应匹配（避免请求ID）**

```
❌ 允许并发：需要给每个请求ID，响应匹配ID
✅ 单飞模式：同一时刻只有1个请求，响应一定是给它的
```

**3. UI状态提示（用户体验）**

```c
if (g_files.busy) {
    绘制灰色按钮（禁用）;
} else {
    绘制正常颜色（可点击）;
}
```

**4. 状态机保护**

```c
// 正在保存时不允许翻页
void handle_page_change() {
    if (g_files.busy) return;
    翻到下一页;
}
```

### 🔒 busy vs 互斥量（Mutex）的区别

| 特性 | busy变量 | Mutex互斥量 |
|---|---|---|
| 本质 | 普通变量 | FreeRTOS内核对象 |
| 检查方式 | `if (busy)` | `xSemaphoreTake()` |
| 检查失败 | 立即返回 | 立即返回/阻塞等待 |
| 谁访问 | 只有GuiTask | 多个任务 |
| 保护对象 | 逻辑流程 | 共享数据 |
| 开销 | 极小 | 较大（内核调用） |
| 引起任务切换 | ❌ | ✅ 可能 |

**FILES为什么用busy而不是Mutex？**

```
关键点：g_files整个状态只被GuiTask一个任务访问！
不存在多任务竞争 → 不需要Mutex
busy只是GuiTask自己给自己的"状态提示"
```

### 🚨 busy的陷阱

**陷阱1：响应丢失导致永久busy**
```
StorageTask发送响应超时（100ms）→ 响应被丢弃
GuiTask的busy=1永远无法清除
按钮永远灰色
```
**修复**：加超时机制
```c
if (busy && (now - request_time > 5000)) {
    busy = 0;
    显示 "TIMEOUT";
}
```

**陷阱2：跨页面重入**
```
在DRAW页面点SAVE，busy=1
用户切换到桌面 → app_draw_close() → busy=0
用户进入FILES → 发新请求
但StorageTask还在处理旧的DRAW请求
```
**修复**：请求ID + 响应匹配

### 💡 行业名称

- **Single-Flight Pattern（单飞模式）** - Go语言singleflight包
- **Semaphore of 1** - 二值信号量的普通变量实现
- **Guarded Suspension** - GoF设计模式
- **Debounce（防抖）** - 前端界面开发常用

### 🎯 一句话记忆

> **busy = "我这边有事在忙，你先等等" 的自我提醒。**

FILES、DRAW、SETTINGS、LOGS、AUDIO都有各自的busy标志，是异步存储架构的**核心润滑剂**。

---

## 六、实战问题：看门狗启动失败排查

### 🔍 问题现象

MONITOR界面显示：`WDG OFF`

**ST-Link调试数据**：
```
✅ HEALTH = 31/0（所有任务健康）
✅ RCC_CSR = 0x0E000003（LSI时钟就绪）
❌ IWDG SR = 0x03（PVU和RVU一直挂起）
❌ g_watchdog_enabled = 0
```

### 🎯 问题根源

**原始时序流程（有问题）**：
```
HealthTask检测到所有任务健康
    ↓
① IWDG1->KR = 0x5555U;  // 解锁
② IWDG1->PR = 6U;
③ IWDG1->RLR = 4095U;
    ↓
④ app_health_wait_bits(...)  // 等待PVU/RVU清零
   问题：100ms超时到期，寄存器还没同步完成 ⚠️
   → 函数返回失败
    ↓
⑤ IWDG1->KR = 0xCCCCU;  // 👈 这行永远没执行！
    ↓
结果：看门狗OFF
```

### 🔍 为什么100ms不够？

**关键问题：LSI时钟同步慢**

```
IWDG的寄存器在两个时钟域：
- ARM侧（AHB总线，480 MHz）
- LSI侧（32 kHz左右）

理论上：
  32 kHz → 6个周期 = 187 μs

实际上：
  STM32H7的同步机制更复杂
  LSI精度差（17~47 kHz范围）
  硬件同步延迟不确定
  100ms 不够！
```

### ✅ 修复方案

**修改内容**：
1. IWDG初始化顺序改为：**启动 → 开放写入 → 配置PR/RLR → 等待同步 → 喂狗**
2. LSI就绪超时保持100ms
3. IWDG参数同步超时**调整为7秒**
4. 二进制反汇编确认寄存器写入顺序正确

**新的时序流程**：
```
HealthTask检测到所有任务健康
    ↓
① IWDG1->KR = 0xCCCCU;  // 先启动（用默认配置）
② IWDG1->KR = 0x5555U;  // 解锁
③ IWDG1->PR = 6U;
④ IWDG1->RLR = 4095U;
    ↓
⑤ app_health_wait_bits(..., 7000ms)  // 等7秒
   → LSI慢慢同步完成
   → PVU/RVU清零
    ↓
⑥ HAL_IWDG_Refresh();  // 第一次喂狗
    ↓
结果：看门狗ON ✅
```

### 🤔 为什么新顺序是"先启动再配置"？

**IWDG硬件机制**：
```
根据STM32H7参考手册：
  "写入0xCCCCU后，IWDG立即启动，
   使用默认或之前的PR/RLR配置开始倒计时"
关键点：启动后仍可修改PR/RLR
      修改会在下次喂狗后生效
```

**新顺序的优势**：
```
第1步：写0xCCCCU → IWDG用默认值启动
第2步：修改PR=6, RLR=4095 → 配置排队
第3步：等待同步（7秒） → PVU/RVU清零
第4步：喂狗 → 使用新配置

即使同步慢，看门狗也已经启动了（只是用旧配置）
第一次喂狗后就切换到新配置
```

### 🔬 为什么7秒是安全的？

```
极限计算：
  LSI最慢：17 kHz
  同步周期：6个LSI周期
  理论时间：6 / 17000 = 0.35 ms
  即使硬件延迟1000倍：350 ms
  7秒 >> 350 ms → 绝对够用
```

### 🎓 你可以学到的经验

1. **时钟域同步问题**
   ```
   不同时钟域的寄存器写入不是瞬间生效的
   需要等待同步标志清零
   超时时间要留足够余量
   ```

2. **硬件启动顺序**
   ```
   有时候"先启动再配置"比"先配置再启动"更可靠
   因为至少保证了"启动"这一步完成
   ```

3. **编译器优化可能打乱关键顺序**
   ```
   对寄存器操作顺序敏感的代码
   需要检查反汇编
   必要时加内存屏障 __DSB()
   ```

4. **超时时间设计哲学**
   ```
   100ms → 根据理论计算的"应该够"
   7秒 → 根据最坏情况的"绝对够"
   关键系统初始化：选择后者
   高频操作（喂狗）：可以用前者
   ```

### 🎯 一句话总结

> **原问题：IWDG寄存器从ARM时钟域同步到LSI时钟域的时间超过100ms，导致启动命令没有执行。**
> **解决方案：增加同步等待时间到7秒，并调整启动顺序为"先启动再配置"，确保看门狗一定能启动。**

这是典型的**"理论正确但工程不够"**的案例——理论上100ms够了，但实际硬件有各种延迟和不确定性，需要留更大余量。

---

## 七、综合总结：FreeRTOS 设计哲学

### 🎯 核心设计原则

**1. 串行化任务是嵌入式RTOS的黄金模式**

```
单一所有权 > 到处加锁
StorageTask独占FatFs
GuiTask独占g_draw、g_files、g_logs
从架构层消除并发 > 用锁保护
```

**2. 按耗时选择并发保护机制**

| 数据操作耗时 | 保护方式 | 例子 |
|---|---|---|
| 微秒级 | taskENTER_CRITICAL | app_logs写入 |
| 毫秒级 | 串行化任务+队列 | StorageTask |
| 异常级 | Backup SRAM | app_fault |

**3. 优先级设计原则**

> **优先级不是重要性，而是"响应延迟敏感度 × 执行时间短"**

```
经常睡眠的任务给高优先级是安全的：
- InputTask(4)：每5ms只跑1ms
- StorageTask(4)：99%时间在睡眠
- HealthTask(6)：每250ms只跑几ms

一直运行的任务给低优先级：
- MonitorTask(1)：不能打断业务
```

**4. 队列超时参数决定任务性格**

| 场景 | 用什么超时 |
|---|---|
| GUI发请求 | 0（不能卡） |
| 服务任务等请求 | portMAX_DELAY（睡眠） |
| GUI轮询响应 | 0（轮询） |
| ISR发送 | 0（严禁阻塞） |

**5. 静态分配 > 动态分配**

```
✅ 所有大缓冲区都是静态数组：
   g_io_document[8208]
   g_logs_snapshot[4096]
   g_monitor_snapshot

✅ 优势：
   - 确定性内存布局
   - 编译期检查
   - 永不malloc失败
   - 无碎片化
```

### 🏗️ 项目架构速览

```
InputTask(优先级4) ──[TouchEvents队列]──▶ GuiTask(优先级3)
                                              │
                                              ├──[StorageRequests]──▶ StorageTask(优先级4) ──▶ SD卡
                                              │◀─[StorageResponses]──┘
                                              │
                                              └──[AudioCommands]──▶ AudioTask(优先级5) ──▶ DMA──▶ 喇叭
                                                                        ▲
                                                          [任务通知: DMA中断]
MonitorTask(优先级1) ── vTaskDelayUntil(500ms) ── 采样heap/栈水印/LED
HealthTask(优先级6) ── vTaskDelay(250ms) ── 心跳检查/喂狗
```

### 💡 五大模块对比表

| 模块 | 核心数据 | 保护机制 | 与Storage交互 |
|---|---|---|---|
| **FILES** | g_files[24] | busy标志 | 每次操作都通过队列 |
| **LOGS** | g_log_repository环形缓冲 | 临界区 | 手动导出SYSTEM.LOG |
| **DRAW** | g_draw.points[1024] | 无锁（单任务） | busy+g_io_document |
| **MONITOR** | g_monitor_snapshot | 临界区（快照发布） | 只读取StorageStats |
| **AUDIO** | 音频DMA缓冲 | 双缓冲乒乓 | 连续读取WAV |

### 🎓 三种存储访问架构对比

**方案A：裸机轮询（最原始）**
```
所有事情排队做，响应性差
```

**方案B：多任务 + 互斥量（常见但不优雅）**
```
GuiTask直接调f_open，等Mutex
问题：UI被阻塞在慢速IO上，界面卡顿
```

**方案C：串行化任务（本项目，最优雅）** ✅
```
GuiTask只花几微秒发请求
StorageTask专职做慢操作
优势：
1. 调用方不阻塞
2. 请求可缓冲
3. FatFs天然安全
4. 错误处理集中
```

### 🚨 项目当前的隐患总结

**🔴 高优先级修复**：
1. SDMMC改用DMA传输（消除关中断隐患）
2. 修复超时路径未恢复中断的bug
3. 添加请求ID防止跨页面重入

**🟡 中优先级改进**：
1. 请求队列容量 4→16
2. busy超时自动解除
3. 增加队列水位监控
4. 缩短快照临界区

**🟢 低优先级优化**：
1. 撤销功能（Undo栈）
2. 增加脏矩形渲染
3. 接入RTC真实时间
4. 双缓冲消除画面撕裂

### 📚 学过的所有知识串起来

| 学过的概念 | 在哪些模块体现 |
|---|---|
| **优先级抢占** | InputTask抢占GuiTask、StorageTask抢占GuiTask |
| **任务状态转换** | StorageTask的Blocked↔Ready↔Running循环 |
| **队列通信** | TouchEvents、StorageRequests、各Response队列 |
| **临界区** | app_logs写入、Monitor快照发布 |
| **互斥量** | 项目实际未使用（架构层避免） |
| **任务通知** | DMA中断→AudioTask（未在本笔记详述） |
| **软件定时器** | 项目未使用 |
| **事件组** | 项目未使用 |
| **栈管理** | Monitor监控栈水印，警戒线128 words |
| **堆管理** | Monitor监控当前堆和历史最低 |
| **静态vs动态** | 所有大缓冲静态分配 |
| **看门狗** | HealthTask启动IWDG，任务心跳保护 |

### 🎯 十条黄金法则（收官总结）

1. **单一所有权** > 加锁保护
2. **任务优先级** = 响应延迟敏感度 × 执行时间短
3. **临界区必须短**（微秒级）
4. **静态分配**保证嵌入式确定性
5. **异步IO**让慢操作不影响UI
6. **事务写入**保护关键文件（TMP→BAK→正式）
7. **不信任外部数据**（多层校验）
8. **快照发布**避免读到半新半旧数据
9. **背压保护**优先保关键事件
10. **超时兜底**防止永久卡死

---

## 📝 附录：项目文件速查

### 核心业务模块
- `App/app_runtime.c` - 应用切换、事件分发
- `App/app_files.c` - 文件管理业务
- `App/app_logs.c` - 结构化日志
- `App/app_draw.c` - 画图业务
- `App/app_monitor.c` - 系统监控
- `App/app_audio.c` - 音频播放

### 基础服务
- `App/app_input.c` - 输入采集（触摸+鼠标）
- `App/app_storage.c` - 存储服务（唯一FatFs访问）
- `App/app_health.c` - 健康检查+看门狗
- `App/app_fault.c` - 崩溃日志
- `App/app_ui.c` - LCD界面绘制

### 硬件驱动
- `Drivers/BSP/LCD/lcd.c` - LCD底层
- `Drivers/BSP/LCD/ltdc.c` - LTDC控制器
- `Drivers/BSP/SDMMC/sdmmc_sdcard.c` - SD卡驱动
- `Middlewares/FatFs/ff.c` - FAT32文件系统

### 配置文件
- `User/FreeRTOSConfig.h` - FreeRTOS配置
- `Middlewares/FatFs/ffconf.h` - FatFs配置
- `User/main.c` - 任务创建

---

**笔记结束 🎓**

本笔记覆盖了 STM32H743_5inch_GT911_Test 项目中 FreeRTOS 应用的核心模块和关键问题。
建议配合 `FreeRTOS学习笔记.md` 的理论基础一起复习。

生成日期：2026-09-07

---

## 八、MUSIC 音乐播放模块

### 🎵 核心难点：让慢速SD卡喂饱高速音频

**类比：工厂的流水线**

```
播放一个WAV需要每秒钟精准送出96000个数字（48kHz双声道）
- 一个都不能少（否则卡顿/静音）
- 一个都不能晚（否则声音变形）

但SD卡读取要几十毫秒才能读一次
每21微秒需要一个样本
怎么让慢速SD卡喂饱高速音频？
```

**答案**：DMA + 双缓冲 + 独立任务

### 📦 整体架构：五层瀑布

```
用户点PLAY按钮
    ↓
① app_music（前台服务员）
   记录你想干嘛：播放/暂停/切歌/调音量
    ↓ 通过 AudioCommands 队列
② app_audio（后厨大厨）
   管理播放状态、控制DMA、转换音频格式
    ↓ 通过 StorageRequests 队列
③ StorageTask（仓库管理员）
   独占SD卡，读取WAV文件
    ↓ 返回音频数据
④ DMA硬件（自动传送带）
   不用CPU参与，自动把数据送给I2S
    ↓
⑤ MAX98357A功放芯片 → 🔊 喇叭
```

### 🎯 音频状态机（7种状态）

| 状态 | 含义 |
|---|---|
| STARTING | 刚启动，正在扫描SD卡 |
| STOPPED | 有歌但没在播放 |
| PLAYING | 正在播放 |
| PAUSED | 播放中被暂停 |
| TEST_TONE | 正在放测试音（440Hz） |
| NO_TRACKS | SD卡里没WAV文件 |
| ERROR | 出错了 |

### 🔄 DMA双缓冲：本模块最核心的技巧

**类比：接力赛跑**

```
❌ 一个人跑全程（CPU边读SD卡边给音频）：
   跑一圈就累趴下，中间要喘气
   → 音频出现空档 → 卡顿爆音

✅ 两个人接力（DMA双缓冲）：
   A跑步的时候，B在准备
   A跑完交棒给B，B跑的时候A休息
   → 无缝衔接，永远有人在跑
```

**内存开一个大缓冲区，分成两半**：

```c
int16_t g_audio_dma_buffer[65504];  // 约128KB

┌───────────────────┬───────────────────┐
│    Half 0（前半） │    Half 1（后半） │
│  索引 0~32751     │  索引 32752~65503 │
└───────────────────┴───────────────────┘

48kHz双声道时：每半区容纳340ms音频
```

**循环播放机制**：
```
初始：Half0填满，Half1填满，DMA开始播Half0
时刻A：DMA播完Half0 → 硬件触发"半传输中断"
       → 通知AudioTask："快填Half0！"
       → DMA继续播Half1（无缝衔接）
时刻B：AudioTask从SD卡读新数据填Half0
时刻C：DMA播完Half1 → 触发"全传输中断"
       → 通知AudioTask："快填Half1！"
       → DMA回到Half0继续（循环模式）
...循环...
```

**这就是"乒乓缓冲"（Ping-Pong Buffer）**，音频/视频流播放的经典架构。

### 🚚 DMA是什么？为啥这么重要？

**类比：自动传送带**

```
没有DMA：
  CPU一个个搬样本给I2S
  48000次/秒 × 每次几微秒 = CPU大部分时间在搬运
  没时间干别的

有DMA：
  CPU："DMA，这有一堆数据（地址+大小），你自动送给I2S"
  然后CPU去干别的
  DMA按硬件时钟自动搬运
  搬到一半发通知，搬完发通知
```

**DMA的价值**：
- 不占CPU（硬件自动干）
- 时序精准（硬件时钟控制，微秒级）
- 支持中断通知

**本项目DMA配置**：
```
外设：SPI2的TX寄存器
通道：DMA1_Stream0
模式：循环模式（CIRC）
中断：HTIE（半传输）+ TCIE（全传输）+ TEIE（错误）
```

### 🔔 任务通知：DMA中断如何唤醒AudioTask？

**问题**：DMA中断怎么告诉AudioTask？

**方案A（错）**：中断里直接干活
```
❌ DMA中断处理函数 {
    读SD卡（50ms）  ← 中断里干50ms
    转换数据
}
问题：其他中断全被阻塞，触摸没响应，系统卡死
```

**方案B（对）**：中断发消息，任务处理
```c
✅ DMA中断处理函数 {
    // 只做几微秒
    读中断标志
    清中断标志
    xTaskNotifyFromISR(AudioTask, 通知位, ...);
    portYIELD_FROM_ISR();
}

AudioTask {
    等待通知(portMAX_DELAY);
    if (HALF0_DONE) 填Half0;
    if (HALF1_DONE) 填Half1;
}
```

**"中断轻量化，任务干重活"是嵌入式黄金法则**。

**为什么用任务通知而不是队列？**

| 维度 | 队列 | 任务通知 |
|---|---|---|
| 传数据 | 大结构体 | 32位值 |
| 速度 | 较慢 | 极快 |
| 场景 | 复杂数据 | 事件通知 |

- 只需要告诉AudioTask"哪个半区完成了" → 32位够用
- 需要极快（DMA中断频率高）
- **任务通知完美匹配** ✅

### 🍳 AudioTask的日常工作

```c
AudioTask主循环 {
    ① 报告心跳"我还活着"
    ② 检查命令队列（PLAY/PAUSE/NEXT等）
    ③ 等DMA通知（最多10ms）
       收到 HALF0 → 填充Half 0
       收到 HALF1 → 填充Half 1
       收到 ERROR → 停止并报错
    ④ 每250ms发布播放进度
    ⑤ 回到①循环
}
```

**为什么"等最多10ms"？**
- 永远等：命令队列的PLAY/PAUSE没人处理，用户点了按钮没反应
- 不等：一直轮询浪费CPU
- 折中：等10ms，有通知立即处理，没通知也醒来检查命令

### 📄 WAV文件格式

**类比：便当盒**

```
WAV文件像分格便当盒：
┌──────────────────────────┐
│  RIFF头（"我是WAV文件"）  │
├──────────────────────────┤
│  fmt格式（采样率/声道数）  │
├──────────────────────────┤
│  data数据（PCM音频样本）   │
└──────────────────────────┘
```

**本项目支持的格式**：
- ✅ PCM（不压缩）、16bit、单/立体声、16k/32k/44.1k/48k Hz
- ❌ MP3/AAC/FLAC（需要解码器）、24/32bit、5.1声道

**为什么限制这些？**
- MP3/AAC需要解码算法（占CPU）
- 24/32bit需要改DMA配置
- 嵌入式支持标准PCM WAV已经够用

**什么是PCM？**

```
真实声波：连续的波浪
PCM采样：每隔一段时间量一次高度
48kHz = 每秒切48000片
每个值用16位整数存储（-32768 ~ +32767）
```

### 🎧 单声道转双声道

**问题**：有些WAV只有单声道，但MAX98357A是立体声输出。

**解决方案**：复制左右声道
```c
// 单声道原始：[10, 15, 20, 15]
// 转换成双声道
dest[0] = 10;  dest[1] = 10;  // 左=右=10
dest[2] = 15;  dest[3] = 15;
dest[4] = 20;  dest[5] = 20;
// 效果：左右耳听到一样，就是单声道
```

**双声道WAV**：数据已经是[L,R,L,R]格式，直接送DMA。

### 🔊 软件音量控制

**类比：数字调光**
```
原始信号强度：100%
调到50%：每个样本 × 0.5

sample = (source[i] * volume_percent) / 100;
```

**档位**：0%、25%、50%、75%、100%

**为什么不是模拟音量？**
- MAX98357A没有音量控制引脚
- 只能软件调节

**数字音量的隐患**：量化失真
```
100%时：32767精度完美
1%时：32767 × 0.01 = 327，精度损失99%
极低音量时声音失真明显
```

档位设计25/50/75不会导致明显失真。

### 💾 D-Cache一致性：Cortex-M7的必修课

**这是最容易被忽略但会导致玄学bug的问题！**

**Cache是什么？**

**类比：办公桌 vs 仓库**
```
CPU（你） ── 办公桌（Cache，快）── 仓库（内存，慢）

要用文件时：
  先看办公桌上有没有
  没有 → 从仓库拿一份放桌上
  用完 → 放桌上（不立即回仓库）
```

**Cache导致的DMA问题**：
```
场景：
  1. CPU在办公桌上写了新数据"音频缓冲Half 0已更新"
  2. 但数据还没回仓库（还在Cache里）
  3. DMA去仓库拿数据 → 拿到的是"旧数据"
  4. DMA播放旧数据 → 声音重复/爆音/错误
```

**解决方案：主动"清Cache"**
```c
// AudioTask填充完Half 0后
SCB_CleanDCache_by_Addr(half0_addr, half0_size);
__DSB();  // 数据同步屏障

// 含义：
// "把办公桌上的新数据立刻同步回仓库"
// "DSB保证同步完成后才继续"
```

**32字节对齐**：
```c
int16_t g_audio_dma_buffer[65504] __attribute__((aligned(32)));
```

**为什么32字节？** Cortex-M7的Cache Line大小是32字节，不对齐可能导致Cache清理时出错。

**很多初学者忽略这一步就会遇到玄学音频bug**。

### ⏸️ 播放暂停恢复

**暂停：不是"停止"**
```
点PAUSE：
  1. 保存当前状态到 g_audio_paused_state
  2. 硬件暂停SPI2（CSUSP位）
  3. DMA缓冲保留（不清空）
  4. 文件位置保留（不关闭）
  5. 状态改为 PAUSED

关键：一切"冻结"，不释放资源
```

**恢复：从暂停处继续**
```
点RESUME：
  1. 恢复SPI2硬件
  2. DMA继续从当前位置读取
  3. 无缝继续播放
```

**停止：真的清理**
```
点STOP：
  1. 停止SPI2和DMA
  2. 关闭SD卡上的WAV文件
  3. 释放文件句柄
  4. 状态改为 STOPPED
  再次点PLAY：从头开始
```

### 🎼 440Hz测试音的用途

**类比：医院的听力测试**

**测试音价值**：
- 验证SPI2/I2S时钟工作
- 验证MAX98357A接线正确
- 验证DMA工作正常
- 验证扬声器没坏
- 验证音量链路
- 验证AudioTask运行

**不用SD卡，纯软件生成**：
```
用相位累加器生成方波
每帧：+6000 或 -6000
持续2秒
左右声道相同
```

**为什么是方波不是正弦波？**
- 方波：一行代码搞定
- 正弦波：需要浮点/查表
- 测试音验证硬件，方波够了

**诊断价值**：
- 测试音正常但WAV放不出声 → 硬件通路OK，问题在SD/WAV解析
- 测试音都没声 → 硬件问题（接线/时钟/功放）

### 🎬 播放完成的优雅处理

**问题**：数据读完但DMA缓冲里还有340ms要播

**优雅停止流程**：
```
1. 检测到EOF
2. 当前半区正常填满
3. 另一个半区填零（无声）
4. 设置 g_audio_stop_after_half
5. 等DMA播完最后半区
6. 调用 app_audio_stop("PLAYBACK COMPLETE")
7. 关闭SPI2/DMA
```

**为什么另一个半区填零？**
- 不填零，DMA读到旧数据 → 出现循环
- 填零 → 播放静音 → 无声结束

**为什么不直接停止DMA？**
- 最后半区的音频没播完 → 声音突然截断 → "咔嚓"声

### 📸 状态快照 + revision

**问题**：GUI每500ms刷新一次，怎么知道什么变了？

**revision机制**：
```c
struct snapshot {
    uint32_t revision;  // 版本号
    state, volume, progress...
};

// AudioTask每次改状态
snapshot.revision++;

// GuiTask刷新
if (snapshot.revision != last_revision) {
    last_revision = snapshot.revision;
    刷新界面;
}
```

**两级刷新策略**：
- 曲目名/采样率/位宽变了 → 完整重绘（app_ui_show_music）
- 只有状态/音量/进度变了 → 局部刷新（app_ui_update_music）

**为什么？** 完整重绘慢会闪烁，局部刷新快只更新变化区域。

### 🎯 AudioTask优先级5的深层含义

**优先级对比**：
```
6 = HealthTask    最高
5 = AudioTask     ← 这里
4 = InputTask
4 = StorageTask
3 = GuiTask
2 = TimerTask
1 = MonitorTask
```

**为什么Audio优先级比GUI高？**
```
音频DMA每340ms需要新数据
如果AudioTask晚到几毫秒：
  → DMA播放旧数据或零
  → 声音卡顿/爆音/循环片段

用户对音频不连续极其敏感（比画面卡顿敏感10倍）
```

**代价**：如果SD卡异常，AudioTask卡住会影响GUI响应。

**缓解方案**：等待SD卡时让出CPU
```c
while (没收到响应) {
    vTaskDelay(pdMS_TO_TICKS(1));  // 每1ms让出CPU
}
```

**"高优先级但礼让"的设计**。

### 💾 内存占用分析

```
g_audio_dma_buffer  ≈ 131,008 字节 (128 KB)
g_audio_raw_buffer  ≈  65,504 字节 (64 KB)
─────────────────────────────────
音频模块总占用      ≈ 192 KB
```

**为什么要这么大？**

**计算缓冲时长**：
```
Half大小：32752个int16_t
双声道48kHz：32752 / 2 / 48000 = 340ms

也就是说：
  DMA播完Half 0 → AudioTask有340ms时间填充Half 1
  340ms足够SD卡读一次数据（约50ms）
```

**如果缓冲小一点会怎样？**
```
假设Half只有10ms：
  AudioTask必须在10ms内完成"通知→读SD→转换→清Cache"
  SD卡读一次50ms → 根本来不及
  → 音频卡顿
```

**缓冲越大越安全，但内存占用越多**。这是**内存换稳定性**的经典权衡。

### 🚨 当前设计的隐患

**💣 隐患1：AudioTask同步等待SD卡**
- 如果SD卡异常，AudioTask卡住2秒
- 期间DMA继续播放旧数据 → 循环片段
- 改进：改为异步事件驱动

**💣 隐患2：SDMMC轮询关中断**
- SD卡读取时关闭所有中断
- 期间DMA的HTIE/TCIE被屏蔽 → 通知延迟
- 改进：改用SDMMC的DMA传输

**💣 隐患3：内存占用大（192KB）**
- STM32H743总内存约1MB
- 音频+LCD帧缓冲+任务栈可能触及上限
- 改进：缩小缓冲（但需要更快的SD卡）

**💣 隐患4：曲目没有排序**
- FatFs的readdir返回顺序不确定
- NEXT/PREVIOUS顺序不确定
- 改进：读完后按文件名strcmp排序

### 💡 结合前面模块的知识串联

| 学过的概念 | 在MUSIC中的体现 |
|---|---|
| **优先级** | AudioTask(5) > GuiTask(3)，保证音频优先 |
| **任务状态** | AudioTask大部分时间在xTaskNotifyWait睡眠 |
| **队列** | AudioCommands队列传命令，独立响应队列 |
| **任务通知** | ⭐ DMA中断→AudioTask的经典应用 |
| **临界区** | 更新g_audio_public时使用 |
| **快照发布** | revision机制，和MONITOR一样 |
| **串行化任务** | StorageTask独占FatFs，AudioTask不直接碰SD |
| **静态分配** | 128KB DMA缓冲静态分配，不malloc |
| **业务层busy** | 命令队列做流量控制 |

### 🎓 15个关键点总结

1. 音频每秒48000次精准送数据 → 必须用DMA
2. DMA双缓冲是音频流的黄金架构
3. DMA中断只发通知，任务干重活
4. 任务通知比队列快，适合ISR→Task
5. AudioTask优先级5（比GUI高）
6. AudioTask不直接碰FatFs → 通过StorageTask
7. Half缓冲340ms给SD卡"喘气时间"
8. 循环DMA模式实现无缝播放
9. D-Cache一致性必须处理
10. 32字节对齐匹配Cache Line
11. 单声道复制到双声道
12. 软件音量=样本×百分比/100
13. 暂停保留状态，停止清理资源
14. 播放完成先填零再等DMA播完
15. revision机制实现高效UI刷新

### 🎯 一句话总结

> **MUSIC模块是本项目最综合的FreeRTOS应用**：涵盖优先级设计、多队列通信、任务通知、临界区保护、串行化任务复用、静态分配、硬件抽象（DMA/I2S/Cache）、状态机、异步IO等所有核心技术。

**理解MUSIC模块 = 掌握嵌入式实时音频的核心技术栈**。

---

**MUSIC模块笔记结束 🎵**

追加日期：2026-09-07

