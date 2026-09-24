#pragma once

#include <string_view>

inline constexpr std::string_view kWebUi = R"PBHTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="theme-color" content="#111318">
<title>PencilBridge</title>
<style>
:root {
    color-scheme: dark;
    font-family: -apple-system, BlinkMacSystemFont, "SF Pro Text", system-ui, sans-serif;
    background: #111318;
    color: #f4f5f7;
}
* { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
html, body {
    margin: 0;
    width: 100%;
    height: 100%;
    overflow: hidden;
    background: #111318;
    user-select: none;
    -webkit-user-select: none;
    -webkit-touch-callout: none;
}
body { display: flex; flex-direction: column; overscroll-behavior: none; }
header {
    flex: 0 0 auto;
    display: flex;
    align-items: center;
    gap: 14px;
    padding: calc(10px + env(safe-area-inset-top)) 14px 10px;
    border-bottom: 1px solid #2a2e36;
    background: #171a20;
}
.brand { font-weight: 700; letter-spacing: .01em; white-space: nowrap; }
.status { display: flex; align-items: center; gap: 7px; min-width: 0; font-size: 14px; color: #aeb4bf; }
.dot { width: 9px; height: 9px; border-radius: 50%; background: #8d939e; flex: 0 0 auto; }
.dot.live { background: #63d58a; }
.spacer { flex: 1; }
label.toggle {
    display: flex;
    align-items: center;
    gap: 7px;
    font-size: 14px;
    color: #d5d9e0;
    white-space: nowrap;
}
input[type=checkbox] { width: 20px; height: 20px; }
.pressure-controls {
    flex: 0 0 auto;
    display: grid;
    grid-template-columns: minmax(180px, 1fr) minmax(180px, 1fr) auto;
    align-items: center;
    gap: 12px;
    padding: 8px 14px;
    border-bottom: 1px solid #2a2e36;
    background: #14171c;
}
.pressure-control {
    display: grid;
    grid-template-columns: auto 1fr auto;
    align-items: center;
    gap: 8px;
    min-width: 0;
    font-size: 12px;
    color: #c7ccd5;
}
.pressure-control input[type=range] { width: 100%; min-width: 80px; }
.pressure-control output {
    min-width: 42px;
    text-align: right;
    font: 12px ui-monospace, SFMono-Regular, Menlo, monospace;
}
.pressure-controls button {
    border: 1px solid #3a414d;
    border-radius: 7px;
    background: #20252d;
    color: #e4e7ec;
    padding: 6px 10px;
    font: inherit;
}
.metrics {
    display: grid;
    grid-template-columns: repeat(4, minmax(62px, 1fr));
    gap: 1px;
    background: #2a2e36;
    border-bottom: 1px solid #2a2e36;
}
.metric { background: #171a20; padding: 7px 10px; min-width: 0; }
.metric b { display: block; font-size: 11px; color: #858d99; font-weight: 600; text-transform: uppercase; letter-spacing: .06em; }
.metric span { display: block; margin-top: 2px; font: 13px ui-monospace, SFMono-Regular, Menlo, monospace; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
#pad {
    position: relative;
    flex: 1 1 auto;
    min-height: 0;
    touch-action: none;
    user-select: none;
    -webkit-user-select: none;
    background:
        linear-gradient(#1c2027 1px, transparent 1px),
        linear-gradient(90deg, #1c2027 1px, transparent 1px),
        #111318;
    background-size: 48px 48px;
    overflow: hidden;
}
#pad::after {
    content: "PENCIL / TOUCH SURFACE";
    position: absolute;
    left: 50%;
    top: 50%;
    transform: translate(-50%, -50%);
    color: #343a45;
    font-weight: 700;
    font-size: clamp(18px, 3vw, 34px);
    letter-spacing: .08em;
    pointer-events: none;
}
#cursor {
    position: absolute;
    width: 26px;
    height: 26px;
    left: 0;
    top: 0;
    margin: -13px 0 0 -13px;
    border: 2px solid #f4f5f7;
    border-radius: 50%;
    pointer-events: none;
    display: none;
    z-index: 3;
}
#cursor::before, #cursor::after {
    content: "";
    position: absolute;
    background: #f4f5f7;
}
#cursor::before { width: 1px; height: 38px; left: 11px; top: -8px; }
#cursor::after { width: 38px; height: 1px; top: 11px; left: -8px; }
#markupView {
    display: none;
    flex: 1 1 auto;
    user-select: none;
    -webkit-user-select: none;
    -webkit-touch-callout: none;
    min-height: 0;
    flex-direction: column;
    background: #0d0f13;
}
.markup-toolbar {
    flex: 0 0 auto;
    display: flex;
    gap: 10px;
    align-items: center;
    padding: 8px 12px;
    border-bottom: 1px solid #2a2e36;
    background: #171a20;
    min-width: 0;
}
.markup-toolbar label {
    display: flex;
    align-items: center;
    gap: 7px;
    white-space: nowrap;
    color: #d5d9e0;
}
.markup-toolbar .markup-finger-toggle {
    font-size: 13px;
}
.markup-input-mode {
    color: #858d99;
    font: 11px ui-monospace, SFMono-Regular, Menlo, monospace;
    white-space: nowrap;
}
body.markup-active .pressure-controls,
body.markup-active .metrics {
    display: none;
}
.markup-toolbar button {
    border: 1px solid #3a414d;
    border-radius: 7px;
    background: #20252d;
    color: #e4e7ec;
    padding: 7px 11px;
    font: inherit;
}
.markup-toolbar input[type=color] {
    width: 42px;
    height: 32px;
    padding: 0;
    border: 0;
    background: transparent;
}
.markup-toolbar input[type=range] { width: 130px; }
#markupStage {
    flex: 1 1 auto;
    min-height: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    overflow: hidden;
    padding: 10px 10px calc(10px + env(safe-area-inset-bottom));
}
#markupCanvas {
    display: block;
    max-width: none;
    max-height: none;
    background: #fff;
    box-shadow: 0 8px 28px rgba(0,0,0,.38);
    touch-action: none;
    user-select: none;
    -webkit-user-select: none;
    -webkit-touch-callout: none;
    -webkit-user-drag: none;
}
#gestureToast {
    position: fixed;
    left: 50%;
    top: 50%;
    transform: translate(-50%, -50%) scale(.92);
    padding: 12px 18px;
    border: 1px solid rgba(255,255,255,.24);
    border-radius: 10px;
    background: rgba(10,12,16,.82);
    color: #f4f5f7;
    font-weight: 700;
    letter-spacing: .08em;
    opacity: 0;
    pointer-events: none;
    transition: opacity 90ms ease, transform 90ms ease;
    z-index: 5;
}
#gestureToast.show {
    opacity: 1;
    transform: translate(-50%, -50%) scale(1);
}
#pressure {
    position: absolute;
    left: 14px;
    bottom: calc(14px + env(safe-area-inset-bottom));
    padding: 7px 10px;
    border-radius: 8px;
    background: rgba(15,17,21,.72);
    color: #c7ccd5;
    font: 12px ui-monospace, SFMono-Regular, Menlo, monospace;
    pointer-events: none;
}
@media (max-width: 650px) {
    header { flex-wrap: wrap; }

    body.phone-mode:not(.markup-active) .pressure-controls {
        display: none;
    }
    body.phone-mode:not(.markup-active) .metrics {
        grid-template-columns: repeat(2, 1fr);
    }
    body.phone-mode:not(.markup-active) header {
        padding-left: 9px;
        padding-right: 9px;
        gap: 8px;
    }
    body.phone-mode:not(.markup-active) .brand {
        font-size: 14px;
    }
    body.phone-mode:not(.markup-active) .status {
        font-size: 12px;
    }
    .status { order: 3; flex-basis: 100%; }
    .pressure-controls { grid-template-columns: 1fr; }
    .metrics { grid-template-columns: repeat(2, 1fr); }

    body.markup-active header {
        display: none;
    }
    .markup-toolbar {
        flex-wrap: wrap;
        gap: 7px 8px;
        padding: calc(7px + env(safe-area-inset-top)) 8px 7px;
    }
    .markup-toolbar strong {
        flex: 0 0 auto;
    }
    .markup-toolbar .spacer {
        display: none;
    }
    .markup-toolbar input[type=range] {
        width: min(34vw, 120px);
    }
    .markup-toolbar button {
        padding: 7px 9px;
        font-size: 13px;
    }
    .markup-toolbar .markup-finger-toggle {
        font-size: 12px;
    }
    #markupStage {
        padding: 6px 6px calc(6px + env(safe-area-inset-bottom));
    }
}
</style>
</head>
<body>
<header>
    <div class="brand">PencilBridge</div>
    <div class="status"><span id="dot" class="dot"></span><span id="statusText">Connecting…</span></div>
    <div class="spacer"></div>
    <label class="toggle"><input id="fingerDraw" type="checkbox"> Finger draw</label>
    <label class="toggle"><input id="fingerMouse" type="checkbox"> Finger mouse</label>
