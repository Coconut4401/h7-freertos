# FreeRTOS 学习笔记（基于 STM32H743_5inch_GT911_Test 项目）

> 本笔记覆盖已学模块 1~5、7，是"边学边和项目对照"的完整整理。所有例子和陷阱都取自本项目实际代码，方便对照。

---

## 学习路线全景

| # | 模块 | 状态 | 核心问题 |
|---|---|---|---|
| 0 | 基础配置与优先级 | ✅ | tick/优先级/门槛怎么回事 |
| 1 | 任务与调度 | ✅ | 任务怎么"睡"、周期任务怎么做 |
| 2 | 队列 | ✅ | 数据怎么传递、串行化模式 |
| 3 | 中断与任务的桥梁 | ✅ | ISR 怎么通知任务、DMA 中断怎么写 |
| 4 | 互斥量 & 优先级反转 | ✅ | 共享资源怎么保护 |
| 5 | 软件定时器 & 事件组 | ✅ | 周期回调、多任务广播 |
| 6 | 栈溢出 & 堆管理 | ✅ | RAM 怎么被吃掉、怎么监控 |
| 7 | 静态 vs 动态分配 | ✅ | 编译期确定 vs 运行时分配 |
| 8 | tickless idle 低功耗 | ⏳ 待学 | 待机省电 |

---

## 项目架构速览

```
InputTask(优先级4)  ──[TouchEvents队列]──▶ GuiTask(优先级3)
                                              │
                                              ├──[Storage.Requests队列]──▶ StorageTask(优先级4) ──▶ SD卡
                                              │◀─[Storage.Responses队列]─┘
                                              │
                                              └──[AudioCommands队列]────▶ AudioTask(优先级3) ──▶ DMA──▶ 喇叭
                                                                            ▲
                                                              [任务通知 xTaskNotify: DMA中断]
MonitorTask(优先级1) ── vTaskDelay(500ms) 周期 ── 采样 heap/栈水印/LED
Timer Service Task(优先级2) ── 内核自带 ── 目前项目未使用软件定时器
```

**核心设计模式**：所有资源访问都通过**串行化任务**——SD 卡只由 StorageTask 访问、音频只由 AudioTask 访问、DMA 中断只通知 AudioTask。这种设计**从架构层消除并发**，比"到处加锁"更优雅。

---

## 模块 0：基础配置与优先级门槛

### 0.1 configTICK_RATE_HZ = 1000

**含义**：SysTick 中断每 1 ms 触发一次，形成系统节拍。

**影响**：
- `vTaskDelay(1)` 最小延时粒度 = 1 ms
- 时间片轮转周期 = 1 ms
- 频率越高调度越精细，但 tick 中断本身开销越大

### 0.2 任务优先级设计

项目 5 个任务优先级：

| 任务 | 优先级 | 设计理由 |
|---|---|---|
| InputTask | 4 | 触摸必须及时采集，但每 5 ms 只跑一小段 |
| StorageTask | 4 | 大部分时间在 `xQueueReceive` 睡眠，一有请求立即处理 |
| GuiTask | 3 | UI 渲染可以稍等 |
| AudioTask | 3 | 音频硬实时，但主要靠 DMA 中断唤醒 |
| MonitorTask | 1 | 只是观察者，别影响正业 |

**核心原则**：**优先级不是重要性，而是"响应延迟敏感度 × 执行时间短"**。经常睡眠的任务给高优先级是安全的。

### 0.3 中断优先级与 syscall 门槛（最容易踩的坑）

**Cortex-M 反直觉规则**：**数字越小，优先级越高**。0 = 皇帝，15 = 打工人。

**configMAX_SYSCALL_INTERRUPT_PRIORITY = 5** 是一条**分界线**：

| 优先级数字 | 实际优先级 | 允许调 FromISR API |
|---|---|---|
| 0 ~ 4 | 超高（比内核还高） | ❌ 绝对禁止 |
| 5 ~ 15 | 内核可控 | ✅ 允许 |

**为什么？** 内核操作数据结构时用 `BASEPRI` 寄存器屏蔽 ≥5 的中断，保护内部数据。<5 的超高优先级中断照常触发，但**不能调用内核 API**，否则破坏内核状态。

**本项目**：DMA 中断优先级 = 6（≥5），可以安全调用 `xTaskNotifyFromISR`。

### 0.4 HAL_NVIC_SetPriority 三个参数

```c
HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 6, 0);
//                        ↑             ↑  ↑
//                        中断号        抢占优先级 子优先级
```

- **中断号**：哪个中断（枚举）
- **抢占优先级**：谁能打断谁（就是我们讲的"数字越小越高"）
- **子优先级**：同抢占级的排队顺序（不能抢占，只影响谁先响应）

**推荐**：`NVIC_PRIORITYGROUP_4`——全部 4 位当抢占级，子优先级不用，避免混淆。

### 0.5 常见外设的作用

| 外设 | 作用 | 比喻 | 项目用途 |
|---|---|---|---|
| **DMA** | 后台搬数据，不占 CPU | 快递员 | 音频数据搬到 SAI |
| **串口 USART** | 双向异步通信 | 对讲机 | 调试打印 |
| **定时器 TIM** | 计数、PWM、精确触发 | 闹钟 | SysTick 本质是定时器 |
| **外部中断 EXTI** | 引脚变化触发中断 | 门铃 | GT911 触摸中断 |

---
## 模块 1：任务与调度

### 1.1 任务的五种状态

```
        ┌────────────┐
        │  Ready     │ ← 就绪，等 CPU 分配
        └─────┬──────┘
              │ 调度器选中
              ▼
        ┌────────────┐
        │  Running   │ ← 正在跑
        └─────┬──────┘
              │
     ┌────────┼──────────┐
     │        │          │
     ▼        ▼          ▼
 vTaskDelay  被抢占   vTaskSuspend
     │        │          │
     ▼        ▼          ▼
 ┌────────┐ Ready   ┌────────┐
 │Blocked │         │Suspended│
 └────────┘         └────────┘
```

