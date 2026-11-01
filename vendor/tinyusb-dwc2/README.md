# DWC2 端点传输与并发处理

`dcd_dwc2.c` 基于 TinyUSB 0.16.0，提交 `1eb6ce784ca9b8acbbe43dba9f1d9c26c2e80eb0`，保留上游 MIT 许可和版权声明。其余 DWC2 头文件来自相同提交的官方依赖目录，USB 类驱动由 Arduino ESP32 2.0.17 提供，DCD 函数签名保持 TinyUSB 0.16.0 ABI。

移植依据为 [TinyUSB PR #3275](https://github.com/hathach/tinyusb/pull/3275)，合并提交 `6daf07bb92d7c7f8204852d80942362d7cea3ce0`，以及 [端点激活状态修复](https://github.com/hathach/tinyusb/commit/1f662779c44dd913f2c26f9bddc4f37ee70f16dc)。寄存器访问使用 0.16.0 的宏和结构体表达相同操作。

启用 ISO incomplete 中断后，驱动根据端点类型、启用状态以及当前帧和目标帧的奇偶值定位未完成的 ISO IN 传输。每次提交将重试次数设置为端点 interval；发生未完成传输时重新设置长度、包数和下一帧奇偶值。重试耗尽后停止 FIFO empty 中断，禁用当前传输并清理发送 FIFO，保留端点的配置及 USBAEP 激活状态，报告零字节的 `XFER_RESULT_FAILED`。USB 类驱动负责在接口仍启用时提交后续音频。

当前设备使用 Full Speed、`bInterval=1`、16 kHz 单声道 16-bit 音频，每次传输 32 字节。每次提交最多重试一次。传输失败保留 `max_size` 和 FIFO 分配，不调用回收 FIFO 的 `dcd_edpt_close()`。

发送路径依据以下官方提交移植跨核同步处理：

- [DIEPEMPMSK 读改写同步](https://github.com/hathach/tinyusb/commit/d2b6142bc34b0e10ffd9f8a21634f1fa0c54139e)：提交传输时设置 TXFE 使能，ISR 写入完成后读取剩余长度并清除 TXFE 使能，两处操作使用同一个临界区，防止覆盖其他端点的使能位。
- [发送 FIFO 写入与 flush 同步](https://github.com/hathach/tinyusb/commit/ec7be1ed1d74de8929782c5dfc1f358a2b088db8)：发送 FIFO 写入、传输长度检查及 FIFO flush 互斥执行，防止处理器核心同时操作 FIFO。
- [GINTMSK 与 DAINTMSK 读改写同步](https://github.com/hathach/tinyusb/commit/e42fa93570a672d4779b7a3d0e14145a3ae919e6)：任务与 ISR 修改中断使能寄存器时使用相同临界区。

当前 IDF 4.4 提供 `portENTER_CRITICAL_SAFE()` 和 `portEXIT_CRITICAL_SAFE()`，根据任务或 ISR 上下文选择对应操作。本地驱动使用一个 `portMUX_TYPE`，保护端点状态、传输调度、FIFO 操作和中断使能寄存器。内部调度与禁用函数由调用方持有临界区；完成事件在退出临界区后发送。`dcd_edpt_open()` 和 `dcd_edpt_close()` 使用入口包装，内部断言返回后仍会退出临界区；向已关闭端点提交传输会返回 `false`。

单端点关闭处理针对 TinyUSB 0.16.0 ABI 实现：确认 FIFO 回收顺序后，屏蔽目标端点的 DAINTMSK 和 TXFE 使能，禁用端点并清理其发送 FIFO，清除 USBAEP 和待处理的端点中断，清空传输状态，随后回收该端点的 FIFO 空间。IN 中断处理仅检查 DAINT 与 DAINTMSK 同时启用的端点。关闭过程保留其他端点的使能位与 FIFO 内容。该处理为本地的端点关闭保护；当前上游使用 ISO FIFO 预分配和 `dcd_edpt_iso_activate()` 接口，未直接采用此单端点关闭实现。

`iso_diagnostics.h` 导出三个从设备启动开始累计的计数：

- `remotemapper_dwc2_iso_incomplete_count()`：通过硬件端点状态与帧奇偶检查的未完成传输次数。
- `remotemapper_dwc2_iso_retry_count()`：实际重新安排下一帧传输的次数。
- `remotemapper_dwc2_iso_failed_count()`：重试耗尽并报告传输失败的次数。

计数增量和读取使用 32-bit relaxed 原子操作，编译期要求无锁，ISR 内不输出日志。三个接口分别读取计数，不提供同时读取三个计数的快照。

`remotemapper_dwc2_diagnostics()` 在同一个临界区内读取 DAINT、DAINTMSK、DIEPEMPMSK，以及各 IN 端点的控制、中断、传输长度寄存器和 `max_size`，供实际 HID 与音频启停检查使用。硬件仍会更新寄存器，该接口提供一次连续读取的诊断结果。
