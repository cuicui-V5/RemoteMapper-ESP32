#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RemoteMapper - 小米蓝牙遥控器硬件桥接器</title>
    <style>
        :root {
            --bg-primary: #0b0f17;
            --bg-card: #151d2a;
            --bg-hover: #1e293b;
            --accent-cyan: #06b6d4;
            --accent-blue: #3b82f6;
            --accent-green: #10b981;
            --accent-orange: #f59e0b;
            --accent-red: #ef4444;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --border-color: #243247;
            --radius-card: 16px;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
        body { background-color: var(--bg-primary); color: var(--text-main); line-height: 1.5; padding-bottom: 60px; }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }

        header { display: flex; justify-content: space-between; align-items: center; padding: 16px 0; border-bottom: 1px solid var(--border-color); margin-bottom: 24px; }
        .logo { font-size: 24px; font-weight: 700; background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue)); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
        .badge { background: rgba(6, 182, 212, 0.15); color: var(--accent-cyan); padding: 4px 10px; border-radius: 9999px; font-size: 12px; font-weight: 600; border: 1px solid rgba(6, 182, 212, 0.3); }

        .tabs { display: flex; gap: 8px; margin-bottom: 20px; overflow-x: auto; padding-bottom: 4px; }
        .tab-btn { background: var(--bg-card); border: 1px solid var(--border-color); color: var(--text-muted); padding: 10px 18px; border-radius: 10px; cursor: pointer; font-size: 14px; font-weight: 600; transition: all 0.2s; white-space: nowrap; }
        .tab-btn:hover { background: var(--bg-hover); color: var(--text-main); }
        .tab-btn.active { background: linear-gradient(135deg, var(--accent-blue), var(--accent-cyan)); color: #fff; border-color: transparent; box-shadow: 0 4px 12px rgba(6, 182, 212, 0.3); }
        .tab-content { display: none; }
        .tab-content.active { display: block; }

        .card { background: var(--bg-card); border: 1px solid var(--border-color); border-radius: var(--radius-card); padding: 20px; margin-bottom: 20px; }
        .card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px; font-size: 16px; font-weight: 600; }
        .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
        .grid-4 { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 16px; margin-bottom: 20px; }
        @media (max-width: 768px) { .grid-2 { grid-template-columns: 1fr; } }

        .stat-card { background: var(--bg-card); border: 1px solid var(--border-color); border-radius: 12px; padding: 14px 18px; }
        .stat-title { font-size: 12px; color: var(--text-muted); margin-bottom: 4px; }
        .stat-val { font-size: 18px; font-weight: 700; color: #fff; }

        /* Real Xiaomi Silver Metallic Remote Visualizer */
        .remote-tester-container { display: flex; gap: 36px; align-items: flex-start; justify-content: center; flex-wrap: wrap; padding: 10px 0; }
        
        .real-remote-body {
            width: 220px;
            background: linear-gradient(180deg, #d4d4d8 0%, #e4e4e7 40%, #d4d4d8 70%, #a1a1aa 100%);
            border: 2px solid #e4e4e7;
            border-radius: 36px;
            padding: 26px 18px 20px 18px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.8), inset 0 2px 4px rgba(255,255,255,0.8);
            display: flex;
            flex-direction: column;
            align-items: center;
            position: relative;
        }

        .remote-top-row { display: flex; width: 100%; justify-content: space-between; margin-bottom: 20px; }
        .r-circle-btn {
            width: 44px; height: 44px;
            background: #27272a;
            border: 1px solid #3f3f46;
            border-radius: 50%;
            display: flex; align-items: center; justify-content: center;
            color: #d4d4d8; cursor: pointer;
            transition: all 0.12s; font-size: 16px; user-select: none;
            box-shadow: 0 4px 6px rgba(0,0,0,0.3);
        }
        .r-circle-btn:hover { background: #3f3f46; color: #fff; transform: translateY(-1px); }
        .r-circle-btn.pressed { background: #06b6d4 !important; color: #000 !important; transform: scale(0.92) !important; box-shadow: 0 0 20px #06b6d4 !important; }
        .r-circle-btn.power { background: #27272a; color: #f87171; }
        .r-circle-btn.voice { background: #27272a; color: #60a5fa; }

        /* D-Pad Section */
        .real-dpad-ring {
            width: 154px; height: 154px;
            border-radius: 50%;
            background: #27272a;
            border: 1px solid #3f3f46;
            position: relative;
            margin-bottom: 22px;
            display: flex; align-items: center; justify-content: center;
            box-shadow: 0 6px 12px rgba(0,0,0,0.35);
        }
        .dpad-part { position: absolute; background: transparent; border: none; color: #71717a; cursor: pointer; font-size: 14px; transition: all 0.12s; display: flex; align-items: center; justify-content: center; }
        .dpad-part:hover { color: #fff; }
        .dpad-part.pressed { color: #06b6d4 !important; transform: scale(0.9); text-shadow: 0 0 12px #06b6d4; }
        .d-up { top: 6px; width: 60px; height: 38px; }
        .d-down { bottom: 6px; width: 60px; height: 38px; }
        .d-left { left: 6px; width: 38px; height: 60px; }
        .d-right { right: 6px; width: 38px; height: 60px; }
        .d-center {
            width: 66px; height: 66px; border-radius: 50%;
            background: #18181b; border: 1px solid #3f3f46;
            z-index: 2; font-size: 13px; font-weight: bold; color: #d4d4d8;
        }
        .d-center.pressed { background: #06b6d4 !important; color: #000 !important; box-shadow: 0 0 20px #06b6d4 !important; }

        /* 2-Column Lower Controls */
        .remote-controls-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; width: 100%; margin-bottom: 24px; align-items: center; }
        .ctrl-col-left { display: flex; flex-direction: column; gap: 14px; align-items: center; }
        .ctrl-col-right { display: flex; flex-direction: column; gap: 14px; align-items: center; }

        /* Integrated Volume Rocker */
        .vol-pill {
            width: 48px; height: 102px;
            background: #27272a;
            border: 1px solid #3f3f46;
            border-radius: 24px;
            display: flex; flex-direction: column;
            overflow: hidden;
            box-shadow: 0 4px 8px rgba(0,0,0,0.3);
        }
        .vol-half {
            flex: 1; border: none; background: transparent;
            color: #d4d4d8; cursor: pointer;
            font-size: 18px; font-weight: bold;
            display: flex; align-items: center; justify-content: center;
            transition: all 0.12s;
        }
        .vol-half:hover { background: #3f3f46; color: #fff; }
        .vol-half.pressed { background: #06b6d4 !important; color: #000 !important; }
        .vol-half:first-child { border-bottom: 1px solid #3f3f46; }

        .btn-tv-box { width: 48px; height: 48px; border-radius: 50%; font-size: 12px; font-weight: bold; }
        .remote-footer { margin-top: 10px; display: flex; flex-direction: column; align-items: center; color: #71717a; font-size: 11px; }
        .remote-footer .nfc-icon { width: 14px; height: 14px; border: 1px solid #71717a; border-radius: 2px; display: flex; align-items: center; justify-content: center; font-size: 9px; font-weight: bold; margin-bottom: 14px; }

        /* Key Event Monitor Panel */
        .event-box { flex: 1; min-width: 320px; background: #090d16; border: 1px solid var(--border-color); border-radius: 14px; padding: 20px; }
        .stat-badge { font-size: 26px; font-weight: 700; color: var(--accent-cyan); margin: 6px 0 14px 0; }
        .event-field { display: flex; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid rgba(255,255,255,0.05); font-size: 14px; }
        .event-field span:first-child { color: var(--text-muted); }
        .event-field span:last-child { font-weight: 600; font-family: monospace; color: #fff; }

        /* Ultra-Simple Interactive Remap Modal */
        .modal-overlay { position: fixed; inset: 0; background: rgba(0,0,0,0.8); backdrop-filter: blur(6px); display: none; align-items: center; justify-content: center; z-index: 100; padding: 20px; }
        .modal { background: var(--bg-card); border: 1px solid var(--border-color); border-radius: var(--radius-card); max-width: 520px; width: 100%; padding: 24px; box-shadow: 0 25px 50px rgba(0,0,0,0.6); }

        .key-recorder-box {
            border: 2px dashed var(--accent-blue);
            background: rgba(59, 130, 246, 0.08);
            border-radius: 12px;
            padding: 24px 16px;
            text-align: center;
            cursor: pointer;
            outline: none;
            transition: all 0.2s;
            margin-bottom: 18px;
        }
        .key-recorder-box:focus, .key-recorder-box.recording {
            border-color: var(--accent-cyan);
            background: rgba(6, 182, 212, 0.15);
            box-shadow: 0 0 20px rgba(6, 182, 212, 0.3);
        }
        .key-badge-display { font-size: 24px; font-weight: 800; color: #fff; margin-top: 8px; min-height: 36px; display: flex; align-items: center; justify-content: center; gap: 8px; flex-wrap: wrap; }
        .kbd-chip { background: #0f172a; border: 1px solid var(--accent-cyan); color: var(--accent-cyan); padding: 4px 12px; border-radius: 6px; font-size: 18px; box-shadow: 0 2px 6px rgba(0,0,0,0.5); }

        .mode-card {
            cursor: pointer;
            background: #0f172a;
            border: 2px solid var(--border-color);
            border-radius: 10px;
            padding: 10px 6px;
            text-align: center;
            transition: all 0.18s cubic-bezier(0.4, 0, 0.2, 1);
            user-select: none;
        }
        .mode-card:hover {
            border-color: #3b82f6;
            background: #1e293b;
            transform: translateY(-1px);
        }
        .mode-card.selected {
            border-color: var(--accent-cyan) !important;
            background: rgba(6, 182, 212, 0.18) !important;
            box-shadow: 0 0 14px rgba(6, 182, 212, 0.3) !important;
        }
        .mode-card.selected .mode-title {
            color: var(--accent-cyan) !important;
        }

        .trigger-tab-btn {
            flex: 1;
            background: transparent;
            border: none;
            color: var(--text-muted);
            padding: 8px 10px;
            border-radius: 8px;
            cursor: pointer;
            font-size: 13px;
            font-weight: 600;
            transition: all 0.15s;
        }
        .trigger-tab-btn:hover {
            color: #fff;
        }
        .trigger-tab-btn.active {
            background: var(--bg-card);
            color: var(--accent-cyan);
            box-shadow: 0 2px 8px rgba(0,0,0,0.5);
        }

        .switch-toggle {
            position: relative;
            display: inline-block;
            width: 44px;
            height: 24px;
        }
        .switch-toggle input { opacity: 0; width: 0; height: 0; }
        .switch-toggle .slider {
            position: absolute; cursor: pointer; inset: 0;
            background-color: #27272a; transition: .2s; border-radius: 24px; border: 1px solid #3f3f46;
        }
        .switch-toggle .slider:before {
            position: absolute; content: ""; height: 16px; width: 16px; left: 3px; bottom: 3px;
            background-color: #d4d4d8; transition: .2s; border-radius: 50%;
        }
        .switch-toggle input:checked + .slider {
            background-color: var(--accent-cyan); border-color: var(--accent-cyan);
        }
        .switch-toggle input:checked + .slider:before {
            transform: translateX(20px); background-color: #000;
        }

        .btn { background: linear-gradient(135deg, var(--accent-blue), var(--accent-cyan)); color: #fff; border: none; padding: 10px 18px; border-radius: 8px; font-size: 14px; font-weight: 600; cursor: pointer; transition: opacity 0.2s; }
        .btn:hover { opacity: 0.9; }
        .btn-outline { background: transparent; border: 1px solid var(--border-color); color: var(--text-main); }
        .btn-outline:hover { background: var(--bg-hover); }
        .btn-danger { background: var(--accent-red); }

        .log-terminal { background: #000; border: 1px solid #1f2937; border-radius: 8px; padding: 12px; font-family: "SFMono-Regular", Consolas, Menlo, monospace; font-size: 12px; height: 380px; overflow-y: auto; color: #34d399; line-height: 1.6; }

        /* Layer Navigation & Configuration */
        .layer-nav-container { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 10px; margin-bottom: 16px; }
        .layer-tab-card {
            background: #0f172a;
            border: 2px solid var(--border-color);
            border-radius: 12px;
            padding: 10px 14px;
            cursor: pointer;
            transition: all 0.2s;
            user-select: none;
            position: relative;
            display: flex;
            flex-direction: column;
            gap: 4px;
        }
        .layer-tab-card:hover { border-color: #3b82f6; background: #151d2a; transform: translateY(-1px); }
        .layer-tab-card.active { border-color: var(--accent-cyan) !important; background: rgba(6, 182, 212, 0.12) !important; box-shadow: 0 4px 16px rgba(6, 182, 212, 0.25); }
        .layer-tab-card.hw-active { outline: 2px solid var(--accent-green); outline-offset: 1px; }
        .layer-tab-header { display: flex; justify-content: space-between; align-items: center; }
        .layer-tab-title { font-size: 14px; font-weight: 700; color: #fff; display: flex; align-items: center; gap: 6px; }
        .layer-color-dot { width: 10px; height: 10px; border-radius: 50%; display: inline-block; box-shadow: 0 0 6px currentColor; }
        .layer-live-badge { font-size: 10px; padding: 1px 6px; border-radius: 999px; background: rgba(16, 185, 129, 0.25); color: #34d399; border: 1px solid rgba(16, 185, 129, 0.5); font-weight: 700; display: none; }
        .layer-tab-card.hw-active .layer-live-badge { display: inline-block; }
        .layer-tab-desc { font-size: 11px; color: var(--text-muted); }

        .layer-config-box {
            background: #090d16;
            border: 1px solid var(--border-color);
            border-radius: 14px;
            padding: 16px 20px;
            margin-bottom: 20px;
            display: flex;
            flex-wrap: wrap;
            gap: 16px;
            align-items: center;
            justify-content: space-between;
        }
        .layer-config-item { display: flex; flex-direction: column; gap: 4px; }
        .layer-config-label { font-size: 11px; font-weight: 600; color: var(--text-muted); text-transform: uppercase; }
        .layer-color-swatch { width: 22px; height: 22px; border-radius: 50%; border: 2px solid transparent; cursor: pointer; transition: transform 0.15s; }
        .layer-color-swatch:hover { transform: scale(1.15); border-color: #fff; }

        /* Remote Button Layer Override Highlight */
        .layer-override-highlight {
            box-shadow: 0 0 12px rgba(168, 85, 247, 0.9), inset 0 0 6px rgba(168, 85, 247, 0.7) !important;
            border: 2px solid #c084fc !important;
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <div style="display: flex; align-items: center; gap: 12px;">
                <div class="logo">RemoteMapper</div>
                <span class="badge">ESP32-S3 Hardware Bridge</span>
            </div>
            <div id="top-status" style="font-size: 13px; color: var(--text-muted);">正在连接硬件...</div>
        </header>

        <!-- System Overview Cards -->
        <div class="grid-4">
            <div class="stat-card">
                <div class="stat-title">蓝牙遥控器连接状态</div>
                <div class="stat-val" id="stat-ble-state" style="color: var(--accent-green);">已连接</div>
                <div style="font-size: 12px; color: var(--text-muted); margin-top: 2px;" id="stat-ble-name">小米蓝牙语音遥控器</div>
            </div>
            <div class="stat-card">
                <div class="stat-title">Wi-Fi 局域网 IP</div>
                <div class="stat-val" id="stat-sta-ip">192.168.2.179</div>
                <div style="font-size: 12px; color: var(--text-muted); margin-top: 2px;" id="stat-ap-status">热点: 192.168.4.1</div>
            </div>
            <div class="stat-card">
                <div class="stat-title">音频流水线状态</div>
                <div class="stat-val" id="stat-audio-frames">0 帧</div>
                <div style="font-size: 12px; color: var(--text-muted); margin-top: 2px;">16kHz 16-Bit Mono UAC 1.0</div>
            </div>
            <div class="stat-card">
                <div class="stat-title">系统内存 / 运行时间</div>
                <div class="stat-val" id="stat-uptime">0s</div>
                <div style="font-size: 12px; color: var(--text-muted); margin-top: 2px;" id="stat-mem">SRAM: 200KB | PSRAM: 8MB</div>
            </div>
        </div>

        <div class="tabs">
            <button class="tab-btn active" onclick="switchTab('tab-tester')">遥控器与改键测试</button>
            <button class="tab-btn" onclick="switchTab('tab-ble')">蓝牙配对管理</button>
            <button class="tab-btn" onclick="switchTab('tab-logs')">运行日志</button>
            <button class="tab-btn" onclick="switchTab('tab-config')">配置管理</button>
            <button class="tab-btn" onclick="switchTab('tab-wifi')">Wi-Fi 与系统配置</button>
        </div>

        <!-- TAB 1: Key Tester & Remapper Visualizer -->
        <div id="tab-tester" class="tab-content active">
            <div class="card">
                <div class="card-header">
                    <span>按键映射和层级设置</span>
                    <button class="btn btn-outline" style="font-size: 12px;" onclick="resetAllKeymaps()">恢复出厂默认层级</button>
                </div>
                
                <!-- 5 Layer Navigation Tabs -->
                <div class="layer-nav-container" id="layer-nav-container">
                    <!-- Injected by renderLayerTabs() -->
                </div>

                <!-- Layer Properties Card -->
                <div class="layer-config-box" id="layer-config-box">
                    <div class="layer-config-item" style="flex:1.2; min-width:160px;">
                        <span class="layer-config-label">层级别名</span>
                        <input type="text" id="layer-name-input" maxlength="20" placeholder="例如：层1" 
                            style="background:#0b0f17; border:1px solid var(--border-color); border-radius:8px; padding:8px 12px; color:#fff; font-size:13px; outline:none;"
                            onchange="onLayerNameChange(this.value)">
                    </div>

                    <div class="layer-config-item" style="flex:1.4; min-width:190px;">
                        <span class="layer-config-label">层级生命周期类型</span>
                        <select id="layer-type-select" 
                            style="background:#0b0f17; border:1px solid var(--border-color); border-radius:8px; padding:8px 12px; color:#fff; font-size:13px; outline:none;"
                            onchange="onLayerTypeChange(this.value)">
                            <option value="0">永久停留层</option>
                            <option value="1">一次性瞬态层</option>
                            <option value="2">超时自动返回层</option>
                        </select>
                    </div>

                    <div class="layer-config-item" id="layer-timeout-wrapper" style="display:none; flex:1; min-width:150px;">
                        <span class="layer-config-label">闲置回退超时（秒）</span>
                        <div style="display:flex; align-items:center; gap:8px;">
                            <input type="number" id="layer-timeout-input" min="3" max="300" value="15" 
                                style="width:70px; background:#0b0f17; border:1px solid var(--border-color); border-radius:8px; padding:8px 10px; color:#fff; font-size:13px; outline:none;"
                                onchange="onLayerTimeoutChange(this.value)">
                            <span style="font-size:12px; color:var(--text-muted);">秒后自动返回</span>
                        </div>
                    </div>

                    <div class="layer-config-item" style="min-width:180px;">
                        <span class="layer-config-label">板载指示灯颜色</span>
                        <div style="display:flex; align-items:center; gap:8px;">
                            <input type="color" id="layer-color-picker" value="#00ff00" 
                                style="width:36px; height:32px; border:none; border-radius:6px; cursor:pointer; background:transparent;"
                                onchange="onLayerColorChange(this.value)">
                            <div style="display:flex; gap:5px; align-items:center;">
                                <div class="layer-color-swatch" style="background:#00FF00;" title="绿色（默认层）" onclick="onLayerColorChange('#00FF00')"></div>
                                <div class="layer-color-swatch" style="background:#06B6D4;" title="青色（层1）" onclick="onLayerColorChange('#06B6D4')"></div>
                                <div class="layer-color-swatch" style="background:#A855F7;" title="紫色（层2）" onclick="onLayerColorChange('#A855F7')"></div>
                                <div class="layer-color-swatch" style="background:#EAB308;" title="黄色（层3）" onclick="onLayerColorChange('#EAB308')"></div>
                                <div class="layer-color-swatch" style="background:#FFFFFF;" title="白色（层4）" onclick="onLayerColorChange('#FFFFFF')"></div>
                                <div class="layer-color-swatch" style="background:#EF4444;" title="红色" onclick="onLayerColorChange('#EF4444')"></div>
                            </div>
                        </div>
                    </div>

                    <div style="display:flex; gap:8px; align-items:flex-end;">
                        <button class="btn btn-outline" id="btn-clear-layer" style="font-size:12px; display:none; color:#f87171; border-color:rgba(239,68,68,0.4);" onclick="clearLayerOverrides(currentEditingLayer)">
                            清空本层覆盖
                        </button>
                        <button class="btn" style="font-size:12px;" onclick="saveAllLayers()">保存层级配置</button>
                    </div>
                </div>

                <div id="layer-status-tip" style="margin-bottom:16px; padding:10px 14px; background:rgba(6,182,212,0.08); border:1px dashed rgba(6,182,212,0.3); border-radius:8px; font-size:12px; color:var(--text-muted); line-height:1.5;">
                    <b>默认层</b>：其他层未单独配置的按键会自动穿透继承默认层的映射。
                </div>
                
                <div class="remote-tester-container">
                    <!-- Real Xiaomi Silver Metallic Remote DOM -->
                    <div class="real-remote-body">
                        <!-- Top Row: Power & Voice -->
                        <div class="remote-top-row">
                            <div class="r-circle-btn power" id="btn-0x66" onclick="openRemapModal(0x66, '电源键')">⏻</div>
                            <div class="r-circle-btn voice" id="btn-0x04" onclick="openRemapModal(0x04, '语音键')"><svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor" style="vertical-align:middle;"><path d="M12 14c1.66 0 3-1.34 3-3V5c0-1.66-1.34-3-3-3S9 3.34 9 5v6c0 1.66 1.34 3 3 3zm5.3-3c0 3-2.54 5.1-5.3 5.1S6.7 14 6.7 11H5c0 3.41 2.72 6.23 6 6.72V21h2v-3.28c3.28-.48 6-3.3 6-6.72h-1.7z"/></svg></div>
                        </div>

                        <!-- Middle: D-Pad -->
                        <div class="real-dpad-ring">
                            <button class="dpad-part d-up" id="btn-0x52" onclick="openRemapModal(0x52, '方向上')">●</button>
                            <button class="dpad-part d-down" id="btn-0x51" onclick="openRemapModal(0x51, '方向下')">●</button>
                            <button class="dpad-part d-left" id="btn-0x50" onclick="openRemapModal(0x50, '方向左')">●</button>
                            <button class="dpad-part d-right" id="btn-0x4F" onclick="openRemapModal(0x4F, '方向右')">●</button>
                            <button class="dpad-part d-center" id="btn-0x28" onclick="openRemapModal(0x28, '确定键')">OK</button>
                        </div>

                        <!-- Lower: 2 Columns Matching Real Remote -->
                        <div class="remote-controls-grid">
                            <!-- Left Column: Back, Home, Menu -->
                            <div class="ctrl-col-left">
                                <div class="r-circle-btn" id="btn-0xF1" onclick="openRemapModal(0xF1, '返回键')">&lt;</div>
                                <div class="r-circle-btn" id="btn-0x24" onclick="openRemapModal(0x24, '主页键')">⌂</div>
                                <div class="r-circle-btn" id="btn-0x5D" onclick="openRemapModal(0x5D, '菜单键')">≡</div>
                            </div>

                            <!-- Right Column: Vol Rocker (+/-) & TV -->
                            <div class="ctrl-col-right">
                                <div class="vol-pill">
                                    <button class="vol-half" id="btn-0x80" onclick="openRemapModal(0x80, '音量+')">+</button>
                                    <button class="vol-half" id="btn-0x81" onclick="openRemapModal(0x81, '音量-')">−</button>
                                </div>
                                <div class="r-circle-btn btn-tv-box" id="btn-0xC0" onclick="openRemapModal(0xC0, '电视键')">TV</div>
                            </div>
                        </div>

                        <!-- Bottom Branding -->
                        <div class="remote-footer">
                            <div class="nfc-icon">N</div>
                            <span style="font-weight: 700; letter-spacing: 1px; font-size: 13px;">xiaomi</span>
                        </div>
                    </div>

                    <!-- Live Key Event Telemetry -->
                    <div class="event-box">
                        <h3 style="margin-bottom: 12px; font-size: 15px; color: var(--accent-cyan);">实时按键状态</h3>
                        <div class="stat-badge" id="live-key-name">等待按键...</div>
                        
                        <div class="event-field">
                            <span>物理键码</span>
                            <span id="live-key-code">0x00</span>
                        </div>
                        <div class="event-field">
                            <span>按键状态</span>
                            <span id="live-key-state" style="color: var(--text-muted);">IDLE</span>
                        </div>
                        <div class="event-field">
                            <span>按下持续时间</span>
                            <span id="live-key-dur">0 ms</span>
                        </div>
                        <div class="event-field">
                            <span>触发动作</span>
                            <span id="live-act-type">ACTION_NONE</span>
                        </div>
                        <div class="event-field">
                            <span>注入键值</span>
                            <span id="live-act-val">None</span>
                        </div>
                        <div class="event-field">
                            <span>当前运行层级</span>
                            <span id="live-active-layer" style="color: var(--accent-cyan); font-weight: bold;">默认层</span>
                        </div>

                        <div style="margin-top: 20px; padding: 12px; background: rgba(6,182,212,0.1); border-radius: 8px; border: 1px dashed rgba(6,182,212,0.3); font-size: 13px;">
                            点击左侧按键进行按键映射调整
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- TAB 2: BLE Device Radar -->
        <div id="tab-ble" class="tab-content">
            <div class="card">
                <div class="card-header">
                    <span>蓝牙设备配对</span>
                    <button class="btn" onclick="scanBleDevices()">扫描蓝牙设备</button>
                </div>
                <div id="ble-dev-list" style="margin-top: 14px;">点击上方按钮扫描附近的蓝牙遥控器...</div>
            </div>
        </div>

        <!-- TAB 3: Runtime Logs -->
        <div id="tab-logs" class="tab-content">
            <div class="card">
                <div class="card-header">
                    <span>实时运行日志</span>
                    <div style="display: flex; gap: 8px;">
                        <button class="btn btn-outline" style="font-size: 12px;" onclick="refreshLogs()">刷新</button>
                        <button class="btn btn-outline" style="font-size: 12px;" onclick="clearLogs()">清空</button>
                    </div>
                </div>
                <div class="log-terminal" id="log-terminal">正在加载运行日志...</div>
            </div>
        </div>

        <!-- TAB 4: Configuration Management -->
        <div id="tab-config" class="tab-content">
            <!-- Section 1 & 2: Keymap Backup & Import -->
            <div class="card">
                <div class="card-header">
                    <div>
                        <span style="font-size: 16px;">按键映射与层级配置备份</span>
                        <div style="font-size: 12px; color: var(--text-muted); font-weight: normal; margin-top: 4px;">
                            仅导入或导出 5 个层级的按键映射规则（Keymap），不包含 Wi-Fi、蓝牙配对及底层系统参数，安全无冲突。
                        </div>
                    </div>
                </div>
                <div class="grid-2">
                    <div style="background: #090d16; border: 1px solid var(--border-color); border-radius: 12px; padding: 18px; display: flex; flex-direction: column; justify-content: space-between;">
                        <div>
                            <b style="color: var(--text-main); font-size: 14px; display: block; margin-bottom: 8px;">导出配置</b>
                            <p style="font-size: 13px; color: var(--text-muted); line-height: 1.6;">
                                将当前 ESP32 的所有按键映射及层级设置导出为 <code>.json</code> 文件，用于本地备份或多设备间快速复制。
                            </p>
                        </div>
                        <div style="margin-top: 16px;">
                            <button class="btn" style="font-size: 13px; width: 100%;" onclick="exportKeymapConfig()">导出按键配置 (.json)</button>
                        </div>
                    </div>

                    <div style="background: #090d16; border: 1px solid var(--border-color); border-radius: 12px; padding: 18px; display: flex; flex-direction: column; justify-content: space-between;">
                        <div>
                            <b style="color: var(--text-main); font-size: 14px; display: block; margin-bottom: 8px;">导入配置</b>
                            <p style="font-size: 13px; color: var(--text-muted); line-height: 1.6;">
                                从本地 JSON 文件恢复按键层级配置。系统将校验数据完整性，仅覆盖按键映射，绝不影响 Wi-Fi 与蓝牙绑定状态。
                            </p>
                        </div>
                        <div style="margin-top: 16px;">
                            <input type="file" id="keymap-file-input" accept=".json" style="display:none;" onchange="importKeymapConfig(event)">
                            <button class="btn btn-outline" style="font-size: 13px; width: 100%;" onclick="document.getElementById('keymap-file-input').click()">导入按键配置 (.json)</button>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Section 3: Reset Operations -->
            <div class="card">
                <div class="card-header">
                    <div>
                        <span style="font-size: 16px;">配置重置与清空</span>
                        <div style="font-size: 12px; color: var(--text-muted); font-weight: normal; margin-top: 4px;">
                            提供按键层级单独重置与系统出厂完全重置两种方式，请仔细区分后执行。
                        </div>
                    </div>
                </div>
                <div class="grid-2">
                    <div style="background: #090d16; border: 1px solid rgba(245, 158, 11, 0.35); border-radius: 12px; padding: 18px; display: flex; flex-direction: column; justify-content: space-between;">
                        <div>
                            <b style="color: #fbbf24; font-size: 14px; display: block; margin-bottom: 8px;">清空按键映射与层级数据</b>
                            <p style="font-size: 13px; color: var(--text-muted); line-height: 1.6;">
                                将所有 5 个层级的自定义映射规则恢复为出厂默认。<br>
                                <span style="color: #34d399;">保留所有 Wi-Fi 密码、AP 设置以及已绑定的蓝牙遥控器连接。</span>
                            </p>
                        </div>
                        <div style="margin-top: 16px;">
                            <button class="btn btn-outline" style="font-size: 13px; width: 100%; color: #fbbf24; border-color: rgba(245, 158, 11, 0.4);" onclick="resetKeymapOnly()">清空按键映射（保留蓝牙与WiFi配置）</button>
                        </div>
                    </div>

                    <div style="background: #090d16; border: 1px solid rgba(239, 68, 68, 0.35); border-radius: 12px; padding: 18px; display: flex; flex-direction: column; justify-content: space-between;">
                        <div>
                            <b style="color: #f87171; font-size: 14px; display: block; margin-bottom: 8px;">清空全部 NVS 数据（出厂完全重置）</b>
                            <p style="font-size: 13px; color: var(--text-muted); line-height: 1.6;">
                                彻底擦除 Flash NVS 内部的全部持久化分区。<br>
                                <span style="color: #f87171;">所有网络、蓝牙遥控器配对与按键映射将全部丢失</span>，设备将立即自动重启。
                            </p>
                        </div>
                        <div style="margin-top: 16px;">
                            <button class="btn btn-danger" style="font-size: 13px; width: 100%;" onclick="resetNvsFactory()">清空全部 NVS 数据（出厂完全重置）</button>
                        </div>
                    </div>
                </div>
            </div>

            <!-- Section 4: Read-Only Debug Viewer -->
            <div class="card">
                <details id="nvs-debug-details" ontoggle="onNvsDebugToggle(this)">
                    <summary style="font-size: 15px; font-weight: 600; color: var(--text-main); user-select: none; display: flex; align-items: center; justify-content: space-between; outline: none; cursor: pointer;">
                        <div style="display: flex; align-items: center; gap: 8px;">
                            <span id="nvs-details-arrow" style="font-size: 12px; color: var(--text-muted); display: inline-block; width: 16px;">▶</span>
                            <span>只读调试查看（仅查看纯文本格式的 NVS 内部数据）</span>
                            <span class="badge" style="font-size: 11px; padding: 2px 8px;">纯只读</span>
                        </div>
                        <span style="font-size: 12px; color: var(--text-muted); font-weight: normal;">点击展开 / 折叠</span>
                    </summary>
                    <div style="margin-top: 16px; border-top: 1px solid var(--border-color); padding-top: 14px;">
                        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; flex-wrap: wrap; gap: 8px;">
                            <div style="font-size: 12px; color: var(--text-muted);">
                                当前 ESP32 Flash NVS 中读取到的内部键值快照（含底层驱动与系统数据）。该区域严格只读，不允许直接修改或回写。
                            </div>
                            <div style="display: flex; gap: 8px;">
                                <button class="btn btn-outline" style="font-size: 12px; padding: 6px 12px;" onclick="loadNvsReadOnly(true)">刷新只读数据</button>
                                <button class="btn btn-outline" style="font-size: 12px; padding: 6px 12px;" onclick="copyNvsReadOnly()">复制到剪贴板</button>
                            </div>
                        </div>
                        <pre id="nvs-readonly-viewer" class="log-terminal" style="height: 380px; white-space: pre-wrap; word-break: break-all; background: #070a10; color: #38bdf8; border: 1px solid var(--border-color); border-radius: 8px; padding: 14px; font-family: 'Fira Code', Consolas, monospace; font-size: 12px; line-height: 1.5;">点击上方展开即可加载 NVS 内部数据快照...</pre>
                    </div>
                </details>
            </div>
        </div>

        <!-- TAB 5: Wi-Fi & System -->
        <div id="tab-wifi" class="tab-content">
            <div class="grid-2">
                <div class="card">
                    <div class="card-header">
                        <span>Wi-Fi 网络配置</span>
                        <button class="btn btn-outline" style="font-size: 12px;" onclick="scanWifiNetworks()">搜索 Wi-Fi</button>
                    </div>
                    <div id="wifi-scan-list" style="margin-bottom: 16px; font-size: 13px; color: var(--text-muted);">
                        点击右上角“搜索 Wi-Fi”可扫描附近无线网络，点击即可自动填入 SSID。
                    </div>
                    <div class="form-group">
                        <label style="display:block; font-size:13px; color:var(--text-muted); margin-bottom:6px;">Wi-Fi 名称</label>
                        <input type="text" id="wifi-ssid" placeholder="输入或选择 Wi-Fi 名称" style="width:100%; background:#0b0f17; border:1px solid var(--border-color); border-radius:8px; padding:10px 14px; color:#fff; font-size:14px; outline:none;">
                    </div>
                    <div class="form-group" style="margin-top:14px;">
                        <label style="display:block; font-size:13px; color:var(--text-muted); margin-bottom:6px;">Wi-Fi 密码</label>
                        <input type="password" id="wifi-pass" placeholder="输入 Wi-Fi 密码" style="width:100%; background:#0b0f17; border:1px solid var(--border-color); border-radius:8px; padding:10px 14px; color:#fff; font-size:14px; outline:none;">
                    </div>
                    <button class="btn" style="width: 100%; margin-top:16px;" onclick="saveWifi()">保存并连接 Wi-Fi</button>
                </div>

                <div style="display: flex; flex-direction: column; gap: 20px;">
                    <div class="card">
                        <div class="card-header">
                            <span>AP 热点配置</span>
                            <span id="ap-badge" style="font-size: 12px; padding: 2px 8px; border-radius: 6px; background: rgba(6,182,212,0.15); color: var(--accent-cyan); border: 1px solid rgba(6,182,212,0.3);">开放热点</span>
                        </div>
                        <div class="form-group">
                            <label style="display:block; font-size:13px; color:var(--text-muted); margin-bottom:6px;">热点名称</label>
                            <input type="text" value="RemoteMapper-AP" disabled style="width:100%; background:#070a10; border:1px solid var(--border-color); border-radius:8px; padding:10px 14px; color:var(--text-muted); font-size:14px; outline:none;">
                        </div>
                        <div class="form-group" style="margin-top:14px;">
                            <label style="display:block; font-size:13px; color:var(--text-muted); margin-bottom:6px;">热点密码</label>
                            <input type="password" id="ap-pass" placeholder="留空为开放热点，设置密码需至少8位" style="width:100%; background:#0b0f17; border:1px solid var(--border-color); border-radius:8px; padding:10px 14px; color:#fff; font-size:14px; outline:none;">
                        </div>
                        <div style="margin-top: 10px; font-size: 12px; color: var(--text-muted); line-height: 1.5;">
                            默认密码为空（开放热点）。设置密码需 8~63 位，保存后将自动启用 WPA2 加密保护。
                        </div>
                        <button class="btn" style="width: 100%; margin-top:14px;" onclick="saveApConfig()">保存 AP 配置</button>
                    </div>

                    <div class="card">
                        <div class="card-header"><span>系统控制</span></div>
                        <p style="font-size: 14px; color: var(--text-muted); margin-bottom: 20px;">
                            当前固件支持 UAC 1.0 USB 麦克风录音设备与标准 HID 键盘/多媒体复合注入。
                        </p>
                        <button class="btn btn-danger" style="width: 100%;" onclick="restartDevice()">重启设备</button>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <!-- Ultra-Simple Interactive Remap Modal -->
    <div class="modal-overlay" id="remap-modal" onclick="if(event.target === this) closeRemapModal()">
        <div class="modal">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 14px;">
                <h3 style="font-size: 18px;" id="modal-title">设置按键映射</h3>
                <span id="modal-vk-badge" style="font-size: 12px; color: var(--accent-cyan); font-family: monospace;">0x00</span>
            </div>

            <!-- Trigger Sub-Tabs (Click / Long Press / Double Click) -->
            <div id="trigger-tab-bar" style="display: flex; gap: 6px; margin-bottom: 14px; background: #070a10; padding: 4px; border-radius: 10px; border: 1px solid var(--border-color);">
                <button type="button" class="trigger-tab-btn active" id="tab-btn-click" onclick="switchTriggerTab('click')">单击</button>
                <button type="button" class="trigger-tab-btn" id="tab-btn-long" onclick="switchTriggerTab('long')">长按</button>
                <button type="button" class="trigger-tab-btn" id="tab-btn-double" onclick="switchTriggerTab('double')">双击</button>
            </div>

            <!-- Voice Mode Locked Card (Only for Voice Key 0x04) -->
            <div id="voice-mode-locked-card" style="display:none; background:rgba(59,130,246,0.15); border:2px solid #3b82f6; border-radius:10px; padding:12px 16px; margin-bottom:14px;">
                <div>
                    <div style="font-size:14px; font-weight:700; color:#93c5fd;">语音对讲专属模式</div>
                    <div style="font-size:12px; color:var(--text-muted); margin-top:2px;">按住时开启硬件麦克风录音并发送输入法热键，松开时停止录音并释放热键。</div>
                </div>
            </div>

            <!-- Long Press Enable / Timing Header -->
            <div id="long-press-header" style="display:none; background:#070a10; border:1px solid var(--border-color); border-radius:10px; padding:12px 14px; margin-bottom:14px;">
                <div style="display:flex; justify-content:space-between; align-items:center;">
                    <div>
                        <b style="font-size:13px; color:#fff;">启用长按功能</b>
                        <div style="font-size:11px; color:var(--text-muted); margin-top:2px;">按住时间达到阈值后触发长按动作</div>
                    </div>
                    <label class="switch-toggle">
                        <input type="checkbox" id="toggle-enable-long" onchange="onToggleTriggerEnable('long')">
                        <span class="slider"></span>
                    </label>
                </div>
                <div id="long-press-timing-row" style="margin-top:10px; display:flex; align-items:center; gap:12px;">
                    <span style="font-size:12px; color:var(--text-muted); white-space:nowrap;">长按时间:</span>
                    <input type="range" id="slider-long-ms" min="200" max="2000" step="50" value="600" oninput="onTimingSliderChange('long', this.value)" style="flex:1;">
                    <span id="label-long-ms" style="font-size:12px; font-weight:bold; color:var(--accent-cyan); width:55px;">600ms</span>
                </div>
            </div>

            <!-- Double Click Enable / Timing Header -->
            <div id="double-click-header" style="display:none; background:#070a10; border:1px solid var(--border-color); border-radius:10px; padding:12px 14px; margin-bottom:14px;">
                <div style="display:flex; justify-content:space-between; align-items:center;">
                    <div>
                        <b style="font-size:13px; color:#fff;">启用双击功能</b>
                        <div style="font-size:11px; color:var(--text-muted); margin-top:2px;">在判定时间窗口内快速按两次触发双击动作</div>
                    </div>
                    <label class="switch-toggle">
                        <input type="checkbox" id="toggle-enable-double" onchange="onToggleTriggerEnable('double')">
                        <span class="slider"></span>
                    </label>
                </div>
                <div id="double-click-timing-row" style="margin-top:10px; display:flex; align-items:center; gap:12px;">
                    <span style="font-size:12px; color:var(--text-muted); white-space:nowrap;">双击窗口:</span>
                    <input type="range" id="slider-double-ms" min="100" max="600" step="25" value="250" oninput="onTimingSliderChange('double', this.value)" style="flex:1;">
                    <span id="label-double-ms" style="font-size:12px; font-weight:bold; color:var(--accent-cyan); width:55px;">250ms</span>
                </div>
            </div>

            <!-- Mode Selector -->
            <div style="margin-bottom: 16px;">
                <label style="display:block; font-size:12px; color:var(--text-muted); margin-bottom:6px; font-weight:600;" id="action-mode-label">触发动作模式</label>
                
                <!-- Grid for Normal Keys (up to 5 modes) -->
                <div id="mode-selector-grid" style="display: grid; grid-template-columns: repeat(auto-fit, minmax(88px, 1fr)); gap: 8px;">
                    <div class="mode-card" id="mode-card-2" onclick="selectActionMode(2)">
                        <div class="mode-title" style="font-size:13px; font-weight:700; color:#fff;">键盘直通</div>
                        <div style="font-size:11px; color:var(--text-muted); margin-top:2px;">支持按住连发</div>
                    </div>
                    <div class="mode-card" id="mode-card-1" onclick="selectActionMode(1)">
                        <div class="mode-title" style="font-size:13px; font-weight:700; color:#fff;">单次点按</div>
                        <div style="font-size:11px; color:var(--text-muted); margin-top:2px;">组合快捷键</div>
                    </div>
                    <div class="mode-card" id="mode-card-4" onclick="selectActionMode(4)">
                        <div class="mode-title" style="font-size:13px; font-weight:700; color:#fff;">多媒体控制</div>
                        <div style="font-size:11px; color:var(--text-muted); margin-top:2px;">音量/播放/休眠</div>
                    </div>
                    <div class="mode-card" id="mode-card-9" onclick="selectActionMode(9)">
                        <div class="mode-title" style="font-size:13px; font-weight:700; color:#fff;">切换层级</div>
                        <div style="font-size:11px; color:var(--text-muted); margin-top:2px;">切换目标层</div>
                    </div>
                    <div class="mode-card" id="mode-card-10" onclick="selectActionMode(10)">
                        <div class="mode-title" style="font-size:13px; font-weight:700; color:#fff;">穿透继承</div>
                        <div style="font-size:11px; color:var(--text-muted); margin-top:2px;">继承默认层配置</div>
                    </div>
                </div>
            </div>

            <!-- Layer Switch Target Selector Box (Mode 9) -->
            <div id="layer-switch-config-box" style="display:none; background: #090d16; border: 1px solid var(--border-color); border-radius: 12px; padding: 16px; margin-bottom: 18px;">
                <label style="display:block; font-size:12px; color:var(--text-muted); margin-bottom:8px; font-weight:600;">选择目标层级</label>
                <div id="layer-target-options" style="display:grid; grid-template-columns: repeat(auto-fit, minmax(90px, 1fr)); gap: 8px;">
                    <!-- Rendered dynamically -->
                </div>
                <div style="margin-top:12px; padding:10px 14px; background:rgba(6,182,212,0.1); border-radius:8px; border:1px dashed rgba(6,182,212,0.3); font-size:12px; color:var(--text-muted); line-height:1.5;">
                    若当前已在目标层，再次触发将自动返回默认层。
                </div>
            </div>

            <!-- Layer Transparent Mode Box (Mode 10) -->
            <div id="layer-trans-config-box" style="display:none; background: rgba(59,130,246,0.1); border: 1px dashed #3b82f6; border-radius: 12px; padding: 16px; margin-bottom: 18px; text-align: center;">
                <div style="font-size:14px; font-weight:700; color:#93c5fd;">穿透继承模式</div>
                <div style="font-size:12px; color:var(--text-muted); margin-top:4px;">
                    本层不单独覆盖此按键触发动作，按下时自动穿透继承默认层的对应配置。
                </div>
            </div>

            <!-- Keyboard Direct Capture Box -->
            <div class="key-recorder-box" id="key-recorder-box" tabindex="0" onclick="startKeyboardRecording()">
                <div style="font-size: 13px; color: var(--text-muted);" id="recorder-instruction">
                    在键盘上按下任意按键或快捷键（支持 Ctrl/Alt/Win/Shift 组合键）
                </div>
                <div class="key-badge-display" id="recorded-badge-display">
                    <span style="color: var(--text-muted); font-size: 14px; font-weight: normal;">点击此处开始按键录制</span>
                </div>
            </div>

            <!-- Advanced Manual Key Code & Quick Select Area (Always Expanded) -->
            <div id="adv-config-container" style="background: #090d16; border: 1px solid var(--border-color); border-radius: 12px; padding: 16px; margin-bottom: 18px;">
                <div style="font-size: 13px; font-weight: 600; color: var(--text-main); margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center;">
                    <span>快捷选择与键码微调</span>
                </div>

                <!-- Quick Key Dropdown -->
                <div style="margin-bottom: 12px;">
                    <label style="display:block; font-size:12px; color:var(--text-muted); margin-bottom:4px;">快速选择按键 / 多媒体功能</label>
                    <select id="quick-key-select" onchange="onQuickKeySelect(this.value)" style="width:100%; padding:8px 10px; background:#151d2a; border:1px solid #243247; color:#fff; border-radius:8px; font-size:13px; outline:none;">
                        <option value="">-- 选择常用按键 / 组合键 / 多媒体 --</option>
                        <optgroup label="常用控制键">
                            <option value="k:0x00:0x28">回车</option>
                            <option value="k:0x00:0x29">Esc</option>
                            <option value="k:0x00:0x2C">空格</option>
                            <option value="k:0x00:0x2B">Tab</option>
                            <option value="k:0x00:0x2A">退格</option>
                            <option value="k:0x00:0x4C">删除</option>
                            <option value="k:0x00:0x39">大写锁定</option>
                            <option value="k:0x00:0x46">屏幕截图</option>
                        </optgroup>
                        <optgroup label="单修饰键">
                            <option value="m:0x08:0x00">Win 键</option>
                            <option value="m:0x01:0x00">Ctrl 键</option>
                            <option value="m:0x04:0x00">Alt 键</option>
                            <option value="m:0x02:0x00">Shift 键</option>
                        </optgroup>
                        <optgroup label="方向与翻页导航">
                            <option value="k:0x00:0x52">方向上</option>
                            <option value="k:0x00:0x51">方向下</option>
                            <option value="k:0x00:0x50">方向左</option>
                            <option value="k:0x00:0x4F">方向右</option>
                            <option value="k:0x00:0x4B">上一页</option>
                            <option value="k:0x00:0x4E">下一页</option>
                            <option value="k:0x00:0x4A">行首</option>
                            <option value="k:0x00:0x4D">行尾</option>
                        </optgroup>
                        <optgroup label="功能键 F1 ~ F12">
                            <option value="k:0x00:0x3A">F1</option>
                            <option value="k:0x00:0x3B">F2</option>
                            <option value="k:0x00:0x3C">F3</option>
                            <option value="k:0x00:0x3D">F4</option>
                            <option value="k:0x00:0x3E">F5</option>
                            <option value="k:0x00:0x3F">F6</option>
                            <option value="k:0x00:0x40">F7</option>
                            <option value="k:0x00:0x41">F8</option>
                            <option value="k:0x00:0x42">F9</option>
                            <option value="k:0x00:0x43">F10</option>
                            <option value="k:0x00:0x44">F11</option>
                            <option value="k:0x00:0x45">F12</option>
                        </optgroup>
                        <optgroup label="自定义脚本专用键 F13 ~ F24">
                            <option value="k:0x00:0x68">F13</option>
                            <option value="k:0x00:0x69">F14</option>
                            <option value="k:0x00:0x6A">F15</option>
                            <option value="k:0x00:0x6B">F16</option>
                            <option value="k:0x00:0x6C">F17</option>
                            <option value="k:0x00:0x6D">F18</option>
                            <option value="k:0x00:0x6E">F19</option>
                            <option value="k:0x00:0x6F">F20</option>
                            <option value="k:0x00:0x70">F21</option>
                            <option value="k:0x00:0x71">F22</option>
                            <option value="k:0x00:0x72">F23</option>
                            <option value="k:0x00:0x73">F24</option>
                        </optgroup>
                        <optgroup label="常用快捷组合键">
                            <option value="k:0x04:0x36">Alt + ,</option>
                            <option value="k:0x08:0x0B">Win + H</option>
                            <option value="k:0x08:0x07">Win + D</option>
                            <option value="k:0x04:0x2B">Alt + Tab</option>
                            <option value="k:0x04:0x3D">Alt + F4</option>
                            <option value="k:0x01:0x06">Ctrl + C</option>
                            <option value="k:0x01:0x19">Ctrl + V</option>
                            <option value="k:0x01:0x1D">Ctrl + Z</option>
                        </optgroup>
                        <optgroup label="多媒体与系统控制" id="quick-optgroup-media">
                            <option value="c:0x00:0x00E9">音量增加</option>
                            <option value="c:0x00:0x00EA">音量减少</option>
                            <option value="c:0x00:0x00E2">静音</option>
                            <option value="c:0x00:0x00CD">播放 / 暂停</option>
                            <option value="c:0x00:0x00B5">下一曲</option>
                            <option value="c:0x00:0x00B6">上一曲</option>
                            <option value="c:0x00:0x00B7">停止播放</option>
                            <option value="c:0x00:0x0032">系统休眠</option>
                            <option value="c:0x00:0x0224">网页返回</option>
                            <option value="c:0x00:0x0223">网页主页</option>
                            <option value="c:0x00:0x0225">网页前进</option>
                            <option value="c:0x00:0x0192">打开计算器</option>
                        </optgroup>
                    </select>
                </div>

                <!-- Numerical Inputs -->
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
                    <div>
                        <label style="display:block; font-size:12px; color:var(--text-muted); margin-bottom:4px;">修饰键（0x01=Ctrl, 0x02=Shift, 0x04=Alt, 0x08=Win）</label>
                        <input type="text" id="adv-mod" value="0x00" oninput="onAdvInputChanged()" placeholder="0x00" style="width:100%; padding:8px 10px; background:#151d2a; border:1px solid #243247; color:#fff; border-radius:8px; font-size:13px; outline:none; font-family:monospace;">
                    </div>
                    <div>
                        <label style="display:block; font-size:12px; color:var(--text-muted); margin-bottom:4px;">按键码（如 0x2C 或多媒体 0x00E9）</label>
                        <input type="text" id="adv-code" value="0x00" oninput="onAdvInputChanged()" placeholder="0x00" style="width:100%; padding:8px 10px; background:#151d2a; border:1px solid #243247; color:#fff; border-radius:8px; font-size:13px; outline:none; font-family:monospace;">
                    </div>
                </div>
            </div>

            <!-- Action Buttons -->
            <div style="display: flex; justify-content: space-between; align-items: center; margin-top: 20px; border-top: 1px solid var(--border-color); padding-top: 16px;">
                <button class="btn btn-outline" style="font-size: 13px; color: var(--accent-red); border-color: rgba(239,68,68,0.3);" onclick="clearCurrentKeyBinding()">清空映射</button>
                <div style="display: flex; gap: 10px;">
                    <button class="btn btn-outline" onclick="closeRemapModal()">取消</button>
                    <button class="btn" onclick="saveRemapConfig()">保存映射</button>
                </div>
            </div>
        </div>
    </div>

    <script>
        let currentKeymap = { active_layer: 0, layers: [] };
        let currentEditingLayer = 0;
        let activeHardwareLayer = 0;
        let selectedTargetLayer = 1;
        let editingKey = 0;
        let currentTriggerTab = 'click'; // 'click' | 'long' | 'double'
        let currentSelectedMode = 2;     // 1: Tap, 2: Hold, 4: Consumer, 7: Voice, 9: Switch Layer, 10: Transparent
        let editingBinding = null;
        let clearHighlightTimer = null;

        const KEY_NAMES = {
            0x66: '电源键',
            0xFF: '电源键',
            0x04: '语音键',
            0x52: '方向上',
            0x51: '方向下',
            0x50: '方向左',
            0x4F: '方向右',
            0x28: '确定键',
            0xF1: '返回键',
            0x24: '主页键',
            0x4A: '主页键',
            0x5D: '菜单键',
            0x65: '菜单键',
            0x80: '音量+',
            0x81: '音量-',
            0xC0: '电视键',
            0x35: '电视键'
        };

        // DOM Key -> USB HID Keyboard Code Map
        const DOM_TO_HID = {
            'KeyA': 0x04, 'KeyB': 0x05, 'KeyC': 0x06, 'KeyD': 0x07, 'KeyE': 0x08,
            'KeyF': 0x09, 'KeyG': 0x0A, 'KeyH': 0x0B, 'KeyI': 0x0C, 'KeyJ': 0x0D,
            'KeyK': 0x0E, 'KeyL': 0x0F, 'KeyM': 0x10, 'KeyN': 0x11, 'KeyO': 0x12,
            'KeyP': 0x13, 'KeyQ': 0x14, 'KeyR': 0x15, 'KeyS': 0x16, 'KeyT': 0x17,
            'KeyU': 0x18, 'KeyV': 0x19, 'KeyW': 0x1A, 'KeyX': 0x1B, 'KeyY': 0x1C, 'KeyZ': 0x1D,
            'Digit1': 0x1E, 'Digit2': 0x1F, 'Digit3': 0x20, 'Digit4': 0x21, 'Digit5': 0x22,
            'Digit6': 0x23, 'Digit7': 0x24, 'Digit8': 0x25, 'Digit9': 0x26, 'Digit0': 0x27,
            'Enter': 0x28, 'Escape': 0x29, 'Backspace': 0x2A, 'Tab': 0x2B, 'Space': 0x2C,
            'Minus': 0x2D, 'Equal': 0x2E, 'BracketLeft': 0x2F, 'BracketRight': 0x30,
            'Backslash': 0x31, 'Semicolon': 0x33, 'Quote': 0x34, 'Backquote': 0x35,
            'Comma': 0x36, 'Period': 0x37, 'Slash': 0x38, 'CapsLock': 0x39,
            'F1': 0x3A, 'F2': 0x3B, 'F3': 0x3C, 'F4': 0x3D, 'F5': 0x3E, 'F6': 0x3F,
            'F7': 0x40, 'F8': 0x41, 'F9': 0x42, 'F10': 0x43, 'F11': 0x44, 'F12': 0x45,
            'F13': 0x68, 'F14': 0x69, 'F15': 0x6A, 'F16': 0x6B, 'F17': 0x6C, 'F18': 0x6D,
            'F19': 0x6E, 'F20': 0x6F, 'F21': 0x70, 'F22': 0x71, 'F23': 0x72, 'F24': 0x73,
            'PrintScreen': 0x46, 'ScrollLock': 0x47, 'Pause': 0x48, 'Insert': 0x49,
            'Home': 0x4A, 'PageUp': 0x4B, 'Delete': 0x4C, 'End': 0x4D, 'PageDown': 0x4E,
            'ArrowRight': 0x4F, 'ArrowLeft': 0x50, 'ArrowDown': 0x51, 'ArrowUp': 0x52
        };

        function hexToHtmlColor(hexStr) {
            if (!hexStr) return '#00ff00';
            if (hexStr.startsWith('#')) return hexStr;
            if (hexStr.startsWith('0x') || hexStr.startsWith('0X')) {
                let h = hexStr.slice(2).padStart(6, '0');
                return '#' + h;
            }
            let num = parseInt(hexStr);
            if (!isNaN(num)) {
                return '#' + (num & 0xFFFFFF).toString(16).padStart(6, '0');
            }
            return '#00ff00';
        }

        function htmlColorToHex(htmlColor) {
            if (!htmlColor) return '0x00FF00';
            let clean = htmlColor.replace('#', '');
            return '0x' + clean.toUpperCase().padStart(6, '0');
        }

        function normalizeKeymapConfig() {
            if (!currentKeymap) currentKeymap = {};
            if (!currentKeymap.layers || !Array.isArray(currentKeymap.layers) || currentKeymap.layers.length === 0) {
                const legacyBindings = currentKeymap.bindings || [];
                currentKeymap.layers = [
                    { id: 0, name: '默认层', type: 0, timeout: 0, color: '0x00FF00', bindings: legacyBindings },
                    { id: 1, name: '层1', type: 2, timeout: 15, color: '0x06B6D4', bindings: [] },
                    { id: 2, name: '层2', type: 0, timeout: 0, color: '0xA855F7', bindings: [] },
                    { id: 3, name: '层3', type: 1, timeout: 0, color: '0xEAB308', bindings: [] },
                    { id: 4, name: '层4', type: 0, timeout: 0, color: '0xFFFFFF', bindings: [] }
                ];
            }
            while (currentKeymap.layers.length < 5) {
                const id = currentKeymap.layers.length;
                const defaultNames = ['默认层', '层1', '层2', '层3', '层4'];
                const defaultColors = ['0x00FF00', '0x06B6D4', '0xA855F7', '0xEAB308', '0xFFFFFF'];
                currentKeymap.layers.push({
                    id: id,
                    name: defaultNames[id] || (`层${id}`),
                    type: (id === 1 ? 2 : (id === 3 ? 1 : 0)),
                    timeout: (id === 1 ? 15 : 0),
                    color: defaultColors[id] || '0x00FF00',
                    bindings: []
                });
            }
        }

        function switchTab(id) {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            event.target.classList.add('active');
            document.getElementById(id).classList.add('active');
        }

        async function fetchStatus() {
            try {
                const res = await fetch('/api/status');
                const d = await res.json();
                document.getElementById('top-status').innerHTML = `固件: ${d.version} | 运行: ${d.uptime_sec}s | IP: ${d.sta_ip}`;
                document.getElementById('stat-sta-ip').innerText = d.sta_ip;
                document.getElementById('stat-uptime').innerText = `${d.uptime_sec}s`;
                document.getElementById('stat-audio-frames').innerText = `${d.frames_decoded} 帧`;
                document.getElementById('stat-mem').innerText = `Heap: ${Math.round(d.free_heap/1024)}KB | PSRAM: ${Math.round(d.free_psram/1024/1024)}MB`;
                
                const bleInfoRes = await fetch('/api/ble/info');
                const bleInfo = await bleInfoRes.json();
                if (bleInfo.connected) {
                    document.getElementById('stat-ble-state').innerText = '已连接';
                    document.getElementById('stat-ble-state').style.color = 'var(--accent-green)';
                    document.getElementById('stat-ble-name').innerText = bleInfo.name || '小米蓝牙语音遥控器';
                } else {
                    document.getElementById('stat-ble-state').innerText = '扫描重连中...';
                    document.getElementById('stat-ble-state').style.color = 'var(--accent-orange)';
                    document.getElementById('stat-ble-name').innerText = bleInfo.bound_mac ? `已绑定: ${bleInfo.bound_mac}` : '未绑定遥控器';
                }

                const apStatEl = document.getElementById('stat-ap-status');
                if (apStatEl && d.ap_ip) {
                    apStatEl.innerText = `热点: ${d.ap_ip} (${d.ap_secured ? 'WPA2' : '开放'})`;
                }

                if (d.ap_pass !== undefined) {
                    const apInput = document.getElementById('ap-pass');
                    if (apInput && document.activeElement !== apInput) {
                        apInput.value = d.ap_pass;
                    }
                    const apBadge = document.getElementById('ap-badge');
                    if (apBadge) {
                        if (d.ap_secured) {
                            apBadge.innerText = 'WPA2 加密';
                            apBadge.style.color = '#34d399';
                            apBadge.style.borderColor = 'rgba(16,185,129,0.4)';
                            apBadge.style.background = 'rgba(16,185,129,0.15)';
                        } else {
                            apBadge.innerText = '开放热点';
                            apBadge.style.color = 'var(--accent-cyan)';
                            apBadge.style.borderColor = 'rgba(6,182,212,0.3)';
                            apBadge.style.background = 'rgba(6,182,212,0.15)';
                        }
                    }
                }
            } catch(e){}
        }

        async function fetchKeyTelemetry() {
            try {
                const res = await fetch('/api/keymap/telemetry');
                const t = await res.json();

                if (t.active_layer !== undefined && t.active_layer !== activeHardwareLayer) {
                    activeHardwareLayer = t.active_layer;
                    renderLayerTabs();
                }

                const curLayerObj = (currentKeymap && currentKeymap.layers) ? currentKeymap.layers[activeHardwareLayer] : null;
                const layerName = curLayerObj ? curLayerObj.name : (activeHardwareLayer === 0 ? '默认层' : `层${activeHardwareLayer}`);
                const layerColor = curLayerObj ? hexToHtmlColor(curLayerObj.color) : 'var(--accent-cyan)';
                const activeEl = document.getElementById('live-active-layer');
                if (activeEl) {
                    activeEl.innerText = layerName;
                    activeEl.style.color = layerColor;
                }

                if (t.source_vk && t.source_vk !== 0) {
                    const vk = t.source_vk;
                    const hexCode = '0x' + vk.toString(16).toUpperCase().padStart(2, '0');
                    const btnName = KEY_NAMES[vk] || `按键 ${hexCode}`;
                    
                    document.getElementById('live-key-code').innerText = hexCode;
                    document.getElementById('live-key-state').innerText = t.is_pressed ? '按下' : '松开';
                    document.getElementById('live-key-state').style.color = t.is_pressed ? 'var(--accent-cyan)' : 'var(--accent-green)';
                    document.getElementById('live-key-dur').innerText = `${t.duration_ms || 0} ms`;
                    document.getElementById('live-act-type').innerText = `TYPE_${t.action_type || 0}`;
                    document.getElementById('live-act-val').innerText = `Key: 0x${(t.key_code||0).toString(16)} Cons: 0x${(t.consumer_code||0).toString(16)}`;

                    let targetHex = hexCode;
                    if (vk === 0xFF) targetHex = '0x66';
                    if (vk === 0x4A) targetHex = '0x24';
                    if (vk === 0x65) targetHex = '0x5D';
                    if (vk === 0x35) targetHex = '0xC0';

                    const btnEl = document.getElementById(`btn-${targetHex}`);
                    if (btnEl) {
                        document.getElementById('live-key-name').innerText = btnName;
                        btnEl.classList.add('pressed');
                        
                        if (clearHighlightTimer) clearTimeout(clearHighlightTimer);
                        clearHighlightTimer = setTimeout(() => {
                            btnEl.classList.remove('pressed');
                        }, 250);
                    }
                }
            } catch(e){}
        }

        const FACTORY_KEYMAP = {
            0x66: { source_vk: 0x66, has_click: true, click_type: 1, click_mod: 4, click_key: 43, click_cons: 0, has_long: true, long_ms: 600, long_type: 4, long_mod: 0, long_key: 0, long_cons: 0x0032, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 },
            0x04: { source_vk: 0x04, has_click: true, click_type: 7, click_mod: 64, click_key: 54, click_cons: 0, has_long: false, long_ms: 600, long_type: 0, long_mod: 0, long_key: 0, long_cons: 0, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 },
            0x52: { source_vk: 0x52, has_click: true, click_type: 2, click_mod: 0, click_key: 82, click_cons: 0, has_long: false, long_ms: 600, long_type: 0, long_mod: 0, long_key: 0, long_cons: 0, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 },
            0x51: { source_vk: 0x51, has_click: true, click_type: 2, click_mod: 0, click_key: 81, click_cons: 0, has_long: false, long_ms: 600, long_type: 0, long_mod: 0, long_key: 0, long_cons: 0, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 },
            0x50: { source_vk: 0x50, has_click: true, click_type: 2, click_mod: 0, click_key: 80, click_cons: 0, has_long: false, long_ms: 600, long_type: 0, long_mod: 0, long_key: 0, long_cons: 0, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 },
            0x4F: { source_vk: 0x4F, has_click: true, click_type: 2, click_mod: 0, click_key: 79, click_cons: 0, has_long: false, long_ms: 600, long_type: 0, long_mod: 0, long_key: 0, long_cons: 0, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 },
            0x28: { source_vk: 0x28, has_click: true, click_type: 2, click_mod: 0, click_key: 40, click_cons: 0, has_long: false, long_ms: 600, long_type: 0, long_mod: 0, long_key: 0, long_cons: 0, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 },
            0xF1: { source_vk: 0xF1, has_click: true, click_type: 4, click_mod: 0, click_key: 0, click_cons: 0x0224, has_long: false, long_ms: 600, long_type: 0, long_mod: 0, long_key: 0, long_cons: 0, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 },
            0x24: { source_vk: 0x24, has_click: true, click_type: 1, click_mod: 8, click_key: 7, click_cons: 0, has_long: false, long_ms: 600, long_type: 0, long_mod: 0, long_key: 0, long_cons: 0, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 },
            0x5D: { source_vk: 0x5D, has_click: true, click_type: 1, click_mod: 0, click_key: 44, click_cons: 0, has_long: false, long_ms: 600, long_type: 0, long_mod: 0, long_key: 0, long_cons: 0, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 },
            0x80: { source_vk: 0x80, has_click: true, click_type: 4, click_mod: 0, click_key: 0, click_cons: 0x00E9, has_long: false, long_ms: 600, long_type: 0, long_mod: 0, long_key: 0, long_cons: 0, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 },
            0x81: { source_vk: 0x81, has_click: true, click_type: 4, click_mod: 0, click_key: 0, click_cons: 0x00EA, has_long: false, long_ms: 600, long_type: 0, long_mod: 0, long_key: 0, long_cons: 0, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 },
            0xC0: { source_vk: 0xC0, has_click: true, click_type: 1, click_mod: 0, click_key: 65, click_cons: 0, has_long: false, long_ms: 600, long_type: 0, long_mod: 0, long_key: 0, long_cons: 0, has_double: false, double_ms: 250, double_type: 0, double_mod: 0, double_key: 0, double_cons: 0 }
        };

        function getActionSummaryText(type, mod, key, cons, target_layer) {
            if (type === 10) return '[继承默认层]';
            if (type === 9) {
                const tgt = target_layer || 0;
                let tgtName = tgt === 0 ? '默认层' : `层${tgt}`;
                if (currentKeymap && currentKeymap.layers && currentKeymap.layers[tgt]) {
                    tgtName = currentKeymap.layers[tgt].name || tgtName;
                }
                return `[切入: ${tgtName}]`;
            }
            if (type === 7) return '[语音对讲录音]';
            if (type === 0 || (!key && !cons && !mod)) return '未映射';

            let parts = [];
            if (mod & 0x01) parts.push('Ctrl');
            if (mod & 0x04) parts.push('Alt');
            if (mod & 0x02) parts.push('Shift');
            if (mod & 0x08) parts.push('Win');

            if (cons > 0) {
                const consMap = {
                    0x00E9: '音量+', 233: '音量+', 545: '音量+',
                    0x00EA: '音量-', 234: '音量-', 546: '音量-',
                    0x00E2: '静音',  226: '静音', 547: '静音',
                    0x00CD: '播放/暂停', 205: '播放/暂停', 516: '播放/暂停',
                    0x00B5: '下一曲', 181: '下一曲', 537: '下一曲',
                    0x00B6: '上一曲', 182: '上一曲', 538: '上一曲',
                    0x00B7: '停止',  183: '停止',
                    0x0032: '休眠',  50: '休眠', 530: '休眠',
                    0x0030: '电源',  48: '电源',
                    0x0224: '返回',  548: '返回', 558: '返回',
                    0x0223: '主页',  547: '主页', 557: '主页',
                    0x0225: '前进',  549: '前进',
                    0x0192: '计算器', 402: '计算器'
                };
                parts.push(consMap[cons] || `多媒体 0x${cons.toString(16).toUpperCase()}`);
            } else if (key > 0) {
                let name = `Key(0x${key.toString(16).toUpperCase()})`;
                for (let k in DOM_TO_HID) {
                    if (DOM_TO_HID[k] === key) {
                        name = k.replace('Key', '').replace('Digit', '').replace('Arrow', '');
                        break;
                    }
                }
                parts.push(name);
            }
            return parts.join('+');
        }

        function renderLayerTabs() {
            normalizeKeymapConfig();
            const container = document.getElementById('layer-nav-container');
            if (!container) return;

            const typeNames = ['永久层', '一次性层', '超时返回'];

            let html = '';
            currentKeymap.layers.forEach((l, idx) => {
                const isActive = (idx === currentEditingLayer);
                const isHwActive = (idx === activeHardwareLayer);
                const htmlColor = hexToHtmlColor(l.color);
                const typeText = (idx === 0) ? '默认层' : (l.type === 2 ? `${l.timeout || 15}s 超时` : (typeNames[l.type] || ''));
                const overrideCount = (idx === 0) ? l.bindings.length : (l.bindings ? l.bindings.filter(b => (b.has_click && b.click_type !== 10) || (b.has_long && b.long_type !== 10) || (b.has_double && b.double_type !== 10)).length : 0);
                const countBadge = (idx === 0) ? `${overrideCount} 基础键` : (overrideCount > 0 ? `${overrideCount} 键覆盖` : `全穿透`);

                html += `
                <div class="layer-tab-card ${isActive ? 'active' : ''} ${isHwActive ? 'hw-active' : ''}" onclick="switchEditingLayer(${idx})">
                    <div class="layer-tab-header">
                        <span class="layer-tab-title">
                            <span class="layer-color-dot" style="background-color:${htmlColor}; color:${htmlColor};"></span>
                            <span>${idx === 0 ? '默认层' : '层 ' + idx}</span>
                        </span>
                        <span class="layer-live-badge">运行中</span>
                    </div>
                    <div style="font-size:13px; font-weight:700; color:#fff; white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">
                        ${l.name || (idx === 0 ? '默认层' : '层' + idx)}
                    </div>
                    <div style="display:flex; justify-content:space-between; align-items:center; margin-top:2px;">
                        <span class="layer-tab-desc">${typeText}</span>
                        <span style="font-size:10px; color:${overrideCount > 0 ? 'var(--accent-cyan)' : 'var(--text-muted)'}; font-weight:600;">${countBadge}</span>
                    </div>
                </div>`;
            });
            container.innerHTML = html;
        }

        function renderLayerCard() {
            normalizeKeymapConfig();
            const layer = currentKeymap.layers[currentEditingLayer];
            if (!layer) return;

            const nameInput = document.getElementById('layer-name-input');
            const typeSelect = document.getElementById('layer-type-select');
            const timeoutWrapper = document.getElementById('layer-timeout-wrapper');
            const timeoutInput = document.getElementById('layer-timeout-input');
            const colorPicker = document.getElementById('layer-color-picker');
            const clearBtn = document.getElementById('btn-clear-layer');
            const tipEl = document.getElementById('layer-status-tip');

            if (nameInput) {
                nameInput.value = layer.name || (currentEditingLayer === 0 ? '默认层' : `层${currentEditingLayer}`);
            }
            if (typeSelect) {
                typeSelect.value = layer.type || 0;
                typeSelect.disabled = (currentEditingLayer === 0);
            }
            if (timeoutWrapper && timeoutInput) {
                timeoutInput.value = layer.timeout || 15;
                timeoutWrapper.style.display = (currentEditingLayer !== 0 && layer.type === 2) ? 'flex' : 'none';
            }
            if (colorPicker) {
                colorPicker.value = hexToHtmlColor(layer.color);
            }
            if (clearBtn) {
                clearBtn.style.display = (currentEditingLayer === 0) ? 'none' : 'inline-block';
            }
            if (tipEl) {
                if (currentEditingLayer === 0) {
                    tipEl.innerHTML = '<b>默认层</b>：其他层未单独配置的按键会自动穿透继承默认层的映射。';
                } else {
                    tipEl.innerHTML = `<b>当前编辑：${layer.name || ('层' + currentEditingLayer)}</b>。高亮按键为本层独立覆盖，其余按键自动穿透继承默认层。`;
                }
            }
        }

        function switchEditingLayer(idx) {
            if (idx < 0 || idx >= 5) return;
            currentEditingLayer = idx;
            renderLayerTabs();
            renderLayerCard();
            updateRemoteVisualTooltips();
        }

        function onLayerNameChange(val) {
            normalizeKeymapConfig();
            const layer = currentKeymap.layers[currentEditingLayer];
            if (!layer) return;
            layer.name = val.trim() || (currentEditingLayer === 0 ? '默认层' : `层${currentEditingLayer}`);
            renderLayerTabs();
        }

        function onLayerTypeChange(val) {
            normalizeKeymapConfig();
            const layer = currentKeymap.layers[currentEditingLayer];
            if (!layer) return;
            layer.type = parseInt(val);
            const timeoutWrapper = document.getElementById('layer-timeout-wrapper');
            if (timeoutWrapper) {
                timeoutWrapper.style.display = (layer.type === 2 && currentEditingLayer !== 0) ? 'flex' : 'none';
            }
            renderLayerTabs();
        }

        function onLayerTimeoutChange(val) {
            normalizeKeymapConfig();
            const layer = currentKeymap.layers[currentEditingLayer];
            if (!layer) return;
            let sec = parseInt(val) || 15;
            if (sec < 3) sec = 3;
            if (sec > 300) sec = 300;
            layer.timeout = sec;
            const input = document.getElementById('layer-timeout-input');
            if (input) input.value = sec;
            renderLayerTabs();
        }

        function onLayerColorChange(hexColor) {
            normalizeKeymapConfig();
            const layer = currentKeymap.layers[currentEditingLayer];
            if (!layer) return;
            layer.color = htmlColorToHex(hexColor);
            const picker = document.getElementById('layer-color-picker');
            if (picker) picker.value = hexColor;
            renderLayerTabs();
        }

        async function saveAllLayers() {
            const ok = await saveKeymapToServer();
            if (ok) {
                showToast('层级配置已成功保存写入 Flash');
                renderLayerTabs();
                renderLayerCard();
                updateRemoteVisualTooltips();
            }
        }

        async function clearLayerOverrides(layerIdx) {
            if (layerIdx === 0) return;
            normalizeKeymapConfig();
            const layer = currentKeymap.layers[layerIdx];
            if (!layer) return;
            if (!confirm(`确定要清空【${layer.name}】的所有按键覆盖吗？\n清空后该层所有按键将穿透继承默认层。`)) return;
            layer.bindings = [];
            const ok = await saveKeymapToServer();
            if (ok) {
                showToast(`【${layer.name}】已恢复穿透继承默认层`);
                renderLayerTabs();
                renderLayerCard();
                updateRemoteVisualTooltips();
            }
        }

        async function saveKeymapToServer() {
            try {
                const res = await fetch('/api/keymap/save', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(currentKeymap)
                });
                if (res.ok) {
                    return true;
                } else {
                    showToast('保存到硬件失败', true);
                    return false;
                }
            } catch(e) {
                showToast('保存请求出错: ' + e.message, true);
                return false;
            }
        }

        function updateRemoteVisualTooltips() {
            normalizeKeymapConfig();
            const vkList = [0x66, 0x04, 0x52, 0x51, 0x50, 0x4F, 0x28, 0xF1, 0x24, 0x5D, 0x80, 0x81, 0xC0];
            const curLayer = currentKeymap.layers[currentEditingLayer] || currentKeymap.layers[0];
            const layer0 = currentKeymap.layers[0];

            vkList.forEach(vk => {
                const hex = '0x' + vk.toString(16).toUpperCase().padStart(2, '0');
                const el = document.getElementById(`btn-${hex}`);
                if (!el) return;

                const b0 = (layer0.bindings && layer0.bindings.find(x => x.source_vk === vk)) || FACTORY_KEYMAP[vk];
                let bCur = (curLayer.bindings && curLayer.bindings.find(x => x.source_vk === vk)) || null;

                const isOverride = (currentEditingLayer > 0 && bCur && (
                    (bCur.has_click && bCur.click_type !== 10) ||
                    (bCur.has_long && bCur.long_type !== 10) ||
                    (bCur.has_double && bCur.double_type !== 10)
                ));

                if (isOverride) {
                    el.classList.add('layer-override-highlight');
                } else {
                    el.classList.remove('layer-override-highlight');
                }

                const effClickType = (bCur && bCur.has_click && bCur.click_type !== 10) ? bCur.click_type : (b0 ? b0.click_type : 0);
                const effClickMod = (bCur && bCur.has_click && bCur.click_type !== 10) ? bCur.click_mod : (b0 ? b0.click_mod : 0);
                const effClickKey = (bCur && bCur.has_click && bCur.click_type !== 10) ? bCur.click_key : (b0 ? b0.click_key : 0);
                const effClickCons = (bCur && bCur.has_click && bCur.click_type !== 10) ? bCur.click_cons : (b0 ? b0.click_cons : 0);
                const effClickLayer = (bCur && bCur.has_click && bCur.click_type !== 10) ? bCur.click_layer : (b0 ? b0.click_layer : 0);

                let desc = `【${KEY_NAMES[vk] || hex}】`;
                if (currentEditingLayer > 0) {
                    desc += isOverride ? ` (本层覆盖)` : ` (继承默认层)`;
                }

                desc += `\n单击: ${getActionSummaryText(effClickType, effClickMod, effClickKey, effClickCons, effClickLayer)}`;

                const hasLong = (bCur && bCur.has_long && bCur.long_type !== 10) ? true : (b0 ? b0.has_long : false);
                if (hasLong) {
                    const lType = (bCur && bCur.has_long && bCur.long_type !== 10) ? bCur.long_type : b0.long_type;
                    const lMod = (bCur && bCur.has_long && bCur.long_type !== 10) ? bCur.long_mod : b0.long_mod;
                    const lKey = (bCur && bCur.has_long && bCur.long_type !== 10) ? bCur.long_key : b0.long_key;
                    const lCons = (bCur && bCur.has_long && bCur.long_type !== 10) ? bCur.long_cons : b0.long_cons;
                    const lLayer = (bCur && bCur.has_long && bCur.long_type !== 10) ? bCur.long_layer : b0.long_layer;
                    const lMs = (bCur && bCur.has_long && bCur.long_type !== 10) ? bCur.long_ms : b0.long_ms;
                    desc += `\n长按(${lMs || 600}ms): ${getActionSummaryText(lType, lMod, lKey, lCons, lLayer)}`;
                }

                const hasDouble = (bCur && bCur.has_double && bCur.double_type !== 10) ? true : (b0 ? b0.has_double : false);
                if (hasDouble) {
                    const dType = (bCur && bCur.has_double && bCur.double_type !== 10) ? bCur.double_type : b0.double_type;
                    const dMod = (bCur && bCur.has_double && bCur.double_type !== 10) ? bCur.double_mod : b0.double_mod;
                    const dKey = (bCur && bCur.has_double && bCur.double_type !== 10) ? bCur.double_key : b0.double_key;
                    const dCons = (bCur && bCur.has_double && bCur.double_type !== 10) ? bCur.double_cons : b0.double_cons;
                    const dLayer = (bCur && bCur.has_double && bCur.double_type !== 10) ? bCur.double_layer : b0.double_layer;
                    const dMs = (bCur && bCur.has_double && bCur.double_type !== 10) ? bCur.double_ms : b0.double_ms;
                    desc += `\n双击(${dMs || 250}ms): ${getActionSummaryText(dType, dMod, dKey, dCons, dLayer)}`;
                }

                el.title = desc;
            });
        }

        async function loadKeymap() {
            try {
                const res = await fetch('/api/keymap');
                currentKeymap = await res.json();
                normalizeKeymapConfig();
                renderLayerTabs();
                renderLayerCard();
                updateRemoteVisualTooltips();
            } catch(e){}
        }

        function parseHexOrDec(val) {
            if (typeof val === 'number') return val;
            if (!val) return 0;
            val = String(val).trim();
            if (val.startsWith('0x') || val.startsWith('0X')) {
                return parseInt(val, 16) || 0;
            }
            if (/^[0-9a-fA-F]+$/.test(val) && /[a-fA-F]/.test(val)) {
                return parseInt(val, 16) || 0;
            }
            return parseInt(val, 10) || 0;
        }

        function renderTargetLayerButtons() {
            const container = document.getElementById('layer-target-options');
            if (!container) return;
            normalizeKeymapConfig();
            let html = '';
            currentKeymap.layers.forEach((l, idx) => {
                const isSel = (idx === selectedTargetLayer);
                html += `
                <button type="button" class="target-layer-btn" data-layer="${idx}" 
                    style="padding:8px 6px; border-radius:10px; cursor:pointer; font-weight:700; font-size:12px; transition:all 0.15s; background:${isSel ? 'rgba(6,182,212,0.2)' : '#151d2a'}; border:2px solid ${isSel ? 'var(--accent-cyan)' : 'var(--border-color)'}; color:${isSel ? 'var(--accent-cyan)' : '#fff'}; display:flex; flex-direction:column; align-items:center; gap:2px;"
                    onclick="selectTargetLayer(${idx})">
                    <span>${idx === 0 ? '默认层' : '层 ' + idx}</span>
                    <span style="font-size:10px; color:var(--text-muted); font-weight:normal; max-width:70px; white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">${l.name || ''}</span>
                </button>`;
            });
            container.innerHTML = html;
        }

        function selectTargetLayer(layerIdx) {
            selectedTargetLayer = layerIdx;
            renderTargetLayerButtons();
            renderTriggerView(9, 0, 0, 0);
        }

        function saveActiveTabToBinding() {
            if (!editingBinding) return;
            const mod = parseHexOrDec(document.getElementById('adv-mod').value);
            const code = parseHexOrDec(document.getElementById('adv-code').value);
            const mode = currentSelectedMode;

            const longToggle = document.getElementById('toggle-enable-long');
            if (longToggle) editingBinding.has_long = longToggle.checked;
            const longSlider = document.getElementById('slider-long-ms');
            if (longSlider) editingBinding.long_ms = parseInt(longSlider.value) || 600;

            const doubleToggle = document.getElementById('toggle-enable-double');
            if (doubleToggle) editingBinding.has_double = doubleToggle.checked;
            const doubleSlider = document.getElementById('slider-double-ms');
            if (doubleSlider) editingBinding.double_ms = parseInt(doubleSlider.value) || 250;

            if (currentTriggerTab === 'click') {
                editingBinding.click_type = mode;
                if (mode === 9) {
                    editingBinding.click_layer = selectedTargetLayer;
                    editingBinding.click_key = 0; editingBinding.click_mod = 0; editingBinding.click_cons = 0;
                    editingBinding.has_click = true;
                } else if (mode === 10) {
                    editingBinding.click_key = 0; editingBinding.click_mod = 0; editingBinding.click_cons = 0;
                    editingBinding.has_click = true;
                } else if (mode === 4) {
                    editingBinding.click_cons = code;
                    editingBinding.click_key = 0; editingBinding.click_mod = 0;
                    editingBinding.has_click = (code > 0);
                } else {
                    editingBinding.click_key = code;
                    editingBinding.click_mod = mod;
                    editingBinding.click_cons = 0;
                    editingBinding.has_click = (mode !== 0 && (code > 0 || mod > 0));
                }
            } else if (currentTriggerTab === 'long') {
                editingBinding.long_type = mode;
                if (mode === 9) {
                    editingBinding.long_layer = selectedTargetLayer;
                    editingBinding.long_key = 0; editingBinding.long_mod = 0; editingBinding.long_cons = 0;
                } else if (mode === 10) {
                    editingBinding.long_key = 0; editingBinding.long_mod = 0; editingBinding.long_cons = 0;
                } else if (mode === 4) {
                    editingBinding.long_cons = code;
                    editingBinding.long_key = 0; editingBinding.long_mod = 0;
                } else {
                    editingBinding.long_key = code;
                    editingBinding.long_mod = mod;
                    editingBinding.long_cons = 0;
                }
            } else if (currentTriggerTab === 'double') {
                editingBinding.double_type = mode;
                if (mode === 9) {
                    editingBinding.double_layer = selectedTargetLayer;
                    editingBinding.double_key = 0; editingBinding.double_mod = 0; editingBinding.double_cons = 0;
                } else if (mode === 10) {
                    editingBinding.double_key = 0; editingBinding.double_mod = 0; editingBinding.double_cons = 0;
                } else if (mode === 4) {
                    editingBinding.double_cons = code;
                    editingBinding.double_key = 0; editingBinding.double_mod = 0;
                } else {
                    editingBinding.double_key = code;
                    editingBinding.double_mod = mod;
                    editingBinding.double_cons = 0;
                }
            }
        }

        function loadTriggerDataToUI(tab) {
            document.querySelectorAll('.trigger-tab-btn').forEach(b => b.classList.remove('active'));
            const activeBtn = document.getElementById(`tab-btn-${tab}`);
            if (activeBtn) activeBtn.classList.add('active');

            const longHeader = document.getElementById('long-press-header');
            const doubleHeader = document.getElementById('double-click-header');
            const actionLabel = document.getElementById('action-mode-label');

            let mode = 2, mod = 0, key = 0, cons = 0, target_layer = 1;

            if (tab === 'click') {
                longHeader.style.display = 'none';
                doubleHeader.style.display = 'none';
                actionLabel.innerText = '单击触发动作模式';
                mode = editingBinding.click_type !== undefined ? editingBinding.click_type : 2;
                mod = editingBinding.click_mod || 0;
                key = editingBinding.click_key || 0;
                cons = editingBinding.click_cons || 0;
                target_layer = editingBinding.click_layer !== undefined ? editingBinding.click_layer : (currentEditingLayer === 1 ? 0 : 1);
            } else if (tab === 'long') {
                longHeader.style.display = 'block';
                doubleHeader.style.display = 'none';
                actionLabel.innerText = '长按触发动作模式';
                document.getElementById('toggle-enable-long').checked = !!editingBinding.has_long;
                document.getElementById('slider-long-ms').value = editingBinding.long_ms || 600;
                document.getElementById('label-long-ms').innerText = `${editingBinding.long_ms || 600}ms`;
                mode = editingBinding.long_type !== undefined ? editingBinding.long_type : 1;
                mod = editingBinding.long_mod || 0;
                key = editingBinding.long_key || 0;
                cons = editingBinding.long_cons || 0;
                target_layer = editingBinding.long_layer !== undefined ? editingBinding.long_layer : (currentEditingLayer === 1 ? 0 : 1);
            } else if (tab === 'double') {
                longHeader.style.display = 'none';
                doubleHeader.style.display = 'block';
                actionLabel.innerText = '双击触发动作模式';
                document.getElementById('toggle-enable-double').checked = !!editingBinding.has_double;
                document.getElementById('slider-double-ms').value = editingBinding.double_ms || 250;
                document.getElementById('label-double-ms').innerText = `${editingBinding.double_ms || 250}ms`;
                mode = editingBinding.double_type !== undefined ? editingBinding.double_type : 1;
                mod = editingBinding.double_mod || 0;
                key = editingBinding.double_key || 0;
                cons = editingBinding.double_cons || 0;
                target_layer = editingBinding.double_layer !== undefined ? editingBinding.double_layer : (currentEditingLayer === 1 ? 0 : 1);
            }

            selectedTargetLayer = target_layer;
            selectActionMode(mode);
            renderTriggerView(mode, mod, key, cons);
            renderTargetLayerButtons();
            document.getElementById('quick-key-select').value = '';
            if (mode === 1 || mode === 2 || mode === 7) {
                startKeyboardRecording();
            }
        }

        function switchTriggerTab(tab) {
            saveActiveTabToBinding();
            currentTriggerTab = tab;
            loadTriggerDataToUI(tab);
        }

        function selectActionMode(mode) {
            currentSelectedMode = mode;
            const inst = document.getElementById('recorder-instruction');
            const quickSelect = document.getElementById('quick-key-select');
            const recorderBox = document.getElementById('key-recorder-box');
            const advContainer = document.getElementById('adv-config-container');
            const layerBox = document.getElementById('layer-switch-config-box');
            const transBox = document.getElementById('layer-trans-config-box');

            document.querySelectorAll('.mode-card').forEach(c => c.classList.remove('selected'));
            const card = document.getElementById(`mode-card-${mode}`);
            if (card) card.classList.add('selected');

            if (mode === 9) {
                if (recorderBox) recorderBox.style.display = 'none';
                if (advContainer) advContainer.style.display = 'none';
                if (layerBox) layerBox.style.display = 'block';
                if (transBox) transBox.style.display = 'none';
                renderTargetLayerButtons();
            } else if (mode === 10) {
                if (recorderBox) recorderBox.style.display = 'none';
                if (advContainer) advContainer.style.display = 'none';
                if (layerBox) layerBox.style.display = 'none';
                if (transBox) transBox.style.display = 'block';
            } else {
                if (recorderBox) recorderBox.style.display = 'block';
                if (advContainer) advContainer.style.display = 'block';
                if (layerBox) layerBox.style.display = 'none';
                if (transBox) transBox.style.display = 'none';

                let curMod = parseInt(document.getElementById('adv-mod').value) || 0;
                let curCode = parseInt(document.getElementById('adv-code').value) || 0;

                if (mode === 4) {
                    if (inst) inst.innerHTML = '<b>多媒体控制模式</b>：在下方下拉框中选择具体的控制功能';
                    let curCons = curCode >= 500 ? curCode : 545;
                    renderTriggerView(4, 0, 0, curCons);
                    if (quickSelect) quickSelect.value = `c:0:${curCons}`;
                } else if (mode === 1) {
                    if (inst) inst.innerHTML = '<b>单次点按模式</b>：按下按键发送一次按键或组合快捷键（敲击键盘直接录制）';
                    let curKey = (curCode > 0 && curCode < 500) ? curCode : 0x28;
                    renderTriggerView(1, curMod, curKey, 0);
                    startKeyboardRecording();
                } else if (mode === 2) {
                    if (inst) inst.innerHTML = '<b>键盘直通模式</b>：按住按键时持续发送（敲击键盘直接录制）';
                    let curKey = (curCode > 0 && curCode < 500) ? curCode : 0x2C;
                    renderTriggerView(2, curMod, curKey, 0);
                    startKeyboardRecording();
                } else if (mode === 7) {
                    if (inst) inst.innerHTML = '<b>语音按键快捷键</b>：敲击键盘录制录音时发送的快捷键（如 Alt+, 或 Win+H）';
                    let curKey = (curCode > 0 && curCode < 500) ? curCode : 54;
                    renderTriggerView(7, curMod || 64, curKey, 0);
                    startKeyboardRecording();
                }
            }
        }

        function renderTriggerView(type, mod, key, cons) {
            const isVoice = (editingKey === 0x04 || editingKey === 0x3E);
            if (isVoice && currentEditingLayer === 0) {
                type = 7;
                cons = 0;
            } else if (type === 4 || cons > 0) {
                type = 4;
                key = 0;
                mod = 0;
            } else if (type === 9) {
                key = 0;
                mod = 0;
                cons = 0;
            } else if (type === 10) {
                key = 0;
                mod = 0;
                cons = 0;
            }

            currentSelectedMode = type;
            document.querySelectorAll('.mode-card').forEach(c => c.classList.remove('selected'));
            const card = document.getElementById(`mode-card-${type}`);
            if (card) card.classList.add('selected');

            document.getElementById('adv-mod').value = '0x' + (mod || 0).toString(16).toUpperCase().padStart(2, '0');
            const codeVal = (key || cons || 0);
            document.getElementById('adv-code').value = '0x' + codeVal.toString(16).toUpperCase().padStart(codeVal > 255 ? 4 : 2, '0');

            const display = document.getElementById('recorded-badge-display');
            if (!display) return;

            if (type === 9) {
                const tgt = selectedTargetLayer || 0;
                let tgtName = tgt === 0 ? '默认层' : `层${tgt}`;
                if (currentKeymap && currentKeymap.layers && currentKeymap.layers[tgt]) {
                    tgtName = currentKeymap.layers[tgt].name || tgtName;
                }
                display.innerHTML = `<span class="kbd-chip" style="border-color:var(--accent-cyan); color:var(--accent-cyan);">切入 ${tgtName}</span>`;
                return;
            }

            if (type === 10) {
                display.innerHTML = `<span class="kbd-chip" style="border-color:#93c5fd; color:#93c5fd;">继承默认层</span>`;
                return;
            }

            if (type === 0 || (!key && !cons && !mod)) {
                display.innerHTML = '<span style="color: var(--text-muted); font-size: 14px; font-weight: normal;">未设置（敲击键盘录制，或在下方选择）</span>';
                return;
            }

            let chips = [];
            if (mod & 0x01) chips.push('Ctrl');
            if (mod & 0x04) chips.push('Alt');
            if (mod & 0x02) chips.push('Shift');
            if (mod & 0x08) chips.push('Win');

            if (cons > 0) {
                const consMap = {
                    0x00E9: '音量+', 233: '音量+', 545: '音量+',
                    0x00EA: '音量-', 234: '音量-', 546: '音量-',
                    0x00E2: '静音',  226: '静音', 547: '静音',
                    0x00CD: '播放/暂停', 205: '播放/暂停', 516: '播放/暂停',
                    0x00B5: '下一曲', 181: '下一曲', 537: '下一曲',
                    0x00B6: '上一曲', 182: '上一曲', 538: '上一曲',
                    0x00B7: '停止',  183: '停止',
                    0x0032: '休眠',  50: '休眠', 530: '休眠',
                    0x0030: '电源',  48: '电源',
                    0x0224: '返回',  548: '返回', 558: '返回',
                    0x0223: '主页',  547: '主页', 557: '主页',
                    0x0225: '前进',  549: '前进',
                    0x0192: '计算器', 402: '计算器'
                };
                chips.push(consMap[cons] || `多媒体 0x${cons.toString(16).toUpperCase()}`);
            } else if (key > 0) {
                let keyName = `Key(0x${key.toString(16).toUpperCase()})`;
                for (let k in DOM_TO_HID) {
                    if (DOM_TO_HID[k] === key) {
                        keyName = k.replace('Key', '').replace('Digit', '').replace('Arrow', '');
                        break;
                    }
                }
                chips.push(keyName);
            }

            display.innerHTML = chips.map(c => `<span class="kbd-chip">${c}</span>`).join(' + ');
        }

        function onToggleTriggerEnable(trigger) {
            if (!editingBinding) return;
            if (trigger === 'long') {
                editingBinding.has_long = document.getElementById('toggle-enable-long').checked;
                showToast(editingBinding.has_long ? '已启用长按动作' : '已关闭长按动作');
            } else if (trigger === 'double') {
                editingBinding.has_double = document.getElementById('toggle-enable-double').checked;
                showToast(editingBinding.has_double ? '已启用双击动作' : '已关闭双击动作');
            }
        }

        function onTimingSliderChange(trigger, val) {
            if (!editingBinding) return;
            if (trigger === 'long') {
                editingBinding.long_ms = parseInt(val);
                document.getElementById('label-long-ms').innerText = `${val}ms`;
            } else if (trigger === 'double') {
                editingBinding.double_ms = parseInt(val);
                document.getElementById('label-double-ms').innerText = `${val}ms`;
            }
        }

        async function openRemapModal(keyVk, keyName) {
            editingKey = keyVk;
            normalizeKeymapConfig();
            const curLayer = currentKeymap.layers[currentEditingLayer];
            const isVoice = (keyVk === 0x04 || keyVk === 0x3E);

            let searchVk = keyVk;
            if (keyVk === 0xFF) searchVk = 0x66;
            if (keyVk === 0x4A) searchVk = 0x24;
            if (keyVk === 0x65) searchVk = 0x5D;
            if (keyVk === 0x35) searchVk = 0xC0;
            if (keyVk === 0x3E) searchVk = 0x04;

            const found = curLayer.bindings && curLayer.bindings.find(x => x.source_vk === searchVk);
            if (found) {
                editingBinding = JSON.parse(JSON.stringify(found));
            } else if (currentEditingLayer === 0 && FACTORY_KEYMAP[searchVk]) {
                editingBinding = JSON.parse(JSON.stringify(FACTORY_KEYMAP[searchVk]));
            } else if (currentEditingLayer > 0) {
                // Default to transparent on Layer 1~4
                editingBinding = {
                    source_vk: searchVk,
                    has_click: true, click_type: 10, click_mod: 0, click_key: 0, click_cons: 0, click_layer: 0,
                    has_long: false, long_ms: 600, long_type: 10, long_mod: 0, long_key: 0, long_cons: 0, long_layer: 0,
                    has_double: false, double_ms: 250, double_type: 10, double_mod: 0, double_key: 0, double_cons: 0, double_layer: 0
                };
            } else {
                editingBinding = {
                    source_vk: searchVk,
                    has_click: true, click_type: 2, click_mod: 0, click_key: 0, click_cons: 0, click_layer: 0,
                    has_long: false, long_ms: 600, long_type: 1, long_mod: 0, long_key: 0, long_cons: 0, long_layer: 0,
                    has_double: false, double_ms: 250, double_type: 1, double_mod: 0, double_key: 0, double_cons: 0, double_layer: 0
                };
            }

            // Show / hide transparent mode card based on whether this is Layer 0
            const transCard = document.getElementById('mode-card-10');
            if (transCard) {
                transCard.style.display = (currentEditingLayer === 0) ? 'none' : 'block';
            }

            // Sync switches and sliders before tab loads
            const longToggle = document.getElementById('toggle-enable-long');
            if (longToggle) longToggle.checked = !!editingBinding.has_long;
            const longSlider = document.getElementById('slider-long-ms');
            if (longSlider) longSlider.value = editingBinding.long_ms || 600;
            const longLabel = document.getElementById('label-long-ms');
            if (longLabel) longLabel.innerText = `${editingBinding.long_ms || 600}ms`;

            const doubleToggle = document.getElementById('toggle-enable-double');
            if (doubleToggle) doubleToggle.checked = !!editingBinding.has_double;
            const doubleSlider = document.getElementById('slider-double-ms');
            if (doubleSlider) doubleSlider.value = editingBinding.double_ms || 250;
            const doubleLabel = document.getElementById('label-double-ms');
            if (doubleLabel) doubleLabel.innerText = `${editingBinding.double_ms || 250}ms`;

            const layerPrefix = currentEditingLayer === 0 ? '' : `【${curLayer.name || ('层' + currentEditingLayer)}】`;
            document.getElementById('modal-title').innerText = (isVoice && currentEditingLayer === 0) ? '设置语音键快捷键' : `${layerPrefix}设置 ${keyName} 映射`;
            document.getElementById('modal-vk-badge').innerText = '0x' + searchVk.toString(16).toUpperCase().padStart(2, '0');
            document.getElementById('quick-optgroup-media').style.display = (isVoice && currentEditingLayer === 0) ? 'none' : 'block';

            if (isVoice && currentEditingLayer === 0) {
                document.getElementById('trigger-tab-bar').style.display = 'none';
                document.getElementById('mode-selector-grid').style.display = 'none';
                document.getElementById('voice-mode-locked-card').style.display = 'block';
                document.getElementById('long-press-header').style.display = 'none';
                document.getElementById('double-click-header').style.display = 'none';
                currentTriggerTab = 'click';
                selectActionMode(7);
                renderTriggerView(7, editingBinding.click_mod || 64, editingBinding.click_key || 54, 0);
            } else {
                document.getElementById('trigger-tab-bar').style.display = 'flex';
                document.getElementById('mode-selector-grid').style.display = 'grid';
                document.getElementById('voice-mode-locked-card').style.display = 'none';
                currentTriggerTab = 'click';
                loadTriggerDataToUI('click');
            }

            document.getElementById('quick-key-select').value = '';
            document.getElementById('remap-modal').style.display = 'flex';
        }

        function closeRemapModal() {
            document.getElementById('remap-modal').style.display = 'none';
        }

        function onQuickKeySelect(val) {
            if (!val) return;
            const parts = val.split(':');
            const prefix = parts[0];
            const mod = parseHexOrDec(parts[1]);
            const code = parseHexOrDec(parts[2]);
            const isVoice = (editingKey === 0x04 || editingKey === 0x3E);

            if (prefix === 'c') {
                if (isVoice && currentEditingLayer === 0) {
                    showToast('语音键专用于语音录音与呼出快捷键，不可设为多媒体键', true);
                    document.getElementById('quick-key-select').value = '';
                    return;
                }
                selectActionMode(4);
                renderTriggerView(4, 0, 0, code);
            } else if (prefix === 'm') {
                if (currentSelectedMode === 4 || currentSelectedMode === 9 || currentSelectedMode === 10) selectActionMode(2);
                renderTriggerView(currentSelectedMode, mod, 0, 0);
            } else if (prefix === 'k') {
                if (currentSelectedMode === 4 || currentSelectedMode === 9 || currentSelectedMode === 10) selectActionMode(2);
                renderTriggerView(currentSelectedMode, mod, code, 0);
            }
        }

        function onAdvInputChanged() {
            const mod = parseHexOrDec(document.getElementById('adv-mod').value);
            const code = parseHexOrDec(document.getElementById('adv-code').value);
            const isVoice = (editingKey === 0x04 || editingKey === 0x3E);

            if (isVoice && currentEditingLayer === 0) {
                renderTriggerView(7, mod, code, 0);
            } else if (currentSelectedMode === 4 || code >= 500) {
                renderTriggerView(4, 0, 0, code);
            } else if (currentSelectedMode === 9 || currentSelectedMode === 10) {
                renderTriggerView(currentSelectedMode, 0, 0, 0);
            } else {
                renderTriggerView(currentSelectedMode, mod, code, 0);
            }
        }

        function startKeyboardRecording() {
            const box = document.getElementById('key-recorder-box');
            if (box) {
                box.focus();
                box.classList.add('recording');
            }
        }

        // Global Keyboard Event Capturer for Ultra-Intuitive Remapping
        window.addEventListener('keydown', function(e) {
            const modal = document.getElementById('remap-modal');
            if (modal.style.display !== 'flex') return;

            // If user is in layer switch or transparent mode, ignore keypresses
            if (currentSelectedMode === 9 || currentSelectedMode === 10) return;

            // If user is typing in advanced numeric inputs, let it through
            if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;

            e.preventDefault();
            e.stopPropagation();

            // Ignore standalone modifier presses (wait for actual key)
            if (['Control', 'Alt', 'Shift', 'Meta'].includes(e.key)) return;

            let mod = 0;
            if (e.ctrlKey) mod |= 0x01; // LCTRL
            if (e.shiftKey) mod |= 0x02; // LSHIFT
            if (e.altKey) mod |= 0x04; // LALT
            if (e.metaKey) mod |= 0x08; // LGUI (Win)

            const hidCode = DOM_TO_HID[e.code] || 0;
            const isVoice = (editingKey === 0x04 || editingKey === 0x3E);

            if (hidCode > 0) {
                if (!(isVoice && currentEditingLayer === 0) && currentSelectedMode === 4) {
                    selectActionMode(2); // Auto switch from media to hold on keyboard input
                }
                renderTriggerView(currentSelectedMode, mod, hidCode, 0);
            }
        });

        function clearCurrentKeyBinding() {
            if (currentTriggerTab === 'click') {
                if (currentEditingLayer > 0) {
                    // Set to transparent
                    selectActionMode(10);
                    renderTriggerView(10, 0, 0, 0);
                    if (editingBinding) {
                        editingBinding.click_type = 10;
                        editingBinding.has_click = true;
                    }
                    showToast('已将单击设置为继承默认层');
                } else {
                    renderTriggerView(0, 0, 0, 0);
                    if (editingBinding) editingBinding.has_click = false;
                    showToast('已清空单击动作配置');
                }
            } else if (currentTriggerTab === 'long') {
                document.getElementById('toggle-enable-long').checked = false;
                if (editingBinding) editingBinding.has_long = false;
                renderTriggerView(0, 0, 0, 0);
                showToast('已关闭长按动作配置');
            } else if (currentTriggerTab === 'double') {
                document.getElementById('toggle-enable-double').checked = false;
                if (editingBinding) editingBinding.has_double = false;
                renderTriggerView(0, 0, 0, 0);
                showToast('已关闭双击动作配置');
            }
            document.getElementById('quick-key-select').value = '';
        }

        async function saveRemapConfig() {
            saveActiveTabToBinding();
            normalizeKeymapConfig();
            const curLayer = currentKeymap.layers[currentEditingLayer];
            if (!curLayer.bindings) curLayer.bindings = [];

            const isVoice = (editingKey === 0x04 || editingKey === 0x3E);
            if (isVoice && currentEditingLayer === 0) {
                editingBinding.source_vk = 0x04;
                editingBinding.has_click = true;
                editingBinding.click_type = 7;
                editingBinding.has_long = false;
                editingBinding.has_double = false;
            }

            // Sparse storage cleanup: if on Layer 1~4 and binding is completely transparent or disabled, remove it
            let isAllTransparent = false;
            if (currentEditingLayer > 0) {
                isAllTransparent = 
                    (!editingBinding.has_click || editingBinding.click_type === 10 || editingBinding.click_type === 0) &&
                    (!editingBinding.has_long || editingBinding.long_type === 10 || editingBinding.long_type === 0) &&
                    (!editingBinding.has_double || editingBinding.double_type === 10 || editingBinding.double_type === 0);
            }

            const idx = curLayer.bindings.findIndex(x => x.source_vk === editingBinding.source_vk);
            if (isAllTransparent) {
                if (idx >= 0) curLayer.bindings.splice(idx, 1);
            } else {
                if (idx >= 0) {
                    curLayer.bindings[idx] = editingBinding;
                } else {
                    curLayer.bindings.push(editingBinding);
                }
            }

            const ok = await saveKeymapToServer();
            if (ok) {
                closeRemapModal();
                showToast(`按键映射已保存至【${curLayer.name || (currentEditingLayer === 0 ? '默认层' : '层' + currentEditingLayer)}】`);
                renderLayerTabs();
                updateRemoteVisualTooltips();
            }
        }

        async function resetAllKeymaps() {
            if (!confirm('确定要将所有层级的按键映射与属性恢复为默认值吗？')) return;
            await fetch('/api/keymap/reset', { method: 'POST' });
            await loadKeymap();
            showToast('已恢复出厂默认层级映射');
        }

        async function scanBleDevices() {
            const container = document.getElementById('ble-dev-list');
            container.innerHTML = '正在扫描周围蓝牙设备 (4秒)...';
            try {
                const res = await fetch('/api/ble/scan');
                const d = await res.json();
                if (!d.devices || d.devices.length === 0) {
                    container.innerHTML = '<div style="color:var(--text-muted);">未发现附近设备，请确保遥控器处于配对广播状态。</div>';
                    return;
                }
                let html = '<div style="display:grid; gap:10px;">';
                d.devices.forEach(dev => {
                    html += `<div style="display:flex; justify-content:space-between; align-items:center; background:#0b0f17; padding:12px; border-radius:8px; border:1px solid #243247;">
                        <div><b>${dev.name}</b> <span style="font-size:12px; color:var(--text-muted); font-family:monospace;">(${dev.mac}) RSSI: ${dev.rssi}dBm</span></div>
                        <button class="btn" style="padding:6px 14px; font-size:12px;" onclick="connectMac('${dev.mac}')">连接</button>
                    </div>`;
                });
                html += '</div>';
                container.innerHTML = html;
            } catch(e){ container.innerText = '扫描出错: ' + e; }
        }

        async function connectMac(mac) {
            const res = await fetch('/api/ble/connect', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ mac }) });
            alert('正在连接目标蓝牙遥控器，请查看运行日志...');
        }

        async function scanWifiNetworks() {
            const list = document.getElementById('wifi-scan-list');
            list.innerHTML = '正在搜索周围 2.4GHz Wi-Fi 网络...';
            try {
                const res = await fetch('/api/wifi/scan');
                const d = await res.json();
                if (!d.networks || d.networks.length === 0) {
                    list.innerHTML = '未扫描到无线网络';
                    return;
                }
                let html = '<div style="display:flex; flex-wrap:wrap; gap:8px; margin-top:8px;">';
                d.networks.forEach(net => {
                    if (net.ssid) {
                        html += `<button class="btn btn-outline" style="font-size:12px; padding:6px 12px;" onclick="selectWifi('${net.ssid}')">${net.ssid} (${net.rssi}dBm)</button>`;
                    }
                });
                html += '</div>';
                list.innerHTML = html;
            } catch(e){ list.innerText = '搜索出错: ' + e; }
        }

        function selectWifi(ssid) {
            document.getElementById('wifi-ssid').value = ssid;
            document.getElementById('wifi-pass').focus();
        }

        async function refreshLogs() {
            try {
                const res = await fetch('/api/logs');
                const d = await res.json();
                const terminal = document.getElementById('log-terminal');
                terminal.innerText = d.logs.join('\n');
                terminal.scrollTop = terminal.scrollHeight;
            } catch(e){}
        }

        async function clearLogs() {
            await fetch('/api/logs/clear', { method: 'POST' });
            refreshLogs();
        }

        async function saveWifi() {
            const ssid = document.getElementById('wifi-ssid').value;
            const pass = document.getElementById('wifi-pass').value;
            if (!ssid) return alert('请输入 Wi-Fi 名称');
            await fetch('/api/wifi/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ ssid, pass }) });
            alert('Wi-Fi 配置已保存，ESP32 正在尝试连接！');
        }

        async function saveApConfig() {
            const ap_pass = document.getElementById('ap-pass').value.trim();
            if (ap_pass.length > 0 && ap_pass.length < 8) {
                alert('AP 热点密码至少需要 8 位字符（留空表示无密码开放热点）');
                return;
            }
            try {
                const res = await fetch('/api/wifi/ap', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ ap_pass })
                });
                const d = await res.json();
                if (res.ok) {
                    showToast(ap_pass ? 'AP 密码已保存并启用 WPA2 加密' : 'AP 密码已清空，恢复为开放热点');
                    fetchStatus();
                } else {
                    alert(d.message || d.error || '保存失败');
                }
            } catch(e) {
                alert('保存失败: ' + e.message);
            }
        }

        async function restartDevice() {
            if (!confirm('确定要重启 ESP32-S3 设备吗？')) return;
            await fetch('/api/system/restart', { method: 'POST' });
            alert('正在重启...');
        }

        function showToast(msg, isError = false) {
            let toast = document.getElementById('app-toast');
            if (!toast) {
                toast = document.createElement('div');
                toast.id = 'app-toast';
                toast.style.cssText = 'position:fixed; bottom:24px; left:50%; transform:translateX(-50%); padding:10px 20px; border-radius:10px; font-size:14px; font-weight:600; color:#fff; z-index:9999; box-shadow:0 8px 24px rgba(0,0,0,0.5); transition:opacity 0.3s; pointer-events:none;';
                document.body.appendChild(toast);
            }
            toast.style.background = isError ? '#ef4444' : '#10b981';
            toast.innerText = msg;
            toast.style.opacity = '1';
            clearTimeout(window.__toastTimer);
            window.__toastTimer = setTimeout(() => {
                toast.style.opacity = '0';
            }, 3000);
        }

        // Config Management: Export Keymap
        async function exportKeymapConfig() {
            try {
                const res = await fetch('/api/keymap');
                const data = await res.json();
                const jsonStr = JSON.stringify(data, null, 2);
                const blob = new Blob([jsonStr], { type: 'application/json' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                const now = new Date();
                const pad = n => String(n).padStart(2, '0');
                const dateStr = `${now.getFullYear()}${pad(now.getMonth()+1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
                a.href = url;
                a.download = `RemoteMapper_Keymap_${dateStr}.json`;
                document.body.appendChild(a);
                a.click();
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
                showToast('按键与层级配置已成功导出！');
            } catch(e) {
                showToast('导出配置失败: ' + e.message, true);
            }
        }

        // Config Management: Import Keymap
        function importKeymapConfig(e) {
            const file = e.target.files[0];
            if (!file) return;
            const reader = new FileReader();
            reader.onload = async function(evt) {
                try {
                    const data = JSON.parse(evt.target.result);
                    if (!data || !Array.isArray(data.layers)) {
                        throw new Error('无效的按键配置格式（必须包含 layers 数组）');
                    }
                    if (!confirm(`确定要导入按键配置文件「${file.name}」吗？\n\n导入后将覆盖所有按键映射与层级设置，保留现有的 Wi-Fi 与蓝牙配置。`)) {
                        return;
                    }
                    const res = await fetch('/api/keymap/save', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(data)
                    });
                    const ret = await res.json();
                    if (res.ok) {
                        showToast('按键配置导入成功并已保存！');
                        await loadKeymap();
                    } else {
                        showToast('导入失败: ' + (ret.error || '服务器拒绝该配置'), true);
                    }
                } catch(err) {
                    showToast('导入解析失败: ' + err.message, true);
                } finally {
                    e.target.value = '';
                }
            };
            reader.readAsText(file);
        }

        // Config Management: Reset Keymap Only (preserve WiFi & BLE)
        async function resetKeymapOnly() {
            if (!confirm('确定要清空所有按键映射与层级设置吗？\n\n所有按键将恢复为出厂默认映射规则。\n蓝牙遥控器配对与 Wi-Fi 网络参数将完全保留！')) {
                return;
            }
            try {
                const res = await fetch('/api/keymap/reset', { method: 'POST' });
                if (res.ok) {
                    await loadKeymap();
                    showToast('按键与层级数据已恢复出厂默认（蓝牙与Wi-Fi配置已保留）');
                } else {
                    showToast('重置按键映射失败', true);
                }
            } catch(e) {
                showToast('请求失败: ' + e.message, true);
            }
        }

        // Config Management: Factory Reset (wipe all NVS)
        async function resetNvsFactory() {
            if (!confirm('⚠️ 严重警告：此操作将清空 ESP32 内部的全部 NVS 持久化数据！\n\n包括：\n• 所有 Wi-Fi 密码与 AP 热点设置\n• 绑定的蓝牙遥控器配对信息\n• 所有自定义按键与多层级映射\n\n设备将自动重启并完全恢复出厂设置！是否继续？')) {
                return;
            }
            try {
                const res = await fetch('/api/nvs/reset', { method: 'POST' });
                const ret = await res.json();
                showToast(ret.message || '全部 NVS 已清空，设备重启中...');
                setTimeout(() => location.reload(), 3500);
            } catch(e) {
                showToast('操作失败: ' + e.message, true);
            }
        }

        // Config Management: Read-Only NVS Debug Viewer
        function onNvsDebugToggle(detailsEl) {
            const arrow = document.getElementById('nvs-details-arrow');
            if (arrow) arrow.innerText = detailsEl.open ? '▼' : '▶';
            if (detailsEl.open) {
                loadNvsReadOnly(false);
            }
        }

        async function loadNvsReadOnly(forceToast = false) {
            const viewer = document.getElementById('nvs-readonly-viewer');
            if (!viewer) return;
            if (forceToast) viewer.textContent = '正在刷新 NVS 内部数据...';
            try {
                const res = await fetch('/api/nvs');
                const json = await res.json();
                viewer.textContent = JSON.stringify(json, null, 2);
                if (forceToast) showToast('NVS 内部数据已刷新');
            } catch(e) {
                viewer.textContent = '读取 NVS 数据失败: ' + e.message;
                if (forceToast) showToast('读取 NVS 失败: ' + e.message, true);
            }
        }

        function copyNvsReadOnly() {
            const viewer = document.getElementById('nvs-readonly-viewer');
            if (!viewer) return;
            navigator.clipboard.writeText(viewer.textContent).then(() => {
                showToast('已复制 NVS 只读数据到剪贴板');
            }).catch(() => {
                showToast('复制失败，请手动选取文本');
            });
        }

        // Periodic background pollers
        setInterval(fetchKeyTelemetry, 100);
        setInterval(fetchStatus, 3000);
        setInterval(refreshLogs, 2000);
        loadKeymap();
        fetchStatus();
    </script>
</body>
</html>
)rawliteral";