- **Running**：占着 CPU（同一时刻只能有一个）
- **Ready**：随时能跑，但优先级更高的在跑
- **Blocked**：主动等某事（延时、队列、信号量），不占 CPU
- **Suspended**：被 `vTaskSuspend()` 强制挂起，直到 `vTaskResume`
- **Deleted**：已销毁（`vTaskDelete`）

### 1.2 让任务"睡"的两种方式

#### vTaskDelay：从现在起等 N 个 tick

```c
while (1) {
    do_work();
    vTaskDelay(pdMS_TO_TICKS(100));   // 从做完事的这一刻起数 100 ms
}
```

**问题**：如果 `do_work` 耗时 30 ms，实际周期 = 30 + 100 = **130 ms**，周期不稳定。

#### vTaskDelayUntil：绝对周期

```c
TickType_t last = xTaskGetTickCount();
while (1) {
    do_work();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(100));  // 保证 100 ms 精确周期
}
```

**用途**：**采样类任务**（比如 InputTask 每 5 ms 采一次触摸）必须用这个，否则采样频率会漂移。

### 1.3 pdMS_TO_TICKS 宏

```c
#define pdMS_TO_TICKS(x)  ((x) * configTICK_RATE_HZ / 1000)
```

在项目里 `configTICK_RATE_HZ=1000` → `pdMS_TO_TICKS(100) = 100`。**永远用这个宏**，不要写死数字，否则改了 tick 频率代码就错了。

---

## 模块 2：队列（Queue）

### 2.1 基本 API

```c
QueueHandle_t q = xQueueCreate(容量, 元素大小);
xQueueSend(q, &data, 超时);       // 发送（拷贝数据）
xQueueReceive(q, &data, 超时);    // 接收（拷贝出）
xQueueSendFromISR(q, &data, &woken);   // ISR 里用这个
```

**注意**：队列是**值拷贝**语义——发送时把整个结构体拷进队列，接收时拷贝出来。发送方之后可以修改原变量，不会影响队列里的副本。

### 2.2 超时参数的三种典型写法（最重要！）

| 写法 | 行为 |
|---|---|
| `xQueueSend(q, &d, 0)` | 不等，满了立刻返回 `errQUEUE_FULL` |
| `xQueueSend(q, &d, pdMS_TO_TICKS(100))` | 等最多 100 ms |
| `xQueueSend(q, &d, portMAX_DELAY)` | 永远等，直到成功 |

**关键理解**：
- `errQUEUE_FULL` **不是**"立刻返回"，而是"**超时到期还没能发送**"
- 是否阻塞由**第三个参数**决定
- `portMAX_DELAY` = `0xFFFFFFFF`，被内核识别为"永远等"

**阻塞机制**：
1. `xQueueReceive` 发现队列空 → 任务被放入"该队列的等待接收列表" → 阻塞态
2. 任务**不占 CPU**，调度器让其他任务运行
3. 有其他任务 `xQueueSend` → 内核**立刻唤醒**等待任务
4. 或超时到期，`xQueueReceive` 返回 `errQUEUE_EMPTY`

这是 RTOS 相比裸机的核心价值——**任务可以睡眠等事件，而不用轮询**。

### 2.3 队列容量的经验值

**容量 = "最坏情况下短时突发数量" + 少量余量**

例子：TouchEvents 队列容量 8——
- 用户可能快速多次点击 → 短时突发 4~6 个事件
- 8 个容量能吸收突发，同时 GuiTask 有时间消费
- 太小（1）→ 高频操作会丢事件
- 太大（100）→ 浪费 RAM，且掩盖"消费方慢"的 bug

**诊断技巧**：
```c
UBaseType_t remaining = uxQueueMessagesWaiting(queue);
```
放到 MonitorTask 里定期打印。如果接近容量 → 消费方慢，需要优化。

### 2.4 串行化任务模式（本项目核心架构）

**问题**：多个任务想访问同一个资源（SD 卡、SPI 总线）怎么办？

**方案 A：互斥量**——各调用者 take/give 锁
**方案 B：串行化任务**——把所有请求发到一个专属任务处理

**本项目选方案 B**：
```
GuiTask/AudioTask/MonitorTask ──[队列]──▶ StorageTask ──▶ SD/FatFs
```

**方案 B 的四大优势**：

1. **调用方不阻塞在慢 IO 上**：SD 卡写扇区可能 50 ms，调用方只需微秒级 `xQueueSend`
2. **请求可缓冲和批处理**：多个请求排队，可合并、可排序
3. **FatFs 天然安全**：只有 StorageTask 一个"手"在动 FatFs 全局状态，无需 `_FS_REENTRANT`
4. **错误处理集中**：SD 错误只在 StorageTask 里处理，业务代码干净

**核心纪律**："这个资源，只能由那一个任务碰"。**从架构层消除并发**，比用锁保护更优雅。这就是 Erlang / Go channel 的思想。

---
## 模块 3：中断与任务的桥梁

### 3.1 任务通知（Task Notification）

**FreeRTOS V8.2 引入的最快 IPC 机制**。相比信号量：

| 维度 | 信号量 | 任务通知 |
|---|---|---|
| RAM | ~80 字节动态分配 | TCB 里预留，零额外分配 |
| 速度 | 遍历队列结构 | 直接改目标 TCB |
| 语义 | 单一 | 5 种模式（值、位、递增、覆盖） |
| 场景 | 多对多 | **一对一** |

### 3.2 项目 DMA 中断的经典写法