</header>

<div class="pressure-controls">
    <label class="pressure-control">
        <span>Full pressure</span>
        <input id="pressureMax" type="range" min="0.40" max="1.00" step="0.01" value="0.70">
        <output id="pressureMaxValue">0.70</output>
    </label>
    <label class="pressure-control">
        <span>Curve</span>
        <input id="pressureCurve" type="range" min="0.35" max="1.50" step="0.05" value="0.80">
        <output id="pressureCurveValue">0.80</output>
    </label>
    <button id="usePeak" type="button">Use peak</button>
</div>

<div class="metrics">
    <div class="metric"><b>Device</b><span id="device">—</span></div>
    <div class="metric"><b>Pressure raw → out</b><span id="pressureMetric">0.000 → 0.000</span></div>
    <div class="metric"><b>Tilt</b><span id="tilt">0°, 0°</span></div>
    <div class="metric"><b>Position / stroke</b><span id="position">0.000, 0.000</span></div>
</div>

<div id="gestureToast"></div>

<section id="markupView">
    <div class="markup-toolbar">
        <strong>Markup</strong><span id="markupInputMode" class="markup-input-mode"></span>
        <input id="markupColor" type="color" value="#ff3b30" aria-label="Markup color">
        <label class="markup-size-control">Size <input id="markupSize" type="range" min="2" max="28" step="1" value="7"></label>
        <label class="markup-finger-toggle"><input id="markupFingerDraw" type="checkbox"> Finger Draw</label>
        <button id="markupClear" type="button">Clear Markup</button>
        <div class="spacer"></div>
        <button id="markupClose" type="button">Close</button>
        <button id="markupSend" type="button">Send Back</button>
    </div>
    <div id="markupStage">
        <canvas id="markupCanvas"></canvas>
    </div>
</section>

<main id="pad" aria-label="Pencil input surface">
    <div id="cursor"></div>
    <div id="pressure">Touch with Apple Pencil to begin</div>
</main>

