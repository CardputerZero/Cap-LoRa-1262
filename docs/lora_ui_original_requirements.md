# LoRa 页面原始需求单

本文根据 APPLaunch 的 `UILoraPage` 声明及其实现整理，来源文件为：

- `/home/nihao/w2T/github/launcher/projects/APPLaunch/main/ui/page_app/ui_app_lora.hpp`
- 同目录的 `ui_app_lora.cpp` 和 `ui_app_lora_view.cpp`

## 1. 页面与生命周期

- 页面标题为 `LoRa`，内容区域为 320×150，背景色 `#0B0C0E`。
- 页面必须创建 Messages、Info、Send 三个视图，以及底部双圆点页面指示器。
- 创建完成后检查所有关键控件句柄；缺少任何控件时删除根对象并停用页面。
- 根对象注册键盘和删除事件。对象删除时清空对应句柄；根对象删除时清空全部句柄并停止轮询。
- 页面析构时解绑事件、取消动画、删除定时器、停止 LoRa 后端、请求初始化线程取消并等待线程退出。

## 2. Messages 视图

- 消息列表为 320×150 的纵向 flex 容器，左右内边距 10，上 20，下 10，行间距 8，允许竖向滚动且隐藏滚动条。
- 空列表显示 `No messages yet` 和 `Type anything to send`。
- 页面顶部显示 `Messages` HUD：初始位置 y=-8，停留 3200ms，再用 340ms 缓动移动到 y=-29 并隐藏；收到真实消息或用户滚动时立即关闭。
- 消息历史最多 64 条，达到上限后删除最早的列表行。
- 新消息追加后隐藏空状态并滚动到最新行；当前不在 Messages 视图时先记录待滚动状态，返回后无动画滚动到底部。
- 发送消息气泡为绿色 `#3FCC75`，接收消息气泡为灰色 `#CCCCCC`，圆角 8，并绘制宽 6、下垂 3 的三角尾巴。
- 气泡宽度由文本和接收元数据计算，范围为 64 至 244；水平内边距 10，上下内边距 7。
- 接收消息显示 `RSSI dBm / SNR dB` 元数据，发送消息不显示元数据；消息文本为黑色 Montserrat 12。

## 3. Info 视图

- 标题为 `LoRa Info`，显示状态圆点、状态文本和 `CLIENT` 标识。
- 显示 DEVICE、RSSI、SNR、LINK 和诊断文本，并用两条分割线分隔区域。
- 状态优先级为：初始化中 `INITIALIZING`（`#C9A45C`）；硬件未就绪 `RADIO OFF`（`#D96C6C`）；发送中 `SENDING`（`#C9A45C`）；发送模式 `TX MODE`（`#C9A45C`）；其余为 `RECEIVING`（`#69AD80`）。
- 设备、链接和诊断为空时分别显示 `Unavailable`、`Link configuration unavailable`、`No diagnostics`。
- RSSI 格式为 `%.0f dBm`，SNR 格式为 `%.1f dB`；设备、链接和诊断长文本使用点号省略模式。
- 诊断文本颜色在初始化中为黄色，硬件就绪为灰色 `#777B82`，失败为红色 `#D96C6C`。

## 4. Send 视图

- 标题为 `New Message`。
- 中央输入气泡尺寸 286×80，背景 `#555555`，圆角 8；输入文本显示当前内容并追加光标 `|`。
- 状态文本默认隐藏，右对齐并使用黄色；按钮为 `ESC: Cancel`（灰色）和 `Enter: Send`（黄色）。
- 初始化中发送显示 `LoRa is still initializing`；硬件不可用显示 `LoRa unavailable`；空消息显示 `Message is empty :(`；发送失败显示 `Send failed`。
- 发送成功调用 `SendText`，刷新 Info，追加发送气泡，然后返回 Messages 视图。

## 5. LoRa 初始化与轮询

- 进入页面时清空消息，显示 `Initializing LoRa...`，诊断文本显示硬件初始化中，并将模型置于不可用状态。
- 后台线程严格按 `Init → Info → StartReceive` 执行；只有初始化成功且硬件就绪时才启动接收。
- 轮询定时器周期为 300ms。初始化未完成时消费线程结果；初始化失败时显示 `LoRa unavailable; see Info`，并至少等待 3 秒后重试。
- 硬件就绪后调用 Poll；收到 `rx_event` 时追加接收消息并刷新 RSSI/SNR 等 Info 字段。

## 6. 键盘与导航

- 物理方向键、Enter/KPEnter、Esc、Backspace、Delete 映射到 LVGL 键值；释放事件不处理。
- Shift、Ctrl、Alt、Meta、Fn 等修饰键以及对应符号名称全部忽略。
- 在非 Send 视图中，F/f 和 X/x 分别等价于上/下；左键、Prev、Z 返回 Messages，右键、Next、C 进入 Info。
- Messages 视图上下键每次滚动 36；其他视图上下键在 Messages 与 Info 间切换。
- 初始化完成且硬件就绪时，Enter 进入 Send；非 Send 视图中的可打印字符可直接开始输入，但 Z/C 保留为导航键。
- Send 视图中 Esc 取消，Backspace/Delete 删除字符，Enter 发送，可打印 ASCII 追加到输入，输入上限 127 字节。
- 页面外的 Esc、Backspace、Delete 调用返回主页。

## 7. LVGL 调用覆盖范围

测试文件 `tests/lora_page_ui_contract_test.cpp` 保存了 UILoraPage 使用的全部 LVGL 调用名称清单，并固定了页面尺寸、文案、颜色相关常量、历史和输入上限、轮询/重试周期、HUD/视图动画时间、导航规则及气泡宽度规则。该测试不实例化硬件页面，因此 SDL 和设备构建均可执行。