```c
// DMA1_Stream0_IRQHandler（音频 I2S TX DMA）
uint32_t bits = 0;
if (HTIF) bits |= 0x01;   // 半传输完成
if (TCIF) bits |= 0x02;   // 全传输完成
if (TEIF) bits |= 0x04;   // 错误

BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xTaskNotifyFromISR(
    g_audio_task_handle,
    bits,
    eSetBits,           // 位或模式
    &xHigherPriorityTaskWoken
);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

AudioTask 端：
```c
uint32_t notified;
xTaskNotifyWait(0, 0xFFFFFFFF, &notified, portMAX_DELAY);
if (notified & 0x01) handle_half();
if (notified & 0x02) handle_full();
if (notified & 0x04) handle_error();
```

**这是"用任务通知当事件位"的最佳实践**——一对一、多事件类型、超低延迟。

### 3.3 任务通知的 5 种模式

```c
xTaskNotify(handle, value, eAction);
```

| eAction | 含义 | 场景 |
|---|---|---|
| `eNoAction` | 仅唤醒，不改通知值 | 纯事件信号 |
| `eSetBits` | 位或 | **本项目 DMA 用这个** |
| `eIncrement` | 递增 | 当作计数信号量 |
| `eSetValueWithOverwrite` | 覆盖写 | 当作邮箱 |
| `eSetValueWithoutOverwrite` | 若没读过则写，读了才写 | 保证不丢 |

### 3.4 portYIELD_FROM_ISR 是什么

ISR 结尾必须写：
```c
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

**作用**：如果被唤醒的任务优先级 > 当前被中断任务，**立即触发调度**，中断返回时直接切到高优先级任务，而不是先回到被中断的任务再等下次 tick。

**记忆点**：`xHigherPriorityTaskWoken` 是"内核告诉你有没有需要立刻切换的任务"的输出参数。你只需要传给 `portYIELD_FROM_ISR`。

---

## 模块 4：互斥量 & 优先级反转

### 4.1 项目隐患：printf 并发未加锁

多个任务都调 `printf` → 底层同一个 USART1 → **交错乱码**：
```
Heap: 45ERR!
678
```
这就是**竞态条件（Race Condition）**。

### 4.2 互斥量修复

```c
SemaphoreHandle_t g_uartMutex = xSemaphoreCreateMutex();

void safe_printf(const char *fmt, ...) {
    xSemaphoreTake(g_uartMutex, portMAX_DELAY);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    xSemaphoreGive(g_uartMutex);
}
```

### 4.3 优先级反转（Priority Inversion）

**场景**：H（高）、M（中）、L（低）三任务，L 拿着锁 H 想要，M 抢占 L。

**灾难流程（如果用二值信号量）**：
```
L 拿锁 → H 想拿 → 阻塞 → M 就绪抢占 L → M 干了 20ms 别的事 → L 才能继续 → 释放锁 → H 才能拿
结果：H 被中优先级 M 间接拖延 21ms
```

**火星探路者事故**（1997）：VxWorks 里的优先级反转导致频繁重启，工程师**远程打补丁开启优先级继承**才解决。

### 4.4 互斥量的救命机制：优先级继承

**FreeRTOS 的 `xSemaphoreCreateMutex()` 自带优先级继承**：

```
L 拿锁 → H 想拿 → 阻塞
★ 内核检测到 H 被 L 挡着 → 把 L 的优先级临时提到 H 的水平
→ M 就绪 → 优先级 3 vs L(现在=H) → M 抢不了 L
→ L 快速释放锁 → 内核把 L 优先级恢复 → H 立刻拿到锁
```

**为什么二值信号量做不到**：它没有"所有者"概念，内核不知道 Give 的会是谁，无法决定提升谁的优先级。互斥量记录了持有者，才能做优先级继承。

### 4.5 递归互斥量

```c
#define configUSE_RECURSIVE_MUTEXES  1   // 本项目开着
```

**用途**：允许**同一任务多次 Take** 同一把锁（比如 A 加锁函数内部又调了 B 加锁函数）。普通互斥量会自死锁，递归的会计数：Take N 次需要 Give N 次才真正释放。

**建议**：递归互斥量方便但容易掩盖设计问题。**优先重构消除嵌套**。

### 4.6 互斥量三大禁区

1. **中断里不能用**：没有 `xSemaphoreTakeFromISR(mutex)` API。ISR 用**任务通知**或**二值信号量**
2. **不能保护可能永久阻塞的操作**：临界区代码要**短、快、可预期**
3. **不能反向 Give**：互斥量要求"谁 Take 谁 Give"

### 4.7 互斥量 vs 临界区 vs 二值信号量

| 场景 | 保护什么 | 用什么 |
|---|---|---|
| 几行赋值/拷贝小结构 | 变量 | **临界区**（~50ns，关中断） |
| printf/SD 卡/SPI 等长时操作 | 外设 | **互斥量**（可阻塞、优先级继承） |
| 事件同步（无所有者） | 事件 | **二值信号量或任务通知** |
| 计数事件 | 累积 | **计数信号量** |

**"短用临界区，长用互斥量"**。临界区关中断上限一般 **10 μs 以内**。

### 4.8 项目 taskENTER_CRITICAL 的用法

项目 `app_audio.c`、`app_monitor.c` 等多处：
```c
taskENTER_CRITICAL();
snapshot = g_stats;   // 拷贝几十字节
taskEXIT_CRITICAL();
```
这是**正确用法**——短小快速操作用临界区，比互斥量快 40 倍以上。

---
## 模块 5：软件定时器 & 事件组

### 5.1 软件定时器（Software Timer）

**基本用法**：
```c
TimerHandle_t timer = xTimerCreate(
    "MyTimer",
    pdMS_TO_TICKS(500),   // 周期
    pdTRUE,               // pdTRUE=周期性, pdFALSE=单次
    NULL,                 // ID
    my_callback           // 到期回调
);
xTimerStart(timer, 0);
```

**内部原理**：所有软件定时器**共享一个 Timer Service 任务**（项目里优先级 2、栈 512）：
1. 服务任务维护"按到期时间排序的列表"
2. 自己 `vTaskDelay` 睡到"最近到期"
3. 醒来执行回调
4. 计算下一次最近到期时间，继续睡

