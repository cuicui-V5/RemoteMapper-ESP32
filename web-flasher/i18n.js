/**
 * RemoteMapper-ESP32 双语国际化支持 (Bilingual i18n: 中文 / English)
 * 自动识别浏览器语言、支持用户手动切换、数据持久化存储
 */

const TRANSLATIONS = {
  'zh-CN': {
    // 导航
    'nav.brand_tag': '硬件级蓝牙遥控器转客制化 USB 复合设备',
    'nav.comparison': '方案优势',
    'nav.features': '核心特性',
    'nav.topology': '系统拓扑',
    'nav.flasher': '刷机方式',
    'nav.tutorial': '使用教学',
    'nav.cta': '🚀 立即使用',

    // Hero 首屏
    'hero.badge': '✨ 2026 全新升级 · 网页端 Web Serial 免驱一键刷机已上线',
    'hero.title': 'Windows 下小米蓝牙遥控器<br><span class="gradient-text">最佳物理级硬件桥接方案</span>',
    'hero.subtitle': '基于 ESP32-S3 原生 USB OTG。彻底告别 Windows 内核丢键与虚拟声卡高延迟难题，将遥控器信号直接重映射为标准免驱物理麦克风（UAC 1.0）与 HID 键盘/多媒体设备。即插即用，游戏反作弊完全免疫。',
    'hero.btn_flasher': '立刻使用 (选择刷机方式)',
    'hero.btn_tutorial': '查看配置教学',
    'hero.btn_star': '⭐ Star 关注项目',
    'hero.pill_uac': '免驱 UAC 1.0 物理麦克风',
    'hero.pill_keys': '13 个物理按键 100% 空口捕获',
    'hero.pill_macros': '5 层独立宏映射 (单/双/长按)',
    'hero.pill_wol': '局域网 WOL 远程开机',
    'hero.pill_web': '内置可视化 Web 控制台',

    // 方案对比
    'comp.tag': 'Direct Comparison',
    'comp.title': '告别传统纯软件方案的妥协与痛点',
    'comp.subtitle': 'Windows 蓝牙驱动内核层过滤机制导致纯软件 Hook 方案无法完美兼容，RemoteMapper 采用硬件物理中继方案实现真正意义上的完美适配。',
    'comp.sw_title': '传统纯软件直连方案',
    'comp.sw_1': '<strong>部分按键被系统内核丢弃</strong>：遥控器的「音量+ (0x80)」、「音量- (0x81)」、「返回 (0xF1)」被 Windows 自带蓝牙驱动（`kbdhid.sys`）过滤丢弃，应用层 Hook/AHK 完全截获不到。',
    'comp.sw_2': '<strong>语音依赖虚拟声卡 (VB-Cable)</strong>：私有 ATVV 压缩音频必须二次路由转存，引入明显音频延迟且经常出现声道占用或采样率不匹配。',
    'comp.sw_3': '<strong>安全与反作弊风险</strong>：如需过滤驱动必须强制开启 Windows 测试模式，极易触发各类竞技游戏（如瓦罗兰特、APEX、CF等）的反作弊封禁。',
    'comp.sw_4': '<strong>开机与睡眠唤醒无解</strong>：PC 关机或深度睡眠时蓝牙接收器关闭，无法通过遥控器进行一键开机。',
    'comp.hw_title': 'RemoteMapper-ESP32 硬件桥接',
    'comp.hw_1': '<strong>13 个物理按键 100% 完整捕获</strong>：ESP32-S3 在空口通过 BLE 协议栈完整接收所有原始报文，彻底绕过 Windows 内核驱动过滤。',
    'comp.hw_2': '<strong>免驱动硬件级 USB 麦克风 (UAC 1.0)</strong>：芯片内置 IMA-ADPCM 实时流解码，电脑识别为物理麦克风，支持语音按下瞬间自动注入快捷键与按键杂音智能消隐。',
    'comp.hw_3': '<strong>即插即用 · 反作弊 100% 免疫</strong>：模拟标准免驱 HID 键盘与麦克风，无需在电脑端运行任何常驻后台或安装内核驱动，与原生物理外设无异。',
    'comp.hw_4': '<strong>局域网 WOL 无线远程唤醒</strong>：关机或睡眠状态下，按下遥控器按键即可向电脑发送 Magic Packet 魔法包，实现沙发上一键无线开机。',

    // 核心特性
    'feat.tag': 'Core Features',
    'feat.title': '专为极致桌面体验打造的核心能力',
    'feat.subtitle': '从底层音频处理、多层按键状态机，到内置 Web 管理系统，每一个细节都经过精心打磨。',
    'feat.f1_title': '免驱 UAC 1.0 麦克风',
    'feat.f1_desc': '模拟 16kHz 16-Bit 单声道硬件级麦克风。支持按下瞬间唤起输入法快捷键，并在前段加入 150ms 杂音消隐区与 10ms 淡入平滑，彻底杜绝按键微动爆音。',
    'feat.f2_title': '5 层独立宏映射系统',
    'feat.f2_desc': '默认层 + 4 个自定义层。每个按键均支持单击、长按（200~2000ms）与双击（100~600ms），具备穿透继承、一次性层、闲置超时自动返回与 RGB 状态指示灯。',
    'feat.f3_title': '局域网远程开机 (WOL)',
    'feat.f3_desc': '可将任意按键（甚至长按）设置为局域网网络唤醒魔术包（Magic Packet）。无论电脑处于关机（S5）还是睡眠状态，轻按遥控器即可无线开机。',
    'feat.f4_title': '多媒体直通与连续长按',
    'feat.f4_desc': '完美支持标准 Consumer 媒体键码。音量增加/减少支持长按持续平滑调节，播放暂停、曲目切换原生响应，无需手动点按多次。',
    'feat.f5_title': '内置可视化 Web 控制台',
    'feat.f5_desc': 'ESP32 固件集成 Web 服务器，自带 AP 热点（RemoteMapper-AP）与 STA 局域网接入。手机或电脑打开网页即可修改按键映射，配置存储于 NVS 掉电不丢。',
    'feat.f6_title': '全系 ESP32-S3 硬件支持',
    'feat.f6_desc': '开箱兼容市场上主流 ESP32-S3-WROOM-1 开发板，包括 N16R8（16MB Flash/8MB PSRAM）、N8R8、N8R2、N4R2 等，刷机网页自动智能匹配。',

    // 系统拓扑
    'topo.tag': 'System Topology',
    'topo.title': '纯粹而高效的硬件拓扑架构',
    'topo.subtitle': '遥控器与电脑之间由 ESP32-S3 充当中转枢纽，实现蓝牙空口到 USB 复合设备的高速转换。',

    // 刷机方式
    'flasher.tag': 'Flashing Methods',
    'flasher.title': '选择适合您的刷机方式',
    'flasher.subtitle': '项目提供了三种不同的刷机途径，<strong>强烈推荐优先使用网页端免驱动在线刷机</strong>，打开独立网页插线即刷，15 秒完成！',
    'flasher.m1_badge': '🌟 官方首选推荐 · 零环境',
    'flasher.m1_title': '网页端免驱动在线刷机',
    'flasher.m1_desc': '直接在浏览器中通过 Web Serial 协议对开发板烧录。无需安装 Python、Git、PlatformIO 或任何驱动，插上数据线一键写入。',
    'flasher.m1_f1': '<strong>自动识别硬件型号</strong>：智能探测 Flash 与 PSRAM 自动匹配固件型号',
    'flasher.m1_f2': '<strong>智能判断全新/升级</strong>：自动检测是否已刷过本项目，新板自动选全量，老用户自动保留已有 Wi-Fi 与按键配置升级',
    'flasher.m1_f3': '<strong>超高传输速率</strong>：支持 460800 / 921600 波特率，全程仅需约 15 秒',
    'flasher.m1_tip': '💡 刷机前小提示：请将 Type-C 线插在开发板 <strong>USB 口</strong>，按住 <strong>BOOT</strong> 键按一下 <strong>RST</strong> 键松开，进入刷机模式后再连接。',
    'flasher.m1_btn': '⚡ 打开网页端在线刷机平台',
    'flasher.m2_badge': '📦 离线环境备选',
    'flasher.m2_title': '本地绿色免安装刷机包',
    'flasher.m2_desc': '适合无网络或受限环境。下载预编译绿色工具包，双击交互向导脚本一键刷机。',
    'flasher.m2_f1': '内置乐鑫官方 esptool 独立执行程序',
    'flasher.m2_f2': '全型号预编译固件（N16R8 / N8R8 / N8R2 / N4R2）',
    'flasher.m2_f3': '全中文批处理交互向导 (`一键烧录.bat`)',
    'flasher.m2_btn': '前往 GitHub Releases 下载 (.7z / .zip)',
    'flasher.m3_badge': '💻 极客与开发者专属',
    'flasher.m3_title': 'PlatformIO 源码自主编译',
    'flasher.m3_desc': '适合二次开发者。自定义蓝牙广播过滤逻辑、按键状态机扩展与板级引脚重定向。',
    'flasher.m3_f1': '完整 C/C++ 源码与 PlatformIO 编译工程',
    'flasher.m3_f2': '一键脚本编译所有型号：`build.bat all`',
    'flasher.m3_f3': '直接指定板型线刷：`flash.bat n16r8`',
    'flasher.m3_btn': '查看 GitHub 仓库源码',

    // 使用教程
    'tut.tag': 'Step-by-Step Guide',
    'tut.title': '刷机完成后快速上手指南',
    'tut.subtitle': '仅需简单 5 步，轻松完成蓝牙遥控器配对、Wi-Fi 局域网接入与按键映射定制。',
    'tut.s1_title': '手动按【RST】按键重启设备',
    'tut.s1_desc': '固件写入完成后芯片仍处于 Bootloader 状态。<strong>必须按一下开发板上的物理【RST】按键</strong>（或重新拔插一次 USB 线），板载 RGB 指示灯亮起即表示已成功开机运行。',
    'tut.s2_title': '连接热点 RemoteMapper-AP',
    'tut.s2_desc': '使用手机或电脑打开 Wi-Fi 列表，搜索并连接到名为 <strong><code>RemoteMapper-AP</code></strong> 的热点（全新刷机默认无密码）。',
    'tut.s3_title': '打开内置 Web 控制台',
    'tut.s3_desc': '在手机或电脑浏览器中打开管理后台地址。整个配置流程纯网页端操作，无需下载任何 App 或驱动。',
    'tut.s3_note': '可在后台「网络设置」中将设备连入家庭 Wi-Fi 局域网',
    'tut.s4_title': '蓝牙遥控器一键配对',
    'tut.s4_desc': '同时按住小米蓝牙遥控器 2 Pro 上的 <strong>【主页键】+【菜单键】</strong>（长按约 3 秒直至听到滴声），在网页后台点击「开始配对」，ESP32 将自动搜索并绑定遥控器 MAC 地址。',
    'tut.s5_title': '配置按键映射与语音输入联动',
    'tut.s5_desc': '在后台「按键映射」页面，为单击、长按、双击分配键盘按键、快捷键组合、Consumer 媒体控制（音量/播放）或 WOL 网络开机。配置保存后立即生效并永久写入 NVS 分区。',

    // FAQ
    'faq.tag': 'FAQ',
    'faq.title': '常见问题与排查',
    'faq.q1': '问：点击“连接开发板”后弹出的串口列表中找不到设备，或连接提示握手超时？',
    'faq.a1': '答：① 请确保连接的是开发板标有【USB】(Native USB) 的 Type-C 接口，不要插在 COM/UART 接口；<br>② 必须使用<strong>具备数据传输功能</strong>的 Type-C 数据线（纯充电线无法通信）；<br>③ <strong>手动进入 Bootloader 模式（最关键）</strong>：用手指按住开发板上的【BOOT】键不放，按一下【RST】按键后松开，最后松开【BOOT】键，然后重新点击连接。',
    'faq.q2': '问：烧录完成后如何让设备正常运行？',
    'faq.a2': '答：<strong>烧录完成后必须手动重启！</strong>因为绝大部分 ESP32-S3 开发板硬件设计上没有自动退出 Bootloader 的电路。因此烧录完成后请<strong>直接按一下开发板上的【RST】按键</strong>（或重新拔插一次 USB 数据线），设备即可退出刷机状态并正常开机启动。',
    'faq.q3': '问：我的开发板是 N16R8 还是 N8R8，选错会怎么样？',
    'faq.a3': '答：网页刷机平台已实现硬件 JEDEC Flash ID 与 eFuse 自动探测！连接设备后会自动为您匹配最适型号。若 Flash 容量不匹配，系统会给出警告以防止死机。',
    'faq.q4': '问：升级固件后之前的配置和按键映射会丢失吗？',
    'faq.a4': '答：在网页刷机平台选择<strong>「保留配置升级」</strong>模式即可！该模式只会写入位于 0x10000 的固件应用程序区，完全避开 0x9000 的 NVS 存储区，您的 Wi-Fi 配置、热点密码、按键映射以及遥控器配对记录会完好无损地保留。',

    // 页脚
    'footer.gh': 'GitHub 仓库',
    'footer.rel': 'Releases 固件发布',
    'footer.issues': '提交 Issue 反馈',
    'footer.flasher': '网页在线刷机',
    'footer.copy': 'RemoteMapper-ESP32 • 开源蓝牙遥控器转客制化键盘固件 • MIT License',

    // 刷机平台专用 (flash.html)
    'flash.brand_subtitle': '免驱动网页端一键刷机平台 (Web Serial Flasher)',
    'flash.nav_home': '🏠 返回项目官网',
    'flash.serial_ready': 'Web Serial API 就绪',
    'flash.unsupported': '当前浏览器不支持 Web Serial 串口通信。请使用 <strong>Chrome、Edge、Brave 或 Opera 浏览器（PC 端）</strong> 访问本页面以进行线刷。',
    'flash.step1_title': '1. 设备连接与串口通信',
    'flash.status_disconnected': '未连接',
    'flash.status_connected': '已连接',
    'flash.guide_badge': '必须操作',
    'flash.guide_title': '请务必先手动进入刷机模式 (Bootloader)',
    'flash.step1_s1': '用具备数据传输功能的 Type-C 数据线连接开发板的 <strong>USB 口</strong>（双口开发板请勿插在 COM/UART 口）。',
    'flash.step1_s2': '<strong>操作按键组合手势进入刷机模式：</strong><br>① 按住开发板上的 <strong>【BOOT】</strong> 键不松手；<br>② 点按一下 <strong>【RST】</strong> 重启键并松开；<br>③ 最后松开 <strong>【BOOT】</strong> 键。<br><span style="font-size:0.75rem; color:#f59e0b;">（★ 大部分 ESP32-S3 开发板无法自动切换状态，必须按此物理组合键才能被识别）</span>',
    'flash.step1_s3': '完成按键操作后，点击下方 <strong>「连接开发板」</strong> 并在弹窗列表中选中对应的串口。',
    'flash.baud_label': '烧录通信波特率',
    'flash.btn_connect': '连接开发板 (Connect)',
    'flash.btn_disconnect': '断开连接',
    'flash.hw_chip': '芯片型号',
    'flash.hw_flash': 'Flash 容量',
    'flash.hw_psram': 'PSRAM 规格',
    'flash.hw_mac': 'MAC 地址',
    'flash.step2_title': '2. 固件匹配与刷机模式',
    'flash.match_badge': '已自动识别型号',
    'flash.model_label': '硬件型号版本',
    'flash.mode_full_name': '全量全新刷机 (出厂完整刷机)',
    'flash.mode_full_desc': '烧录 bootloader + 分区表 + 应用程序完整镜像 (0x0)。适合新买的空白芯片或希望彻底出厂重置。',
    'flash.badge_full': '全新板推荐',
    'flash.mode_upg_name': '保留配置升级 (升级固件)',
    'flash.mode_upg_desc': '仅刷入应用程序区 (0x10000)，跳过 0x9000 NVS 存储区，完整保留已有 Wi-Fi、蓝牙配对与按键映射！',
    'flash.badge_upg': '老用户推荐',
    'flash.chk_erase': '刷机前全盘擦除 Flash (Erase All，解决疑难损坏与存储冲突)',
    'flash.step3_title': '3. 执行固件烧录',
    'flash.step3_desc': '烧录过程大约需要 10~25 秒。<strong>★ 烧录完成后，请手动按一下开发板上的【RST】重启按键启动设备！</strong>',
    'flash.btn_flash': '立即开始刷机',
    'flash.console_title': '通信终端控制台 (Console Log)',
    'flash.btn_clear': '清空日志',
    'flash.faq_title': '💡 常见问题与排查指南',
    'flash.success_title': '🎉 固件烧录成功！请按【RST】重启开发板',
    'flash.success_guide': '1. 烧录已完成，开发板无法自动重启退出刷机模式；<br>2. <strong>请按一下开发板上的【RST】按键</strong>（或重新插拔 USB 线）启动系统！'
  },

  'en': {
    // Nav
    'nav.brand_tag': 'Hardware-level BLE Remote to Custom USB Composite Device',
    'nav.comparison': 'Why Hardware',
    'nav.features': 'Features',
    'nav.topology': 'Topology',
    'nav.flasher': 'Flasher',
    'nav.tutorial': 'Tutorial',
    'nav.cta': '🚀 Get Started',

    // Hero
    'hero.badge': '✨ 2026 Brand New · Web Serial Browser One-Click Flasher Live Now',
    'hero.title': 'Xiaomi Bluetooth Remote on Windows<br><span class="gradient-text">The Ultimate Hardware Bridge Solution</span>',
    'hero.subtitle': 'Powered by ESP32-S3 Native USB OTG. Eliminates Windows kernel key-drop & virtual audio latency. Directly remaps remote signals to a driverless physical microphone (UAC 1.0) and standard HID keyboard/multimedia devices. Plug-and-play, completely immune to anti-cheat.',
    'hero.btn_flasher': 'Get Started (Choose Flasher)',
    'hero.btn_tutorial': 'Setup Guide',
    'hero.btn_star': '⭐ Star on GitHub',
    'hero.pill_uac': 'Driverless UAC 1.0 Physical Mic',
    'hero.pill_keys': '13 Physical Keys 100% Air-Captured',
    'hero.pill_macros': '5-Layer Macro Mapping (Click/Hold/Double)',
    'hero.pill_wol': 'LAN Remote Wake-on-LAN (WOL)',
    'hero.pill_web': 'Built-in Visual Web Console',

    // Comparison
    'comp.tag': 'Direct Comparison',
    'comp.title': 'Say Goodbye to the Compromises of Software Solutions',
    'comp.subtitle': 'Windows Bluetooth kernel driver filters non-standard keys by design. RemoteMapper adopts a hardware intermediary relay for 100% native compatibility.',
    'comp.sw_title': 'Traditional Pure Software Direct Bluetooth',
    'comp.sw_1': '<strong>Keys Dropped by Windows Kernel</strong>: Volume+ (0x80), Volume- (0x81), and Back (0xF1) are filtered by kbdhid.sys. Application-level Hooks / AutoHotkey never receive them.',
    'comp.sw_2': '<strong>Voice Input Relies on Virtual Soundcards</strong>: Proprietary ATVV audio must be routed via VB-Cable, introducing severe latency, audio glitches, and channel conflicts.',
    'comp.sw_3': '<strong>Anti-Cheat & Security Risks</strong>: Custom kernel filtering drivers require Windows Test Mode, frequently triggering bans in competitive games (Vanguard, EAC, BattleEye).',
    'comp.sw_4': '<strong>Cannot Wake or Boot PC from Sleep</strong>: Bluetooth receiver powers off when PC sleeps or shuts down, making wireless power-on impossible.',
    'comp.hw_title': 'RemoteMapper-ESP32 Hardware Bridge',
    'comp.hw_1': '<strong>13 Physical Keys 100% Captured</strong>: ESP32-S3 captures raw BLE packets over the air, completely bypassing Windows kernel filtering.',
    'comp.hw_2': '<strong>Driverless Physical USB Mic (UAC 1.0)</strong>: On-chip real-time IMA-ADPCM streaming presents a genuine physical microphone to Windows, with instant hotkey triggering and click-noise suppression.',
    'comp.hw_3': '<strong>Plug-and-Play · 100% Anti-Cheat Immune</strong>: Appears to Windows as a standard physical keyboard and microphone. Zero background software or kernel drivers required.',
    'comp.hw_4': '<strong>LAN Wake-on-LAN (WOL) Power-On</strong>: Independently powered MCU sends Magic Packets on key hold, waking your PC from sleep or S5 shutdown from across the room.',

    // Features
    'feat.tag': 'Core Features',
    'feat.title': 'Core Capabilities Built for Desktop Excellence',
    'feat.subtitle': 'From low-level DSP and multi-layer state machines to embedded Web management, every detail is refined.',
    'feat.f1_title': 'Driverless UAC 1.0 Microphone',
    'feat.f1_desc': 'Simulates a 16kHz 16-Bit mono hardware microphone. Automatically triggers IME hotkey on press, with 150ms lead mute and 10ms micro fade-in to eliminate microswitch clicks.',
    'feat.f2_title': '5-Layer Independent Macro System',
    'feat.f2_desc': 'Default layer + 4 custom layers. Every key supports single-click, hold (200~2000ms), and double-click (100~600ms), with passthrough, momentary layers, idle timeout, and RGB.',
    'feat.f3_title': 'Wake-on-LAN (WOL) Remote Boot',
    'feat.f3_desc': 'Assign any key (or hold) to send network Magic Packets. Turn on or wake your PC from sleep or power-off (S5) with a single tap of the remote.',
    'feat.f4_title': 'Media Passthrough & Smooth Repeat',
    'feat.f4_desc': 'Native Consumer multimedia support. Smooth continuous volume adjustment on key hold, responsive play/pause and track skipping without repeated clicks.',
    'feat.f5_title': 'Built-in Visual Web Console',
    'feat.f5_desc': 'Embedded Web server with AP hotspot (RemoteMapper-AP) & STA Wi-Fi. Modify keymaps directly in any browser; all configurations persist in NVS flash across reboots.',
    'feat.f6_title': 'All ESP32-S3 Variants Supported',
    'feat.f6_desc': 'Compatible out-of-the-box with mainstream ESP32-S3 boards (N16R8, N8R8, N8R2, N4R2). Automatically detected and matched by the web flasher.',

    // Topology
    'topo.tag': 'System Topology',
    'topo.title': 'Pure & Efficient Hardware Topology',
    'topo.subtitle': 'ESP32-S3 bridges the remote and the PC, transforming BLE over-the-air packets into native USB composite signals.',

    // Flasher
    'flasher.tag': 'Flashing Methods',
    'flasher.title': 'Choose Your Flashing Method',
    'flasher.subtitle': 'We offer 3 flashing methods. <strong>Strongly recommend using the driverless Web Serial online flasher</strong> — plug in and flash in 15 seconds!',
    'flasher.m1_badge': '🌟 Official Best Choice · Zero Setup',
    'flasher.m1_title': 'Web Serial Browser Flasher',
    'flasher.m1_desc': 'Flash directly from Chrome or Edge via Web Serial. Zero drivers, zero Python, zero Git — write firmware in 15 seconds.',
    'flasher.m1_f1': '<strong>Automatic Hardware Detection</strong>: Probes Flash & PSRAM to automatically match correct firmware',
    'flasher.m1_f2': '<strong>Smart Fresh/Upgrade Detection</strong>: Detects existing RemoteMapper installations; new chips get clean flash, existing users keep Wi-Fi and keymaps',
    'flasher.m1_f3': '<strong>Ultra-High Speed</strong>: Supports 460800 / 921600 baud, writing in just ~15 seconds',
    'flasher.m1_tip': '💡 Quick Tip: Plug Type-C into the board\'s <strong>USB port</strong>, hold <strong>BOOT</strong>, click <strong>RST</strong>, release BOOT into download mode before connecting.',
    'flasher.m1_btn': '⚡ Open Web Serial Flasher Tool',
    'flasher.m2_badge': '📦 Offline Standalone Package',
    'flasher.m2_title': 'Green Standalone Flasher Package',
    'flasher.m2_desc': 'Perfect for offline or restricted environments. Download pre-packaged zip and double-click the one-click batch script.',
    'flasher.m2_f1': 'Includes official Espressif esptool standalone binary',
    'flasher.m2_f2': 'Pre-built firmware for all variants (N16R8 / N8R8 / N8R2 / N4R2)',
    'flasher.m2_f3': 'Interactive wizard batch script (`一键烧录.bat`)',
    'flasher.m2_btn': 'Download on GitHub Releases (.7z / .zip)',
    'flasher.m3_badge': '💻 Developers & Geeks',
    'flasher.m3_title': 'PlatformIO Source Build',
    'flasher.m3_desc': 'For developers wishing to customize BLE filtering, extend key state machines, or remap hardware pins.',
    'flasher.m3_f1': 'Full C/C++ source code with PlatformIO project config',
    'flasher.m3_f2': 'Build all targets with one command: `build.bat all`',
    'flasher.m3_f3': 'Direct USB flash via target command: `flash.bat n16r8`',
    'flasher.m3_btn': 'View GitHub Repository',

    // Tutorial
    'tut.tag': 'Step-by-Step Guide',
    'tut.title': 'Quick Start Guide After Flashing',
    'tut.subtitle': 'Just 5 simple steps to pair your remote, connect Wi-Fi, and customize key bindings.',
    'tut.s1_title': 'Manually Press [RST] to Reboot',
    'tut.s1_desc': 'The chip remains in Bootloader state after flashing. <strong>You must press the physical [RST] button</strong> (or re-plug USB) to start the system.',
    'tut.s2_title': 'Connect to RemoteMapper-AP Hotspot',
    'tut.s2_desc': 'On your phone or PC, search Wi-Fi and connect to <strong><code>RemoteMapper-AP</code></strong> (no password for clean flash).',
    'tut.s3_title': 'Open Built-in Web Console',
    'tut.s3_desc': 'Navigate to the management address in any browser. Pure web configuration without any apps or drivers.',
    'tut.s3_note': 'You can connect the device to your home Wi-Fi in Network Settings',
    'tut.s4_title': 'One-Click Remote Pairing',
    'tut.s4_desc': 'Hold <strong>[Home] + [Menu]</strong> on Xiaomi Remote 2 Pro for ~3s until a beep sounds, then click "Start Pairing" on the web page to bind.',
    'tut.s5_title': 'Configure Keymaps & Voice Hotkey',
    'tut.s5_desc': 'Assign keyboard keys, combos, multimedia controls, or WOL power-on to click, hold, or double-click in Keymap Settings. Persisted permanently in NVS.',

    // FAQ
    'faq.tag': 'FAQ',
    'faq.title': 'Frequently Asked Questions',
    'faq.q1': 'Q: Device not found in the COM port list or handshake times out?',
    'faq.a1': 'A: ① Ensure connection to the Type-C port labeled [USB] (Native USB), not COM/UART;<br>② Ensure your Type-C cable supports data transfer (charging-only cables will not work);<br>③ <strong>Manually enter Bootloader mode (Crucial)</strong>: Hold the [BOOT] button, click [RST] once, then release [BOOT], and click Connect again.',
    'faq.q2': 'Q: How to run the device normally after flashing completes?',
    'faq.a2': 'A: <strong>You must manually reboot after flashing!</strong> ESP32-S3 dev boards lack auto-reset-exit hardware circuits. Simply <strong>press the [RST] button on the board</strong> (or re-plug USB) to boot into normal operating mode.',
    'faq.q3': 'Q: My board is N16R8 or N8R8, what if I pick the wrong one?',
    'faq.a3': 'A: The web flasher automatically probes JEDEC Flash ID and eFuses to match the correct model! If there is a flash size mismatch, a warning will be displayed.',
    'faq.q4': 'Q: Will my settings and keymaps be lost after updating firmware?',
    'faq.a4': 'A: Simply select <strong>"Keep Config & Upgrade"</strong> mode! It only writes to the application region at 0x10000, completely preserving 0x9000 NVS where your Wi-Fi, passwords, remote pairing, and keymaps are saved.',

    // Footer
    'footer.gh': 'GitHub Repository',
    'footer.rel': 'Releases',
    'footer.issues': 'Issues & Feedback',
    'footer.flasher': 'Web Serial Flasher',
    'footer.copy': 'RemoteMapper-ESP32 • Open Source BLE Remote to Custom Keyboard Firmware • MIT License',

    // Flasher Platform Specific (flash.html)
    'flash.brand_subtitle': 'Driverless Web Serial Flasher Tool',
    'flash.nav_home': '🏠 Back to Official Site',
    'flash.serial_ready': 'Web Serial API Ready',
    'flash.unsupported': 'Web Serial is not supported in this browser. Please use <strong>Chrome, Edge, Brave or Opera on desktop</strong> to flash firmware.',
    'flash.step1_title': '1. Device Connection & Serial Port',
    'flash.status_disconnected': 'Disconnected',
    'flash.status_connected': 'Connected',
    'flash.guide_badge': 'Required Step',
    'flash.guide_title': 'Please Enter Bootloader Mode First',
    'flash.step1_s1': 'Connect a data-capable Type-C cable to the dev board\'s <strong>USB port</strong> (do not use COM/UART port on dual-port boards).',
    'flash.step1_s2': '<strong>Button Gesture for Bootloader Mode:</strong><br>① Hold the <strong>[BOOT]</strong> button;<br>② Click <strong>[RST]</strong> button and release;<br>③ Release the <strong>[BOOT]</strong> button.<br><span style="font-size:0.75rem; color:#f59e0b;">(★ Most ESP32-S3 boards cannot auto-switch without this physical gesture)</span>',
    'flash.step1_s3': 'After the button gesture, click <strong>"Connect Board"</strong> below and select the serial port.',
    'flash.baud_label': 'Flashing Baud Rate',
    'flash.btn_connect': 'Connect Board (Connect)',
    'flash.btn_disconnect': 'Disconnect',
    'flash.hw_chip': 'Chip Model',
    'flash.hw_flash': 'Flash Size',
    'flash.hw_psram': 'PSRAM Type',
    'flash.hw_mac': 'MAC Address',
    'flash.step2_title': '2. Firmware & Flashing Mode',
    'flash.match_badge': 'Model Auto-Detected',
    'flash.model_label': 'Hardware Model',
    'flash.mode_full_name': 'Full Clean Flash (Factory Reset)',
    'flash.mode_full_desc': 'Flashes bootloader + partitions + full app image (0x0). Recommended for clean factory reset or blank chips.',
    'flash.badge_full': 'Recommended for Blank Chips',
    'flash.mode_upg_name': 'Keep Config & Upgrade (Firmware Only)',
    'flash.mode_upg_desc': 'Writes only the app partition (0x10000), skips 0x9000 NVS to preserve all Wi-Fi, remote pairings, and keymaps!',
    'flash.badge_upg': 'Recommended for Existing Users',
    'flash.chk_erase': 'Erase entire flash before writing (Erase All)',
    'flash.step3_title': '3. Execute Flashing',
    'flash.step3_desc': 'Writing takes ~10-25s. <strong>★ Must physically press [RST] after flashing to start the device!</strong>',
    'flash.btn_flash': 'Start Flashing Now',
    'flash.console_title': 'Serial Console Log',
    'flash.btn_clear': 'Clear Log',
    'flash.faq_title': '💡 Troubleshooting & FAQ',
    'flash.success_title': '🎉 Flash Succeeded! Please press [RST] to reboot',
    'flash.success_guide': '1. Flashing is complete. The dev board cannot auto-reboot from bootloader mode.<br>2. <strong>Please physically press the [RST] button</strong> (or re-plug USB cable) to boot into normal mode!'
  }
};