<script>
(function () {
    'use strict';

    var pad = document.getElementById('pad');
    var dot = document.getElementById('dot');
    var statusText = document.getElementById('statusText');
    var fingerDraw = document.getElementById('fingerDraw');
    var fingerMouse = document.getElementById('fingerMouse');
    var deviceText = document.getElementById('device');
    var pressureMetric = document.getElementById('pressureMetric');
    var pressureMax = document.getElementById('pressureMax');
    var pressureCurve = document.getElementById('pressureCurve');
    var pressureMaxValue = document.getElementById('pressureMaxValue');
    var pressureCurveValue = document.getElementById('pressureCurveValue');
    var usePeak = document.getElementById('usePeak');
    var tiltText = document.getElementById('tilt');
    var positionText = document.getElementById('position');
    var cursor = document.getElementById('cursor');
    var pressureText = document.getElementById('pressure');
    var gestureToast = document.getElementById('gestureToast');
    var markupView = document.getElementById('markupView');
    var markupInputMode = document.getElementById('markupInputMode');
    var markupStage = document.getElementById('markupStage');
    var markupCanvas = document.getElementById('markupCanvas');
    var markupColor = document.getElementById('markupColor');
    var markupSize = document.getElementById('markupSize');
    var markupFingerDraw = document.getElementById('markupFingerDraw');
    var markupClear = document.getElementById('markupClear');
    var markupClose = document.getElementById('markupClose');
    var markupSend = document.getElementById('markupSend');

    var socket = null;
    var reconnectTimer = null;
    var activeTouchId = null;
    var penActive = false;
    var penRestarts = 0;
    var penCancels = 0;
    var peakPressure = 0;
    var padRect = null;
    var latestTelemetry = null;
    var lastInputTime = 0;
    var strokeMaxGapMs = 0;

    var touchGesture = new Map();
    var touchGestureStart = 0;
    var touchGestureMaxCount = 0;
    var touchGestureMoved = false;
    var touchGestureCancelled = false;
    var pendingFingerDown = null;
    var pendingFingerMode = null;
    var pendingFingerTimer = null;
    var activeFingerSample = null;
    var activeFingerMode = null;
    var nativePadStylusId = null;
    var toastTimer = null;

    var GESTURE_MAX_MS = 420;
    var GESTURE_MOVE_PX = 28;
    var FINGER_MOUSE_DELAY_MS = 120;

    var markupImage = null;
    var markupDrawing = false;
    var markupPointerId = null;
    var markupLastX = 0;
    var markupLastY = 0;
    var markupDidMove = false;
    var markupRect = null;
    var markupRecoveredStrokes = 0;
    var markupTouchId = null;
    var markupTouchType = null;

    function loadNumber(key, fallback) {
        var value = Number(localStorage.getItem(key));
        return Number.isFinite(value) ? value : fallback;
    }

    pressureMax.value = String(Math.max(0.40, Math.min(1.00, loadNumber('pencilbridge.pressureMax', 0.70))));
    pressureCurve.value = String(Math.max(0.35, Math.min(1.50, loadNumber('pencilbridge.pressureCurve', 0.80))));

    function isAppleTouchDevice() {
        var ua = navigator.userAgent || '';
        return /iPad|iPhone|iPod/i.test(ua) ||
            (navigator.platform === 'MacIntel' &&
             navigator.maxTouchPoints > 1);
    }

    function isLikelyPhone() {
        return /iPhone|iPod/i.test(navigator.userAgent || '') ||
            (navigator.maxTouchPoints > 0 &&
             Math.min(window.screen.width, window.screen.height) <= 480);
    }

    var useNativeIosTouchInk =
        isAppleTouchDevice() && ('ontouchstart' in window);

    if (isLikelyPhone()) {
        document.body.classList.add('phone-mode');
    }

    markupInputMode.textContent =
        useNativeIosTouchInk ? 'iOS native ink' : 'pointer ink';

    if (useNativeIosTouchInk) {
        pressureText.textContent = isLikelyPhone()
            ? 'iOS native touch input'
            : 'iOS native Pencil input';
    }

    var savedFingerDraw = localStorage.getItem('pencilbridge.markupFingerDraw');
    markupFingerDraw.checked =
        savedFingerDraw === null
            ? isLikelyPhone()
            : savedFingerDraw === '1';

    markupFingerDraw.addEventListener('change', function () {
        localStorage.setItem(
            'pencilbridge.markupFingerDraw',
            markupFingerDraw.checked ? '1' : '0');
    });

    var savedTabletFingerDraw =
        localStorage.getItem('pencilbridge.fingerDraw');
    fingerDraw.checked =
        savedTabletFingerDraw === null
            ? isLikelyPhone()
            : savedTabletFingerDraw === '1';

    fingerDraw.addEventListener('change', function () {
        localStorage.setItem(
            'pencilbridge.fingerDraw',
            fingerDraw.checked ? '1' : '0');

        if (fingerDraw.checked) {
            fingerMouse.checked = false;
        }
        cancelActiveFinger();
    });

    function refreshPressureControls() {
        pressureMaxValue.textContent = Number(pressureMax.value).toFixed(2);
        pressureCurveValue.textContent = Number(pressureCurve.value).toFixed(2);
    }

    function mappedPressure(raw) {
        raw = Math.max(0, Math.min(1, Number(raw) || 0));
        var maxValue = Math.max(0.01, Number(pressureMax.value) || 0.70);
        var curve = Math.max(0.05, Number(pressureCurve.value) || 0.80);
        var normalized = Math.max(0, Math.min(1, raw / maxValue));
        return Math.pow(normalized, curve);
    }

    pressureMax.addEventListener('input', function () {
        localStorage.setItem('pencilbridge.pressureMax', pressureMax.value);
        refreshPressureControls();
    });

    pressureCurve.addEventListener('input', function () {
        localStorage.setItem('pencilbridge.pressureCurve', pressureCurve.value);
        refreshPressureControls();
    });

    usePeak.addEventListener('click', function () {
        if (peakPressure > 0.05) {
            pressureMax.value = String(Math.max(0.40, Math.min(1.00, peakPressure)).toFixed(2));
            localStorage.setItem('pencilbridge.pressureMax', pressureMax.value);
            refreshPressureControls();
        }
    });

    refreshPressureControls();

    fingerMouse.addEventListener('change', function () {
        if (fingerMouse.checked) {
            fingerDraw.checked = false;
            localStorage.setItem('pencilbridge.fingerDraw', '0');
        }
        cancelActiveFinger();
    });

    function setConnection(isLive, text) {
        dot.classList.toggle('live', isLive);
        statusText.textContent = text;
    }

    function connect() {
        clearTimeout(reconnectTimer);
        setConnection(false, 'Connecting…');

        try {
            socket = new WebSocket('ws://' + location.host + '/ws');
            socket.binaryType = 'arraybuffer';
        } catch (error) {
            setConnection(false, 'Connection failed');
            reconnectTimer = setTimeout(connect, 1200);
            return;
        }

        socket.addEventListener('open', function () {
            setConnection(true, 'Connected');
        });

        socket.addEventListener('close', function () {
            setConnection(false, 'Disconnected — retrying');
            reconnectTimer = setTimeout(connect, 1200);
        });

        socket.addEventListener('error', function () {
            setConnection(false, 'Connection error');
        });

        socket.addEventListener('message', function (event) {
            if (typeof event.data === 'string') {
                return;
            }
            openMarkupImage(event.data);
        });
    }


    function fitMarkupCanvas() {
        if (!markupCanvas.width ||
            !markupCanvas.height ||
            !document.body.classList.contains('markup-active')) {
            return;
        }
        var rect = markupStage.getBoundingClientRect();
        var availableWidth = Math.max(1, rect.width - 12);
        var availableHeight = Math.max(1, rect.height - 12);
        var scale = Math.min(
            availableWidth / markupCanvas.width,
            availableHeight / markupCanvas.height,
            1);
        markupCanvas.style.width =
            Math.max(1, Math.round(markupCanvas.width * scale)) + 'px';
        markupCanvas.style.height =
            Math.max(1, Math.round(markupCanvas.height * scale)) + 'px';
        markupRect = null;
    }

    function redrawMarkupBase() {
        if (!markupImage) {
            return;
        }
        var ctx = markupCanvas.getContext('2d');
        ctx.clearRect(0, 0, markupCanvas.width, markupCanvas.height);
        ctx.drawImage(markupImage, 0, 0, markupCanvas.width, markupCanvas.height);
    }

    function openMarkupImage(buffer) {
        var blob = new Blob([buffer], { type: 'image/png' });
        var url = URL.createObjectURL(blob);
        var image = new Image();

        image.onload = function () {
            URL.revokeObjectURL(url);
            markupImage = image;
            markupCanvas.width = image.naturalWidth;
            markupCanvas.height = image.naturalHeight;
            redrawMarkupBase();
            markupDrawing = false;
            markupPointerId = null;
            markupRect = null;
            pad.style.display = 'none';
            markupView.style.display = 'flex';
            document.body.classList.add('markup-active');
            requestAnimationFrame(fitMarkupCanvas);
            showGestureToast('CLIP READY');
        };

        image.onerror = function () {
            URL.revokeObjectURL(url);
            showGestureToast('IMAGE ERROR');
        };

        image.src = url;
    }

    function closeMarkup() {
        markupDrawing = false;
        markupPointerId = null;
        markupRect = null;
        markupView.style.display = 'none';
        document.body.classList.remove('markup-active');
        pad.style.display = '';
        refreshPadRect();
    }

    function canDrawMarkup(event) {
        if (event.pointerType === 'pen') {
            return true;
        }
        return event.pointerType === 'touch' && markupFingerDraw.checked;
    }

    function refreshMarkupRect() {
        markupRect = markupCanvas.getBoundingClientRect();
    }

    function markupPointFromClient(clientX, clientY) {
        if (!markupRect) {
            refreshMarkupRect();
        }

        return {
            x: (clientX - markupRect.left) *
                markupCanvas.width / Math.max(1, markupRect.width),
            y: (clientY - markupRect.top) *
                markupCanvas.height / Math.max(1, markupRect.height)
        };
    }

    function markupPoint(event) {
        return markupPointFromClient(event.clientX, event.clientY);
    }

    function normalizedMarkupPressure(pointerType, pressure) {
        if (pointerType === 'touch') {
            return 0.5;
        }

        pressure = Number(pressure || 0);
        return Math.max(0.12, Math.min(1, pressure));
    }

    function markupBrushWidthFor(pointerType, pressure) {
        if (!markupRect) {
            refreshMarkupRect();
        }

        // The slider describes visible-screen pixels. Convert that into source
        // image pixels so the brush stays visually consistent at any fit scale.
        var sourceScale =
            markupCanvas.width / Math.max(1, markupRect.width);
        return Number(markupSize.value) *
            sourceScale *
            (0.55 + normalizedMarkupPressure(pointerType, pressure) * 0.9);
    }

    function stampMarkupPointFor(pointerType, pressure, x, y) {
        var ctx = markupCanvas.getContext('2d');
        var radius = Math.max(
            0.75,
            markupBrushWidthFor(pointerType, pressure) * 0.5);
        ctx.fillStyle = markupColor.value;
        ctx.beginPath();
        ctx.arc(x, y, radius, 0, Math.PI * 2);
        ctx.fill();
    }

    function drawMarkupSegmentFor(
        pointerType,
        pressure,
        fromX,
        fromY,
        toX,
        toY) {
        var ctx = markupCanvas.getContext('2d');
        ctx.strokeStyle = markupColor.value;
        ctx.lineWidth = markupBrushWidthFor(pointerType, pressure);
        ctx.lineCap = 'round';
        ctx.lineJoin = 'round';
        ctx.beginPath();
        ctx.moveTo(fromX, fromY);
        ctx.lineTo(toX, toY);
        ctx.stroke();
    }

    function stampMarkupPoint(event, x, y) {
        stampMarkupPointFor(
            event.pointerType,
            event.pressure,
            x,
            y);
    }

    function drawMarkupSegment(event, fromX, fromY, toX, toY) {
        drawMarkupSegmentFor(
            event.pointerType,
            event.pressure,
            fromX,
            fromY,
            toX,
            toY);
    }

    function resetMarkupStroke() {
        markupDrawing = false;
        markupPointerId = null;
        markupTouchId = null;
        markupTouchType = null;
        markupDidMove = false;
        markupRect = null;
    }

    markupCanvas.addEventListener('pointerdown', function (event) {
        if (useNativeIosTouchInk || !canDrawMarkup(event)) {
            return;
        }

        event.preventDefault();
        event.stopPropagation();

        // Safari can occasionally lose the UP/capture transition for a fast
        // Pencil stroke. Never reject the next physical DOWN because of stale
        // JavaScript state: retire the old stroke and start this one immediately.
        if (markupDrawing || markupPointerId !== null) {
            markupRecoveredStrokes += 1;
            resetMarkupStroke();
        }

        refreshMarkupRect();

        var point = markupPoint(event);
        markupDrawing = true;
        markupPointerId = event.pointerId;
        markupDidMove = false;
        markupLastX = point.x;
        markupLastY = point.y;

        // Draw immediately. Very quick handwriting marks can otherwise be only
        // DOWN/UP with no MOVE event between them.
        stampMarkupPoint(event, point.x, point.y);
    }, { passive: false });

    function handleMarkupMove(event) {
        if (useNativeIosTouchInk ||
            !markupDrawing ||
            event.pointerId !== markupPointerId ||
            !canDrawMarkup(event)) {
            return;
        }

        event.preventDefault();
        event.stopPropagation();

        var samples = [];
        if (typeof event.getCoalescedEvents === 'function') {
            samples = event.getCoalescedEvents();
        }
        if (!samples || samples.length === 0) {
            samples = [event];
        }

        for (var i = 0; i < samples.length; ++i) {
            var point = markupPoint(samples[i]);
            if (point.x === markupLastX && point.y === markupLastY) {
                continue;
            }

            drawMarkupSegment(
                samples[i],
                markupLastX,
                markupLastY,
                point.x,
                point.y);
            markupLastX = point.x;
            markupLastY = point.y;
            markupDidMove = true;
        }
    }

    window.addEventListener(
        'pointermove',
        handleMarkupMove,
        { capture: true, passive: false });

    function endMarkupStroke(event, cancelled) {
        if (event.pointerId !== markupPointerId) {
            return;
        }

        event.preventDefault();
        event.stopPropagation();

        if (markupDrawing && !cancelled) {
            var point = markupPoint(event);
            if (point.x !== markupLastX || point.y !== markupLastY) {
                drawMarkupSegment(
                    event,
                    markupLastX,
                    markupLastY,
                    point.x,
                    point.y);
                markupLastX = point.x;
                markupLastY = point.y;
                markupDidMove = true;
            }
        }

        resetMarkupStroke();
    }

    window.addEventListener('pointerup', function (event) {
        if (!useNativeIosTouchInk &&
            markupDrawing &&
            event.pointerId === markupPointerId) {
            endMarkupStroke(event, false);
        }
    }, { capture: true, passive: false });

    // Do not let pointercancel poison the next stroke. Safari can cancel a
    // Pencil sequence during fast handwriting; the next physical DOWN always
    // resets stale state and starts cleanly.
    window.addEventListener('pointercancel', function (event) {
        if (!useNativeIosTouchInk &&
            markupDrawing &&
            event.pointerId === markupPointerId) {
            markupRecoveredStrokes += 1;
            resetMarkupStroke();
        }
    }, { capture: true, passive: false });

    function touchKind(touch) {
        return touch && touch.touchType === 'stylus'
            ? 'pen'
            : 'touch';
    }

    function canDrawMarkupTouch(touch) {
        var kind = touchKind(touch);
        return kind === 'pen' ||
            (kind === 'touch' && markupFingerDraw.checked);
    }

    function findTouchById(list, identifier) {
        for (var i = 0; i < list.length; ++i) {
            if (list[i].identifier === identifier) {
                return list[i];
            }
        }
        return null;
    }

    function touchPressure(touch) {
        var kind = touchKind(touch);
        if (kind === 'touch') {
            return 0.5;
        }

        var force = Number(touch.force || 0);
        return Math.max(0.12, Math.min(1, force));
    }

    function beginMarkupTouch(touch) {
        resetMarkupStroke();
        refreshMarkupRect();

        var kind = touchKind(touch);
        var point = markupPointFromClient(
            touch.clientX,
            touch.clientY);

        markupDrawing = true;
        markupTouchId = touch.identifier;
        markupTouchType = kind;
        markupDidMove = false;
        markupLastX = point.x;
        markupLastY = point.y;

        stampMarkupPointFor(
            kind,
            touchPressure(touch),
            point.x,
            point.y);
    }

    function moveMarkupTouch(touch) {
        if (!markupDrawing ||
            touch.identifier !== markupTouchId) {
            return;
        }

        var point = markupPointFromClient(
            touch.clientX,
            touch.clientY);

        if (point.x === markupLastX &&
            point.y === markupLastY) {
            return;
        }

        drawMarkupSegmentFor(
            markupTouchType || touchKind(touch),
            touchPressure(touch),
            markupLastX,
            markupLastY,
            point.x,
            point.y);

        markupLastX = point.x;
        markupLastY = point.y;
        markupDidMove = true;
    }

    function endMarkupTouch(touch, cancelled) {
        if (!markupDrawing ||
            !touch ||
            touch.identifier !== markupTouchId) {
            return;
        }

        if (!cancelled) {
            moveMarkupTouch(touch);
        } else {
            markupRecoveredStrokes += 1;
        }

        resetMarkupStroke();
        markupTouchId = null;
        markupTouchType = null;
    }

    markupCanvas.addEventListener('touchstart', function (event) {
        if (!useNativeIosTouchInk) {
            return;
        }

        for (var i = 0; i < event.changedTouches.length; ++i) {
            var touch = event.changedTouches[i];
            if (!canDrawMarkupTouch(touch)) {
                continue;
            }

            event.preventDefault();
            event.stopPropagation();

            // Touch identifiers are scoped to the physical contact, which avoids
            // Safari PointerEvent pointer-id reuse / late-cancel corruption.
            beginMarkupTouch(touch);
            return;
        }
    }, { passive: false });

    markupCanvas.addEventListener('touchmove', function (event) {
        if (!useNativeIosTouchInk ||
            !markupDrawing ||
            markupTouchId === null) {
            return;
        }

        var touch = findTouchById(
            event.changedTouches,
            markupTouchId);
        if (!touch) {
            touch = findTouchById(
                event.touches,
                markupTouchId);
        }
        if (!touch) {
            return;
        }

        event.preventDefault();
        event.stopPropagation();
        moveMarkupTouch(touch);
    }, { passive: false });

    markupCanvas.addEventListener('touchend', function (event) {
        if (!useNativeIosTouchInk ||
            !markupDrawing ||
            markupTouchId === null) {
            return;
        }

        var touch = findTouchById(
            event.changedTouches,
            markupTouchId);
        if (!touch) {
            return;
        }

        event.preventDefault();
        event.stopPropagation();
        endMarkupTouch(touch, false);
    }, { passive: false });

    markupCanvas.addEventListener('touchcancel', function (event) {
        if (!useNativeIosTouchInk ||
            !markupDrawing ||
            markupTouchId === null) {
            return;
        }

        var touch = findTouchById(
            event.changedTouches,
            markupTouchId);

        event.preventDefault();
        event.stopPropagation();

        if (touch) {
            endMarkupTouch(touch, true);
        } else {
            markupRecoveredStrokes += 1;
            resetMarkupStroke();
            markupTouchId = null;
            markupTouchType = null;
        }
    }, { passive: false });

    ['click', 'dblclick', 'contextmenu', 'dragstart', 'selectstart'].forEach(function (type) {
        markupCanvas.addEventListener(type, function (event) {
            event.preventDefault();
            event.stopPropagation();
            if (window.getSelection) {
                var selection = window.getSelection();
                if (selection) {
                    selection.removeAllRanges();
                }
            }
        }, { passive: false });
    });

    ['gesturestart', 'gesturechange', 'gestureend'].forEach(function (type) {
        markupCanvas.addEventListener(type, function (event) {
            event.preventDefault();
            event.stopPropagation();
        }, { passive: false });
    });

    document.addEventListener('selectionchange', function () {
        if (document.body.classList.contains('markup-active') && window.getSelection) {
            var selection = window.getSelection();
            if (selection && selection.rangeCount > 0) {
                selection.removeAllRanges();
            }
        }
    });

    markupFingerDraw.addEventListener('change', function () {
        if (!markupFingerDraw.checked &&
            markupDrawing &&
            markupPointerId !== null) {
            resetMarkupStroke();
        }
    });

    markupClear.addEventListener('click', function () {
        redrawMarkupBase();
        showGestureToast('CLEARED');
    });

    markupClose.addEventListener('click', closeMarkup);

    markupSend.addEventListener('click', function () {
        if (!socket || socket.readyState !== WebSocket.OPEN || !markupCanvas.width) {
            showGestureToast('NOT CONNECTED');
            return;
        }

        markupCanvas.toBlob(function (blob) {
            if (!blob) {
                showGestureToast('EXPORT ERROR');
                return;
            }
            blob.arrayBuffer().then(function (buffer) {
                socket.send(buffer);
                showGestureToast('SENT TO PC');
            });
        }, 'image/png');
    });

    function refreshPadRect() {
        padRect = pad.getBoundingClientRect();
    }

    function normalized(event) {
        if (!padRect) {
            refreshPadRect();
        }
        var rect = padRect;
        var x = Math.max(0, Math.min(1, (event.clientX - rect.left) / Math.max(1, rect.width)));
        var y = Math.max(0, Math.min(1, (event.clientY - rect.top) / Math.max(1, rect.height)));
        return { x: x, y: y, rect: rect };
    }

    function queueTelemetry(event, point) {
        latestTelemetry = {
            pointerType: event.pointerType || 'unknown',
            pressure: Number(event.pressure || 0),
            tiltX: Math.round(event.tiltX || 0),
            tiltY: Math.round(event.tiltY || 0),
            clientX: event.clientX,
            clientY: event.clientY,
            x: point.x,
            y: point.y
        };
    }

    function updateReadout(sample) {
        var type = sample.pointerType;
        var rawPressure = sample.pressure;
        var outputPressure = type === 'pen' ? mappedPressure(rawPressure) : rawPressure;
        if (type === 'pen' && rawPressure > peakPressure) {
            peakPressure = rawPressure;
        }
        deviceText.textContent = type === 'pen' ? 'Apple Pencil / pen' : type;
        pressureMetric.textContent = rawPressure.toFixed(3) + ' → ' + outputPressure.toFixed(3);
        tiltText.textContent = sample.tiltX + '°, ' + sample.tiltY + '°';
        positionText.textContent =
            sample.x.toFixed(3) + ', ' + sample.y.toFixed(3) +
            '  r' + penRestarts + '/c' + penCancels +
            '  gap≤' + strokeMaxGapMs.toFixed(0) + 'ms';

        cursor.style.display = 'block';
        cursor.style.left = (sample.clientX - padRect.left) + 'px';
        cursor.style.top = (sample.clientY - padRect.top) + 'px';
        var p = outputPressure;
        var scale = 0.8 + p * 0.8;
        cursor.style.transform = 'scale(' + scale.toFixed(2) + ')';
        pressureText.textContent =
            type + '  raw ' + rawPressure.toFixed(3) +
            '  out ' + outputPressure.toFixed(3) +
            '  peak ' + peakPressure.toFixed(3);
    }

    function touchTilt(touch) {
        var altitude = Number(touch.altitudeAngle);
        var azimuth = Number(touch.azimuthAngle);

        if (!Number.isFinite(altitude) ||
            !Number.isFinite(azimuth) ||
            altitude <= 0) {
            return { x: 0, y: 0 };
        }

        var tanAltitude = Math.tan(altitude);
        if (Math.abs(tanAltitude) < 0.0001) {
            tanAltitude = 0.0001;
        }

        return {
            x: Math.max(-90, Math.min(
                90,
                Math.atan(Math.cos(azimuth) / tanAltitude) * 180 / Math.PI)),
            y: Math.max(-90, Math.min(
                90,
                Math.atan(Math.sin(azimuth) / tanAltitude) * 180 / Math.PI))
        };
    }

    function nativeTouchSample(touch, pointerType) {
        var tilt = pointerType === 'pen'
            ? touchTilt(touch)
            : { x: 0, y: 0 };

        return {
            pointerType: pointerType,
            pointerId: touch.identifier,
            clientX: touch.clientX,
            clientY: touch.clientY,
            pressure: pointerType === 'pen'
                ? Math.max(0, Math.min(1, Number(touch.force || 0)))
                : 0.5,
            tiltX: tilt.x,
            tiltY: tilt.y
        };
    }

    function snapshotPointer(event) {
        return {
            pointerType: event.pointerType || 'unknown',
            pointerId: event.pointerId || 0,
            clientX: event.clientX,
            clientY: event.clientY,
            pressure: Number(event.pressure || 0),
            tiltX: Number(event.tiltX || 0),
            tiltY: Number(event.tiltY || 0)
        };
    }

    function showGestureToast(text) {
        gestureToast.textContent = text;
        gestureToast.classList.add('show');
        clearTimeout(toastTimer);
        toastTimer = setTimeout(function () {
            gestureToast.classList.remove('show');
        }, 420);
    }

    function sendCommand(command) {
        if (!socket || socket.readyState !== WebSocket.OPEN) {
            return;
        }
        socket.send('cmd,' + command);
        showGestureToast(command === 'undo' ? 'UNDO' : 'REDO');
    }

    function clearPendingFingerTimer() {
        if (pendingFingerTimer !== null) {
            clearTimeout(pendingFingerTimer);
            pendingFingerTimer = null;
        }
    }

    function requestedFingerMode() {
        if (fingerDraw.checked) {
            return 'draw';
        }
        if (fingerMouse.checked) {
            return 'mouse';
        }
        return null;
    }

    function cancelActiveFinger() {
        clearPendingFingerTimer();
        pendingFingerDown = null;
        pendingFingerMode = null;

        if (activeTouchId !== null && activeFingerSample) {
            sendEvent(activeFingerSample, 'c');
        }

        activeFingerSample = null;
        activeFingerMode = null;
    }

    function commitPendingFingerDown() {
        clearPendingFingerTimer();
        if (!pendingFingerDown ||
            !pendingFingerMode ||
            touchGestureMaxCount >= 2) {
            pendingFingerDown = null;
            pendingFingerMode = null;
            return;
        }

        activeFingerMode = pendingFingerMode;
        activeFingerSample = pendingFingerDown;
        sendEvent(pendingFingerDown, 'd');
        pendingFingerDown = null;
        pendingFingerMode = null;
    }

    function beginTouchGesture(event) {
        if (touchGesture.size === 0) {
            touchGestureStart = performance.now();
            touchGestureMaxCount = 0;
            touchGestureMoved = false;
            touchGestureCancelled = false;
        }

        touchGesture.set(event.pointerId, {
            startX: event.clientX,
            startY: event.clientY,
            lastX: event.clientX,
            lastY: event.clientY
        });

        touchGestureMaxCount = Math.max(touchGestureMaxCount, touchGesture.size);

        if (touchGestureMaxCount >= 2) {
            clearPendingFingerTimer();
            pendingFingerDown = null;
            pendingFingerMode = null;

            if (activeTouchId !== null && activeFingerSample) {
                sendEvent(activeFingerSample, 'c');
                activeFingerSample = null;
                activeFingerMode = null;
            }
        }
    }

    function moveTouchGesture(event) {
        var touch = touchGesture.get(event.pointerId);
        if (!touch) {
            return;
        }

        touch.lastX = event.clientX;
        touch.lastY = event.clientY;
        var dx = event.clientX - touch.startX;
        var dy = event.clientY - touch.startY;
        if ((dx * dx + dy * dy) > GESTURE_MOVE_PX * GESTURE_MOVE_PX) {
            touchGestureMoved = true;
        }
    }

    function finishTouchGesture(event, cancelled) {
        if (!touchGesture.has(event.pointerId)) {
            return false;
        }

        if (cancelled) {
            touchGestureCancelled = true;
        }

        touchGesture.delete(event.pointerId);
        var wasMultiTouch = touchGestureMaxCount >= 2;

        if (touchGesture.size !== 0) {
            return wasMultiTouch;
        }

        var duration = performance.now() - touchGestureStart;
        var count = touchGestureMaxCount;
        var recognized =
            !touchGestureCancelled &&
            !touchGestureMoved &&
            duration <= GESTURE_MAX_MS &&
            (count === 2 || count === 3);

        if (recognized) {
            sendCommand(count === 2 ? 'undo' : 'redo');
        }

        touchGestureStart = 0;
        touchGestureMaxCount = 0;
        touchGestureMoved = false;
        touchGestureCancelled = false;
        return wasMultiTouch || recognized;
    }

    function shouldForward(event) {
        if (event.pointerType === 'pen') {
            return true;
        }
        if (event.pointerType === 'touch') {
            return activeFingerMode !== null ||
                pendingFingerMode !== null ||
                requestedFingerMode() !== null;
        }
        return false;
    }

    function sendEvent(event, phase) {
        if (!socket || socket.readyState !== WebSocket.OPEN || !shouldForward(event)) {
            return;
        }

        var penLike =
            event.pointerType === 'pen' ||
            (event.pointerType === 'touch' && activeFingerMode === 'draw');

        if (penLike) {
            if (phase === 'd') {
                penActive = true;
            } else if (phase === 'm' && !penActive) {
                // Safari can occasionally cancel/recreate a Pencil pointer during a
                // continuous physical contact. If pressure says the tip is still down,
                // transparently begin a fresh synthetic stroke instead of going silent.
                if (event.pointerType === 'touch' ||
                    Number(event.pressure || 0) > 0.001) {
                    penActive = true;
                    penRestarts += 1;
                    phase = 'd';
                } else {
                    return;
                }
            } else if ((phase === 'u' || phase === 'c') && !penActive) {
                return;
            }
        }

        if (event.pointerType === 'touch') {
            if (phase === 'd') {
                if (activeTouchId !== null) {
                    return;
                }
                activeTouchId = event.pointerId;
            } else if (activeTouchId !== event.pointerId) {
                return;
            }
        }

        var now = performance.now();
        if (penLike) {
            if (phase === 'd') {
                lastInputTime = now;
                strokeMaxGapMs = 0;
            } else if (penActive && lastInputTime > 0) {
                var gap = now - lastInputTime;
                if (gap > strokeMaxGapMs) {
                    strokeMaxGapMs = gap;
                }
                lastInputTime = now;
            }

            if (phase === 'u' || phase === 'c') {
                lastInputTime = 0;
            }
        }

        var point = normalized(event);
        queueTelemetry(event, point);

        var device = penLike ? 'p' : 't';
        var outputPressure = event.pointerType === 'pen'
            ? mappedPressure(event.pressure)
            : (penLike ? 0.55 : Number(event.pressure || 0));
        var message = [
            device,
            phase,
            point.x.toFixed(6),
            point.y.toFixed(6),
            outputPressure.toFixed(6),
            String(Math.round(event.tiltX || 0)),
            String(Math.round(event.tiltY || 0)),
            String(event.pointerId || 0)
        ].join(',');

        socket.send(message);

        if (penLike && (phase === 'u' || phase === 'c')) {
            penActive = false;
        }

        if (event.pointerType === 'touch' && (phase === 'u' || phase === 'c')) {
            activeTouchId = null;
        }
    }

    pad.addEventListener('pointerdown', function (event) {
        if (useNativeIosTouchInk) {
            return;
        }

        if (event.pointerType === 'touch') {
            var mode = requestedFingerMode();
            if (!mode) {
                return;
            }

            event.preventDefault();
            beginTouchGesture(event);

            if (touchGestureMaxCount >= 2) {
                return;
            }

            pendingFingerDown = snapshotPointer(event);
            pendingFingerMode = mode;
            clearPendingFingerTimer();
            pendingFingerTimer = setTimeout(
                commitPendingFingerDown,
                FINGER_MOUSE_DELAY_MS);
            return;
        }

        if (!shouldForward(event)) {
            return;
        }

        event.preventDefault();

        // A fresh physical Pencil DOWN always starts a fresh bridge stroke.
        // If Safari lost the prior UP/CANCEL, explicitly release the host-side
        // synthetic pen before starting this stroke.
        if (penActive) {
            penCancels += 1;
            sendEvent(event, 'c');
        }
        sendEvent(event, 'd');
    }, { passive: false });

    pad.addEventListener('pointermove', function (event) {
        if (useNativeIosTouchInk) {
            return;
        }

        if (event.pointerType === 'touch') {
            event.preventDefault();
            moveTouchGesture(event);

            if (touchGestureMaxCount >= 2) {
                return;
            }

            if (pendingFingerMode || activeFingerMode) {
                var sample = snapshotPointer(event);
                if (pendingFingerDown) {
                    var dx = sample.clientX - pendingFingerDown.clientX;
                    var dy = sample.clientY - pendingFingerDown.clientY;
                    if ((dx * dx + dy * dy) > 16) {
                        commitPendingFingerDown();
                    }
                }

                if (activeTouchId !== null) {
                    activeFingerSample = sample;
                    sendEvent(sample, 'm');
                }
            }
            return;
        }

        if (!shouldForward(event)) {
            return;
        }

        event.preventDefault();
        sendEvent(event, 'm');
    }, { passive: false });

    window.addEventListener('pointerup', function (event) {
        if (useNativeIosTouchInk || markupDrawing) {
            return;
        }

        if (event.pointerType === 'touch') {
            if (!touchGesture.has(event.pointerId) &&
                event.pointerId !== activeTouchId) {
                return;
            }

            event.preventDefault();
            var multiTouch = finishTouchGesture(event, false);

            if (!multiTouch &&
                (pendingFingerDown || activeTouchId !== null)) {
                if (pendingFingerDown) {
                    commitPendingFingerDown();
                }

                if (activeTouchId !== null) {
                    var sample = snapshotPointer(event);
                    sendEvent(sample, 'u');
                    activeFingerSample = null;
                    activeFingerMode = null;
                }
            } else {
                clearPendingFingerTimer();
                pendingFingerDown = null;
                pendingFingerMode = null;
            }
            return;
        }

        if (event.pointerType === 'pen' && penActive) {
            event.preventDefault();
            sendEvent(event, 'u');
        }
    }, { capture: true, passive: false });

    window.addEventListener('pointercancel', function (event) {
        if (useNativeIosTouchInk || markupDrawing) {
            return;
        }

        if (event.pointerType === 'touch') {
            if (!touchGesture.has(event.pointerId) &&
                event.pointerId !== activeTouchId) {
                return;
            }

            event.preventDefault();
            finishTouchGesture(event, true);
            clearPendingFingerTimer();
            pendingFingerDown = null;
            pendingFingerMode = null;

            if (activeTouchId !== null && activeFingerSample) {
                sendEvent(activeFingerSample, 'c');
            }
            activeFingerSample = null;
            activeFingerMode = null;
            return;
        }

        if (event.pointerType === 'pen' && penActive) {
            event.preventDefault();
            penCancels += 1;
            sendEvent(event, 'c');
            penActive = false;
        }
    }, { capture: true, passive: false });

    function beginNativePadStylus(touch) {
        var sample = nativeTouchSample(touch, 'pen');

        // A new physical stylus contact is authoritative. Explicitly close any
        // stale browser/host stroke before beginning this one.
        if (penActive) {
            penCancels += 1;
            sendEvent(sample, 'c');
        }

        nativePadStylusId = touch.identifier;
        sendEvent(sample, 'd');
    }

    function moveNativePadStylus(touch) {
        if (nativePadStylusId !== touch.identifier) {
            return;
        }
        sendEvent(nativeTouchSample(touch, 'pen'), 'm');
    }

    function endNativePadStylus(touch, cancelled) {
        if (nativePadStylusId !== touch.identifier) {
            return;
        }

        sendEvent(
            nativeTouchSample(touch, 'pen'),
            cancelled ? 'c' : 'u');
        nativePadStylusId = null;
    }

    pad.addEventListener('touchstart', function (event) {
        if (!useNativeIosTouchInk ||
            document.body.classList.contains('markup-active')) {
            return;
        }

        var handled = false;

        for (var i = 0; i < event.changedTouches.length; ++i) {
            var touch = event.changedTouches[i];

            if (touchKind(touch) === 'pen') {
                event.preventDefault();
                event.stopPropagation();
                beginNativePadStylus(touch);
                handled = true;
                continue;
            }

            var sample = nativeTouchSample(touch, 'touch');
            beginTouchGesture(sample);
            handled = true;

            var mode = requestedFingerMode();
            if (mode &&
                touchGestureMaxCount < 2 &&
                pendingFingerDown === null &&
                activeTouchId === null) {
                pendingFingerDown = sample;
                pendingFingerMode = mode;
                clearPendingFingerTimer();
                pendingFingerTimer = setTimeout(
                    commitPendingFingerDown,
                    FINGER_MOUSE_DELAY_MS);
            }
        }

        if (handled) {
            event.preventDefault();
            event.stopPropagation();
        }
    }, { passive: false });

    pad.addEventListener('touchmove', function (event) {
        if (!useNativeIosTouchInk ||
            document.body.classList.contains('markup-active')) {
            return;
        }

        var handled = false;

        for (var i = 0; i < event.changedTouches.length; ++i) {
            var touch = event.changedTouches[i];

            if (touchKind(touch) === 'pen') {
                if (nativePadStylusId === touch.identifier) {
                    moveNativePadStylus(touch);
                    handled = true;
                }
                continue;
            }

            var sample = nativeTouchSample(touch, 'touch');
            if (touchGesture.has(sample.pointerId)) {
                moveTouchGesture(sample);
                handled = true;
            }

            if (pendingFingerDown &&
                pendingFingerDown.pointerId === sample.pointerId) {
                var dx = sample.clientX - pendingFingerDown.clientX;
                var dy = sample.clientY - pendingFingerDown.clientY;
                if ((dx * dx + dy * dy) > 16) {
                    commitPendingFingerDown();
                }
            }

            if (activeTouchId === sample.pointerId &&
                activeFingerMode !== null) {
                activeFingerSample = sample;
                sendEvent(sample, 'm');
                handled = true;
            }
        }

        if (handled) {
            event.preventDefault();
            event.stopPropagation();
        }
    }, { passive: false });

    pad.addEventListener('touchend', function (event) {
        if (!useNativeIosTouchInk ||
            document.body.classList.contains('markup-active')) {
            return;
        }

        var handled = false;

        for (var i = 0; i < event.changedTouches.length; ++i) {
            var touch = event.changedTouches[i];

            if (touchKind(touch) === 'pen') {
                if (nativePadStylusId === touch.identifier) {
                    event.preventDefault();
                    event.stopPropagation();
                    endNativePadStylus(touch, false);
                    handled = true;
                }
                continue;
            }

            var sample = nativeTouchSample(touch, 'touch');
            if (!touchGesture.has(sample.pointerId) &&
                activeTouchId !== sample.pointerId) {
                continue;
            }

            var multiTouch = finishTouchGesture(sample, false);

            if (!multiTouch &&
                (pendingFingerDown || activeTouchId === sample.pointerId)) {
                if (pendingFingerDown &&
                    pendingFingerDown.pointerId === sample.pointerId) {
                    commitPendingFingerDown();
                }

                if (activeTouchId === sample.pointerId) {
                    activeFingerSample = sample;
                    sendEvent(sample, 'u');
                    activeFingerSample = null;
                    activeFingerMode = null;
                }
            } else {
                clearPendingFingerTimer();
                pendingFingerDown = null;
                pendingFingerMode = null;
            }

            handled = true;
        }

        if (handled) {
            event.preventDefault();
            event.stopPropagation();
        }
    }, { passive: false });

    pad.addEventListener('touchcancel', function (event) {
        if (!useNativeIosTouchInk ||
            document.body.classList.contains('markup-active')) {
            return;
        }

        var handled = false;

        for (var i = 0; i < event.changedTouches.length; ++i) {
            var touch = event.changedTouches[i];

            if (touchKind(touch) === 'pen') {
                if (nativePadStylusId === touch.identifier) {
                    endNativePadStylus(touch, true);
                    handled = true;
                }
                continue;
            }

            var sample = nativeTouchSample(touch, 'touch');

            if (touchGesture.has(sample.pointerId)) {
                finishTouchGesture(sample, true);
                handled = true;
            }

            if (pendingFingerDown &&
                pendingFingerDown.pointerId === sample.pointerId) {
                clearPendingFingerTimer();
                pendingFingerDown = null;
                pendingFingerMode = null;
            }

            if (activeTouchId === sample.pointerId) {
                activeFingerSample = sample;
                sendEvent(sample, 'c');
                activeFingerSample = null;
                activeFingerMode = null;
                handled = true;
            }
        }

        if (handled) {
            event.preventDefault();
            event.stopPropagation();
        }
    }, { passive: false });

    pad.addEventListener('contextmenu', function (event) {
        event.preventDefault();
    });

    ['click', 'dblclick', 'dragstart', 'selectstart'].forEach(function (type) {
        pad.addEventListener(type, function (event) {
            event.preventDefault();
        }, { passive: false });
    });

    ['gesturestart', 'gesturechange', 'gestureend'].forEach(function (type) {
        pad.addEventListener(type, function (event) {
            event.preventDefault();
        }, { passive: false });
    });

    function telemetryFrame() {
        if (latestTelemetry) {
            updateReadout(latestTelemetry);
            latestTelemetry = null;
        }
        requestAnimationFrame(telemetryFrame);
    }

    window.addEventListener('resize', function () {
        refreshPadRect();
        fitMarkupCanvas();
    });
    window.addEventListener('orientationchange', function () {
        setTimeout(function () {
            refreshPadRect();
            markupRect = null;
            fitMarkupCanvas();
        }, 50);
    });

    refreshPadRect();
    requestAnimationFrame(telemetryFrame);

    window.addEventListener('pagehide', function () {
        resetMarkupStroke();
        cancelActiveFinger();
        nativePadStylusId = null;
        penActive = false;
        if (socket) {
            socket.close();
        }
    });

    connect();
}());
</script>
</body>
</html>
)PBHTML";