一个服务任务能管几十上百个定时器，只占一份栈。

### 5.2 软件定时器三大禁区

1. **回调不能阻塞**：`xQueueReceive`、`vTaskDelay` 等都禁用。会**阻塞整个 Timer Service 任务**，所有定时器全瘫。
2. **回调要短**：定时器服务是单车道，你回调跑 10 ms，其他所有定时器都晚 10 ms
3. **精度受限**：定时器服务任务优先级 2，如果有高优先级任务霸占 CPU，定时器会**延迟触发**。需要微秒级用硬件定时器。

**如果非要在回调里干耗时事**：转发给任务
```c
void my_callback(TimerHandle_t t) {
    xQueueSend(work_queue, &task_id, 0);  // 立刻返回
}
```

### 5.3 单次定时器的常用模式：超时保护

```c
xTimerCreate("...", timeout, pdFALSE, ...);   // pdFALSE = 单次

void send_command(void) {
    send_to_uart(cmd);
    xTimerStart(timeout_timer, 0);   // 5s 没回应就报错
}

void on_response(void) {
    xTimerStop(timeout_timer, 0);    // 收到就取消
}
```

**背光自动熄灭的例子**（比新建任务方案更优雅）：
```c
TimerHandle_t backlight_timer = xTimerCreate(
    "BL", pdMS_TO_TICKS(30000), pdFALSE, NULL, backlight_off);

// InputTask 每次触摸时：
lcd_backlight_on();
xTimerReset(backlight_timer, 0);   // 每次触摸重新开始 30s 倒计时
```

**核心 API：`xTimerReset`**——无论定时器当前是"未启动/运行中/已到期"，都会重新开始倒计时。这是"延时关闭/心跳超时"的**教科书模式**。

### 5.4 软件定时器 vs 任务+delay 选型

| 维度 | 软件定时器 | 任务+vTaskDelay |
|---|---|---|
| 内存 | ~40 字节 + 共享服务任务 | 每任务 512~1024 words 栈 |
| 优先级 | 服务任务优先级（一般较低） | 独立可配 |
| 能否阻塞 | ❌ | ✅ |
| 适合 | 大量轻量周期任务、超时管理 | 复杂状态、需阻塞等待 |

**判断原则**：如果一个"任务"的整个循环体就是 `wait → 简单动作`，且动作能在几微秒内完成不阻塞，那**它更适合软件定时器**。

### 5.5 事件组（Event Group）

**核心特性**：24 位标志位集合，支持**多任务同时等**、**等待多位组合**。

```c
EventGroupHandle_t eg = xEventGroupCreate();

xEventGroupSetBits(eg, 0x03);       // 置 bit0 和 bit1
xEventGroupClearBits(eg, 0x01);     // 清 bit0

uint32_t bits = xEventGroupWaitBits(
    eg,
    0x07,               // 等的位（bit0|bit1|bit2）
    pdTRUE,             // 拿到后是否清除
    pdFALSE,            // pdTRUE=等所有位, pdFALSE=等任意一位
    portMAX_DELAY
);
```

### 5.6 事件组的三大独特能力

1. **多任务同时等同一事件**（广播）
```c
// A/B/C 三个任务都在等 BIT_START
xEventGroupSetBits(eg, BIT_START);   // → 三个任务同时被唤醒
```
**任务通知做不到**——任务通知是"点对点"。

2. **等待"多个事件都满足"**
```c
xEventGroupWaitBits(eg, WIFI_READY | SERVER_READY, pdTRUE, pdTRUE, ...);
//                                                          ↑ pdTRUE = 等全部
```

3. **同步屏障**（多任务在某点集合，全到齐才继续）
```c
xEventGroupSync(eg, MY_BIT, ALL_TASK_BITS, portMAX_DELAY);
```

### 5.7 事件组 vs 任务通知选型

| 场景 | 最佳 |
|---|---|
| 一对一事件（DMA 通知 AudioTask） | 任务通知 |
| 一对多广播（Shutdown 唤醒所有任务） | **事件组** |
| 等待多个条件同时满足 | **事件组**（唯一支持"等全部"） |
| ISR 里通知，追求最低延迟 | 任务通知（事件组要通过定时器服务任务转发） |

**本项目 DMA 用任务通知就是最优解**——一对一、极简、最快。

### 5.8 事件组的坑：pdTRUE 自动清除

如果多任务都等同一 bit 且都设了 pdTRUE：**先醒来的任务把位清了**，后来的等不到！

**解决方案**：
- 用 pdFALSE，让发送方定时清位
- 或为每个消费者分配独立位：`DMA_DONE_FOR_AUDIO`、`DMA_DONE_FOR_LOG`

---
## 模块 6：栈溢出 & 堆管理

### 6.1 项目隐患配置

```c
#define configCHECK_FOR_STACK_OVERFLOW    0   // ⚠️ 栈溢出静默，不报错
#define configUSE_MALLOC_FAILED_HOOK      0   // ⚠️ malloc 失败静默
#define configTOTAL_HEAP_SIZE  (128 * 1024)   // 128 KB 堆
```

MonitorTask 只监控了 3 个任务栈水印，**遗漏了 StorageTask / AudioTask / TimerTask**。

### 6.2 栈是什么

每个任务有独立栈，`xTaskCreate("T", 512, ...)` 会从 heap 分配 2048 字节作为该任务栈。

**栈上放什么**：
1. 局部变量（`int x = 5`）
2. 函数调用返回地址
3. 函数参数
4. 保存的 CPU 寄存器（切换任务/中断时）

**Cortex-M 栈向下增长**：
```
高地址  ┌───────────┐  ← 栈底（初始 SP）
        │  已使用    │
        │     ↓      │
        │  当前 SP   │
        │            │
        │  未使用    │
低地址  └───────────┘  ← 栈的地址下限
```