/**
 * 获取用户首选语言
 */
function getPreferredLanguage() {
  const saved = localStorage.getItem('rm_lang');
  if (saved && (saved === 'zh-CN' || saved === 'en')) {
    return saved;
  }
  const browserLang = (navigator.language || navigator.userLanguage || '').toLowerCase();
  return browserLang.startsWith('zh') ? 'zh-CN' : 'en';
}

/**
 * 切换全局语言
 */
function setLanguage(lang) {
  if (!TRANSLATIONS[lang]) lang = 'zh-CN';
  localStorage.setItem('rm_lang', lang);
  document.documentElement.lang = lang;

  const dict = TRANSLATIONS[lang];

  // 更新文本元素
  document.querySelectorAll('[data-i18n]').forEach(el => {
    const key = el.getAttribute('data-i18n');
    if (dict[key]) {
      el.textContent = dict[key];
    }
  });

  // 更新 HTML 元素 (包含强调标签等)
  document.querySelectorAll('[data-i18n-html]').forEach(el => {
    const key = el.getAttribute('data-i18n-html');
    if (dict[key]) {
      el.innerHTML = dict[key];
    }
  });

  // 更新语言切换按钮状态
  document.querySelectorAll('.lang-btn').forEach(btn => {
    btn.classList.toggle('active', btn.getAttribute('data-lang') === lang);
  });

  // 派发自定义语言变更事件供动态脚本响应
  window.dispatchEvent(new CustomEvent('rm-lang-change', { detail: { lang } }));
}

// 页面加载完成后立即初始化
document.addEventListener('DOMContentLoaded', () => {
  setLanguage(getPreferredLanguage());

  // 绑定切换按钮点击事件
  document.querySelectorAll('.lang-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const targetLang = btn.getAttribute('data-lang');
      if (targetLang) {
        setLanguage(targetLang);
      }
    });
  });
});