### 6.3 栈溢出的破坏

**后果 A：踩到相邻任务的栈**——症状随机、难复现，"莫名其妙的数据错乱"。**最难查的 bug 之一**：错误发生的位置和真正原因不在同一个任务里。

**后果 B：踩到内核数据结构**——调度崩溃、HardFault、死机。

### 6.4 两种栈溢出检测方式

#### Method 1：切换时检查 SP 位置
```c
#define configCHECK_FOR_STACK_OVERFLOW  1
```
每次任务切换时，检查即将换出任务的 SP 是否已越过栈的下限。

**优点**：几乎零开销
**缺点**：**只在切换时看**——如果"栈瞬间冲高再回落"就漏检

#### Method 2：栈末端"金丝雀"
```c
#define configCHECK_FOR_STACK_OVERFLOW  2
```
任务创建时栈末端 16 字节填充为 `0xA5A5A5A5`。切换时检查这 16 字节有没有被改动。

**优点**：能捕获"曾经踩过"的情况（金丝雀被踩过就永远显示"死了"）
**缺点**：略高开销

**推荐 Method 2**——嵌入式最常见的"局部大数组"陷阱（Method 1 漏检）它能捕获。

### 6.5 Method 1 vs Method 2 关键差别

**Method 1 能测到的场景**：递归调用——栈是"持续深"的，任何切换都能捕获。

**Method 1 漏检、Method 2 能捕获的场景**：
```c
void one_shot_function(void) {
    uint8_t huge_buffer[8000];   // 瞬间栈涨 8000 字节 → 已越界
    process(huge_buffer);
    // 函数返回，栈回到浅位置
}
```
函数返回后 SP 回到安全区，此时切换 → Method 1 看 SP 正常，漏检。但金丝雀已被踩过 → Method 2 捕获。

### 6.6 栈溢出钩子函数

```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    printf("STACK OVERFLOW in task %s !!\n", pcTaskName);
    __disable_irq();
    while (1) { /* 等你 attach 调试器 */ }
}
```

### 6.7 栈水印（High Water Mark）

```c
UBaseType_t remaining = uxTaskGetStackHighWaterMark(taskHandle);
// 单位：word (Cortex-M 上 = 4 字节)
// 返回：栈从创建到现在"最深处"离底还剩多少 words
```

**原理**：内核创建任务时把栈全填成 `0xA5A5A5A5`，查询时从栈底向上扫描找第一个非 `0xA5` 位置。

### 6.8 栈使用率判断标准

| 剩余水印比例 | 使用率 | 判断 |
|---|---|---|
| > 30% | < 70% | ✅ 安全 |
| 15% ~ 30% | 70% ~ 85% | ⚠️ 关注 |
| 5% ~ 15% | 85% ~ 95% | 🟠 危险 |
| < 5% | > 95% | 🔴 极危险 |

**判断"是否真的安全"的完整流程**：
1. 运行 **至少 24 小时**，覆盖各种场景
2. 特别测试**错误处理路径**（SD 拔出、文件损坏等罕见路径可能吃栈）
3. 记录**历史最低水印**
4. 目标：历史最低 > 30%

**大坑**：Cortex-M 上 ISR 用被中断任务的栈（PSP 模式）→ 任务栈要给"任务本身 + 最深中断嵌套"两者留空间。

### 6.9 heap_4 详解

FreeRTOS 5 种堆管理方案里，**heap_4 是项目使用的**。核心特性：**相邻空闲块自动合并**。

```
初始（128 KB 空闲）：
┌────────────────────────────────┐
│   ONE BIG FREE BLOCK 128 KB    │
└────────────────────────────────┘

分配后：
┌───┬───┬───┬───┬─────────────────┐
│用A│用B│用C│用D│    空 剩余       │
└───┴───┴───┴───┴─────────────────┘

释放 B 和 D 后（合并相邻空闲）：
┌───┬───┬───┬─────────────────────┐
│用A│空 │用C│    空 剩余合并       │
└───┴───┴───┴─────────────────────┘
```

**每个块头**（BlockLink_t）约 8~16 字节。`pvPortMalloc(100)` 实际耗掉约 108~116 字节。

### 6.10 内存碎片化

**heap_4 减轻但不消除碎片**。经典陷阱：**大小交替分配**。

```
GuiTask 反复 malloc(50KB) → free
AudioTask 一直持有 malloc(4KB) 但地址变来变去
→ 4KB 像钉子钉在中间
→ 反复几十次后：总空闲 100KB，但最大连续块只有 30KB
→ GuiTask 想 malloc 50KB 图片 → 失败！
```

`xPortGetFreeHeapSize()` 返回**总空闲**，不是**最大连续块**——空闲很多但分配失败，就是碎片化征兆。

**避免碎片的三招**：
1. **启动时分配一次**，运行期尽量不 malloc/free
2. **对象池**：预分配一批同大小对象重用
3. **静态大小的池**：`heap_5` 或自己写内存池

### 6.11 heap 关键 API

```c
size_t xPortGetFreeHeapSize(void);            // 当前剩余
size_t xPortGetMinimumEverFreeHeapSize(void); // 历史最少剩过多少
```

MonitorTask 里定期打印，如果 `minimum` 接近 0 → **危险信号**。

### 6.12 malloc 失败钩子

```c
#define configUSE_MALLOC_FAILED_HOOK  1

void vApplicationMallocFailedHook(void) {
    printf("MALLOC FAILED!\n");
    __disable_irq();
    while (1) {}
}
```
不开这个钩子，`pvPortMalloc` 失败只是返回 NULL——如果代码没检查返回值就用，会立刻或稍后崩溃。

### 6.13 项目改进清单

```c
// FreeRTOSConfig.h 修改
#define configCHECK_FOR_STACK_OVERFLOW    2
#define configUSE_MALLOC_FAILED_HOOK      1

// 提供三个钩子
vApplicationStackOverflowHook
vApplicationMallocFailedHook

// MonitorTask 补齐监控
uint32_t storage_wm = uxTaskGetStackHighWaterMark(storageTaskHandle);
uint32_t audio_wm   = uxTaskGetStackHighWaterMark(audioTaskHandle);
uint32_t timer_wm   = uxTaskGetStackHighWaterMark(xTimerGetTimerDaemonTaskHandle());
size_t free_now  = xPortGetFreeHeapSize();
size_t min_ever  = xPortGetMinimumEverFreeHeapSize();
```

---
## 模块 7：静态 vs 动态分配

### 7.1 项目现状

```c
#define configSUPPORT_DYNAMIC_ALLOCATION   1   // ✅ 开着
#define configSUPPORT_STATIC_ALLOCATION    0   // ❌ 关着
```
项目所有任务、队列都用**动态**版本，从 128 KB heap 分配。

### 7.2 两种分配方式对比

**动态分配**：
```c
xTaskCreate(TaskFunc, "T", 512, NULL, 3, &handle);
// 内部：pvPortMalloc 分配 TCB + 栈 → 从 heap 里"临时借用"
```

**静态分配**：
```c
static StaticTask_t g_task_tcb;
static StackType_t  g_task_stack[512];
xTaskCreateStatic(TaskFunc, "T", 512, NULL, 3, g_task_stack, &g_task_tcb);
// 内存在编译时分配（.bss 段），FreeRTOS 只是"使用"
```

### 7.3 静态分配的六大优势

1. **不会失败**：内存在 .bss 预留，`xTaskCreateStatic` 永不返回 NULL
2. **编译时可见**：`.map` 文件里能看到每个对象的地址和大小
3. **零碎片化**：没有 heap，何来碎片
4. **更快**：无 `pvPortMalloc` 遍历开销
5. **确定性**：分配时间为零，硬实时系统必需
6. **满足行业标准**：MISRA C、DO-178C（航空）、IEC 62304（医疗）都禁用动态分配

### 7.4 静态分配的代价

1. **代码更啰嗦**（一行变三步）
2. **必须预留最大量**（永远用不到也占着 RAM）
3. **需要提供两个函数**：`vApplicationGetIdleTaskMemory` 和 `vApplicationGetTimerTaskMemory`（内核的空闲任务和 Timer Service 任务也要静态内存）

```c
void vApplicationGetIdleTaskMemory(
    StaticTask_t **ppxIdleTaskTCBBuffer,
    StackType_t **ppxIdleTaskStackBuffer,
    uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t  uxIdleTaskStack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer   = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}
```

### 7.5 混合模式（推荐）

```c
#define configSUPPORT_DYNAMIC_ALLOCATION   1
#define configSUPPORT_STATIC_ALLOCATION    1
```

**判断原则**：
- **启动后永久存在的对象** → 静态
- **偶尔创建销毁的临时对象** → 动态或**静态池**

### 7.6 静态版 API 对照表

| 动态 | 静态 |
|---|---|
| `xTaskCreate` | `xTaskCreateStatic` |
| `xQueueCreate` | `xQueueCreateStatic` |
| `xSemaphoreCreateBinary` | `xSemaphoreCreateBinaryStatic` |
| `xSemaphoreCreateMutex` | `xSemaphoreCreateMutexStatic` |
| `xSemaphoreCreateCounting` | `xSemaphoreCreateCountingStatic` |
| `xEventGroupCreate` | `xEventGroupCreateStatic` |
| `xTimerCreate` | `xTimerCreateStatic` |

**统一模式**：调用者提供两块内存——**控制结构体**（`StaticXxx_t`）+ **数据存储区**。

### 7.7 关键认知：静态 ≠ 省 RAM

**误区**：静态化能省内存
**真相**：**RAM 是 RAM，不管哪个段都占 RAM**

- 动态：`ucHeap[128 KB]` 在 .bss 里一整块预留
- 静态：各个对象在 .bss 里独立占用

**总 RAM 基本不变，只是分配位置从"heap 内部"移到".bss 独立对象"**。

**静态真正的价值**：**确定性和可靠性**——编译时可见、永不失败、零分配开销、零碎片。

**什么时候静态真的省 RAM**：
- heap 之前预留过大（比如实际用 30KB 却预留 128KB）
- 动态碎片化导致需要更大 heap 抗碎片

**什么时候静态反而占更多 RAM**：
- 预留了永远用不到的对象槽
- 互斥存在的对象组（TaskA/TaskB 不同时存在，动态可复用）

### 7.8 对象池模式（进阶）

**"看似动态但数量有上限"的对象适合池化**：

```c
#define MAX_WINDOWS 4

typedef struct {
    StaticTask_t tcb;
    StackType_t  stack[512];
    TaskHandle_t handle;
    bool         in_use;
} WindowSlot_t;

static WindowSlot_t g_window_pool[MAX_WINDOWS];

TaskHandle_t open_window(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!g_window_pool[i].in_use) {
            g_window_pool[i].in_use = true;
            g_window_pool[i].handle = xTaskCreateStatic(
                WindowTask, "W", 512, NULL, 3,
                g_window_pool[i].stack, &g_window_pool[i].tcb);
            return g_window_pool[i].handle;
        }
    }
    return NULL;
}
```

**嵌入式几乎所有对象都有上限**（不像服务器不定），所以对象池是**核心武器**。

### 7.9 项目改造建议

**5 个业务任务都改为静态**：
```c
static StaticTask_t g_gui_tcb;
static StackType_t  g_gui_stack[1024];
g_guiTaskHandle = xTaskCreateStatic(
    GuiTask, "Gui", 1024, NULL, 3,
    g_gui_stack, &g_gui_tcb);
```

**8 个队列都改为静态**（TouchEvents、AudioCommands、6 个 Storage 队列）。

**保留一小块 heap**（比如 32 KB）用于真正的动态需求（FatFs 内部、图片解码临时缓冲、未来扩展）。

**改造后附加好处**：崩溃时可从 `.map` 文件定位——SP 落在哪个静态栈的地址范围内，立刻知道是哪个任务溢出。

---

## 综合速查表

### 同步原语选型（黄金决策表）

| 问 | 答 |
|---|---|
| **一对一 vs 一对多** | 一对一→任务通知；一对多→事件组 |
| **传数据吗** | 是→队列；否→通知/信号量/事件组 |
| **周期 vs 事件触发** | 周期→软件定时器；事件→任务+队列/通知 |
| **保护的资源短 vs 长** | 短→临界区；长→互斥量 |
| **需要广播吗** | 是→事件组（唯一选择） |
| **等待多个条件都满足** | 事件组（唯一支持"等全部"） |
| **ISR 里通知** | 优先任务通知（最快） |

### 常用超时值

| 场景 | 超时 |
|---|---|
| 请求转发（快速失败） | `0` |
| 生产者防丢消息 | `pdMS_TO_TICKS(100)` |
| 消费者事件驱动 | `portMAX_DELAY` |
| 服务型任务主循环 | `portMAX_DELAY` |
| 需要周期性做事 | `pdMS_TO_TICKS(x)` |

### 项目现存隐患清单

| 隐患 | 位置 | 改进 |
|---|---|---|
| 栈溢出检测关闭 | `configCHECK_FOR_STACK_OVERFLOW = 0` | 改为 2 |
| malloc 失败静默 | `configUSE_MALLOC_FAILED_HOOK = 0` | 改为 1 |
| 静态分配未启用 | `configSUPPORT_STATIC_ALLOCATION = 0` | 改为 1（混合模式） |
| 栈水印监控不全 | MonitorTask 仅监控 3 个任务 | 补齐 StorageTask、AudioTask、TimerTask |
| printf 并发无锁 | 多任务共用 USART1 | 加互斥量或改用队列到日志任务 |

### 关键 API 速记

**任务**：
```c
xTaskCreate / xTaskCreateStatic
vTaskDelay / vTaskDelayUntil
vTaskSuspend / vTaskResume / vTaskDelete
xTaskNotify / xTaskNotifyWait / xTaskNotifyFromISR
uxTaskGetStackHighWaterMark
```

**队列**：
```c
xQueueCreate / xQueueCreateStatic
xQueueSend / xQueueReceive
xQueueSendFromISR / xQueueReceiveFromISR
uxQueueMessagesWaiting
```

**互斥量/信号量**：
```c
xSemaphoreCreateMutex / xSemaphoreCreateBinary / xSemaphoreCreateCounting
xSemaphoreTake / xSemaphoreGive
xSemaphoreGiveFromISR
```

**软件定时器**：
```c
xTimerCreate / xTimerStart / xTimerStop / xTimerReset / xTimerChangePeriod
```

**事件组**：
```c
xEventGroupCreate
xEventGroupSetBits / xEventGroupClearBits / xEventGroupWaitBits
xEventGroupSync
```

**内存**：
```c
xPortGetFreeHeapSize / xPortGetMinimumEverFreeHeapSize
```

**临界区**：
```c
taskENTER_CRITICAL / taskEXIT_CRITICAL             // 任务中
taskENTER_CRITICAL_FROM_ISR / taskEXIT_CRITICAL_FROM_ISR   // ISR 中
```

---

## 核心洞察汇总

**1. 优先级不是重要性，而是"响应延迟敏感度"**
高优先级任务必须频繁睡眠让位。经常睡眠的任务给高优先级是安全的。

**2. Cortex-M 优先级：数字越小越高**
`configMAX_SYSCALL_INTERRUPT_PRIORITY` 是分界线，`≥` 门槛的中断才能调 FromISR。

**3. 阻塞是 RTOS 的核心价值**
任务可以睡等事件，不用轮询。这是相比裸机的最大优势。

**4. "最好的锁是不需要锁"**
架构层用**串行化任务**消除并发，比到处加锁更优雅。

**5. 优先级继承是互斥量的独家能力**
二值信号量没有所有者概念，做不到优先级继承。

**6. 短用临界区，长用互斥量**
临界区关中断 ≤10 μs；长时保护用互斥量（可阻塞）。

**7. 定时器服务是单车道**
回调不能阻塞，否则所有定时器全瘫。

**8. 一对一用任务通知，一对多用事件组**
广播和"等待所有条件"是事件组的独家能力。

**9. 静态分配的价值是确定性，不是省内存**
RAM 总量不变，但编译时可见、永不失败、零分配开销、零碎片。

**10. 嵌入式黄金公式**
```
最专业方案 = 编译期确定 + 对象池
= 静态分配核心对象 + 对可变数量对象用固定大小池
```

---

## 模块 8：tickless idle 低功耗

### 8.1 项目现状

```c
#define configUSE_TICKLESS_IDLE  0   // 未开启低功耗
```

### 8.2 为什么标准 tick 模式耗电

**SysTick 每 1 ms 中断一次**，即使所有任务都在阻塞睡眠，CPU 也会被强制唤醒：

```
0ms:   SysTick 中断 → CPU 从 Sleep 唤醒
       → 中断服务函数（保存寄存器 + tick 计数）
       → 调度器扫描延时列表 → 没人到期
       → 空闲任务 __WFI() → CPU 回到 Sleep
1ms:   重复上述过程...
```

**结果**：即使系统"空闲"，功耗仍是满载的 20%~40%。

### 8.3 tickless 核心思想

**"没人干活就睡到有事再叫醒，而不是每 1 ms 醒来查一次"**

启用后：
1. 空闲任务扫描延时列表，找出"最近要醒的任务"（比如 500 ms 后）
2. 关掉 SysTick
3. 配置 RTC/LPTIM 在 500 ms 后触发中断
4. CPU 进入 Sleep 或 Stop 模式
5. 唤醒后重启 SysTick + 补偿 tick 计数

**收益**：Stop 模式下功耗降到满载的 1%~5%。

### 8.4 三种睡眠深度

| 模式 | 关闭 | 保留 | 唤醒延迟 | 功耗 | 适合 |
|---|---|---|---|---|---|
| **Sleep** | CPU 时钟 | 所有外设、SRAM | ~100 ns | ~30% | 频繁唤醒 |
| **Stop** | CPU + 大部分外设时钟 + HSE | SRAM、RTC、LSI/LSE | 几毫秒 | ~1% | 秒级睡眠 |
| **Standby** | 几乎一切 | RTC、备份寄存器 | 几十毫秒 | ~0.01% | 小时级、按键唤醒 |

**Standby 等同于重启**：唤醒需要重新初始化整个系统。

### 8.5 三大陷阱

**陷阱 1：时间精度下降**
- Stop 模式用 LSI/LSE（32.768 kHz）计时，精度 ±1%~3%
- `vTaskDelay(10)` 可能变成 9~11 ms
- 音频采样、通信协议时序严格的场景**不能容忍**

**陷阱 2：外设时钟停止**
- Stop 模式下 DMA、USART、SPI 全部冻结
- 传输中的数据会**中断或丢失**
- 你项目 AudioTask + DMA 持续跑，进 Stop 会导致音频断声

**陷阱 3：调试困难**
- Sleep/Stop 下调试器可能断连
- 需要 `DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP` 让调试时不进低功耗
- 开发期建议先关 tickless，后期再优化

### 8.6 适用性判断（四条判据）

| 判据 | 适合开 | 不适合 |
|---|---|---|
| 电源类型 | 电池供电 | 外接电源 |
| 外设活动 | 稀疏（每 10 分钟一次） | 持续（音频、DMA） |
| 时间精度 | 秒级、分钟级 | 微秒/毫秒级严格 |
| 唤醒延迟 | 可容忍几毫秒 | 要求即时响应 |

**你项目**：**四条全不适合**——USB 供电、持续音频 DMA、44.1 kHz 严格音频采样、触摸屏要求即时响应。当前 `configUSE_TICKLESS_IDLE = 0` **正确**。

### 8.7 典型低功耗产品的配置

**CR2032 电池 + 每 10 分钟采温度上报（如智能家居温湿度传感器）**：

```c
#define configUSE_TICKLESS_IDLE          1
#define configSUPPORT_STATIC_ALLOCATION  1   // 完全静态，无 heap
```

**10 分钟一次的活动分解**：

| 阶段 | 时长 | 电流 |
|---|---|---|
| Stop 深睡 | 599.9 s | ~1 μA |
| 唤醒 + 采样 | 0.05 s | ~3 mA |
| BLE 广播 | 0.05 s | ~8 mA |

**平均电流 ~2 μA** → CR2032（220 mAh）能用 **3~5 年**（考虑自放电）。

这是米家温湿度计、Aqara 传感器、蓝牙防丢器等主流 IoT 传感器的标准架构。

### 8.8 变通方案：不开 tickless 也能省电

如果项目大部分时间有活动、但偶尔要长时间空闲：

**方案 A：分段手动进入低功耗**
```c
if (idle_for_30s) {
    stop_audio();
    lcd_backlight_off();
    HAL_PWR_EnterSTOPMode(...);   // 手动进 Stop，触摸中断唤醒
}
```

**方案 B：动态调整 tick 频率**
```c
if (system_idle) {
    SysTick_Config(SystemCoreClock / 100);  // 降到 100 Hz
} else {
    SysTick_Config(SystemCoreClock / 1000); // 恢复 1000 Hz
}
```

### 8.9 验证 tickless 是否生效

**方法 1：GPIO + 示波器**
```c
void vPortSuppressTicksAndSleep(...) {
    HAL_GPIO_WritePin(DEBUG_PIN, HIGH);   // 拉高标记
    HAL_PWR_EnterSTOPMode(...);
    HAL_GPIO_WritePin(DEBUG_PIN, LOW);    // 拉低
}
```

**方法 2：串一个电流表**测 VDD 平均电流

**方法 3：STM32CubeMonitor-Power** 官方工具，可视化各模式占比

### 8.10 你项目为什么不用开 tickless

四大理由（前面已讲）：
1. **持续音频 DMA** → 没有真正空闲
2. **触摸每 5 ms 扫描** → 深睡收益极小
3. **USB 供电** → 功耗不敏感
4. **触摸即时响应** → 唤醒延迟会显著卡顿

**记忆口诀**：
> **"电池驱动的物联网传感器才是 tickless 的主场，多媒体交互设备就别折腾了"**

---

## 最终核心洞察（补充第 11 条）

**11. 低功耗设计的本质是让 CPU 尽可能长时间不干活**
tickless 是让 CPU 有权"深睡"的钥匙，Stop 模式是深睡的深度选择。**选对场景能耗差 100 倍，选错场景功能全崩**。判断标准：电源类型 + 外设活动 + 时间精度 + 唤醒延迟。

---

## 学习完结 🎓

**8 个模块全部完成！**

已掌握能力：
- ✅ 看懂 FreeRTOS 项目架构和配置
- ✅ 选择合适的同步原语
- ✅ 判断优先级设计合理性
- ✅ 诊断优先级反转、竞态条件、内存碎片、栈溢出
- ✅ 用串行化任务模式消除并发
- ✅ 评估项目是否适合低功耗
- ✅ 审查 FreeRTOSConfig.h 关键配置

**推荐下一步**：
1. 动手改进本项目的隐患配置（栈溢出检测、malloc 钩子、静态分配）
2. 阅读 FreeRTOS 源码（tasks.c / queue.c / timers.c）
3. 用小板子做一个 CR2032 供电的低功耗项目，验证 tickless 实际效果
4. 学习 Trace 工具（Tracealyzer/SystemView）可视化任务调度





