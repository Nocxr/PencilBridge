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
    padding: 10px;
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
    .status { order: 3; flex-basis: 100%; }
    .pressure-controls { grid-template-columns: 1fr; }
    .metrics { grid-template-columns: repeat(2, 1fr); }
}
</style>
</head>
<body>
<header>
    <div class="brand">PencilBridge</div>
    <div class="status"><span id="dot" class="dot"></span><span id="statusText">Connecting…</span></div>
    <div class="spacer"></div>
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
        <strong>Markup</strong>
        <input id="markupColor" type="color" value="#ff3b30" aria-label="Markup color">
        <label>Size <input id="markupSize" type="range" min="2" max="28" step="1" value="7"></label>
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
    var markupStage = document.getElementById('markupStage');
    var markupCanvas = document.getElementById('markupCanvas');
    var markupColor = document.getElementById('markupColor');
    var markupSize = document.getElementById('markupSize');
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
    var pendingFingerTimer = null;
    var activeFingerSample = null;
    var toastTimer = null;

    var GESTURE_MAX_MS = 420;
    var GESTURE_MOVE_PX = 28;
    var FINGER_MOUSE_DELAY_MS = 120;

    var markupImage = null;
    var markupDrawing = false;
    var markupLastX = 0;
    var markupLastY = 0;

    function loadNumber(key, fallback) {
        var value = Number(localStorage.getItem(key));
        return Number.isFinite(value) ? value : fallback;
    }

    pressureMax.value = String(Math.max(0.40, Math.min(1.00, loadNumber('pencilbridge.pressureMax', 0.70))));
    pressureCurve.value = String(Math.max(0.35, Math.min(1.50, loadNumber('pencilbridge.pressureCurve', 0.80))));

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
        if (!fingerMouse.checked) {
            clearPendingFingerTimer();
            pendingFingerDown = null;
            if (activeTouchId !== null && activeFingerSample) {
                sendEvent(activeFingerSample, 'c');
                activeFingerSample = null;
            }
        }
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
        if (!markupCanvas.width || !markupCanvas.height || markupView.style.display === 'none') {
            return;
        }
        var rect = markupStage.getBoundingClientRect();
        var scale = Math.min(
            Math.max(1, rect.width - 20) / markupCanvas.width,
            Math.max(1, rect.height - 20) / markupCanvas.height,
            1);
        markupCanvas.style.width = Math.max(1, markupCanvas.width * scale) + 'px';
        markupCanvas.style.height = Math.max(1, markupCanvas.height * scale) + 'px';
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
            pad.style.display = 'none';
            markupView.style.display = 'flex';
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
        markupView.style.display = 'none';
        pad.style.display = '';
        refreshPadRect();
    }

    function markupPoint(event) {
        var rect = markupCanvas.getBoundingClientRect();
        return {
            x: (event.clientX - rect.left) * markupCanvas.width / Math.max(1, rect.width),
            y: (event.clientY - rect.top) * markupCanvas.height / Math.max(1, rect.height)
        };
    }

    function drawMarkupSegment(event, fromX, fromY, toX, toY) {
        var ctx = markupCanvas.getContext('2d');
        var pressure = Math.max(0.15, Number(event.pressure || 0.5));
        ctx.strokeStyle = markupColor.value;
        ctx.lineWidth = Number(markupSize.value) * (0.55 + pressure * 0.9);
        ctx.lineCap = 'round';
        ctx.lineJoin = 'round';
        ctx.beginPath();
        ctx.moveTo(fromX, fromY);
        ctx.lineTo(toX, toY);
        ctx.stroke();
    }

    markupCanvas.addEventListener('pointerdown', function (event) {
        if (event.pointerType !== 'pen') {
            return;
        }
        event.preventDefault();
        var point = markupPoint(event);
        markupDrawing = true;
        markupLastX = point.x;
        markupLastY = point.y;
        try { markupCanvas.setPointerCapture(event.pointerId); } catch (_) {}
    }, { passive: false });

    markupCanvas.addEventListener('pointermove', function (event) {
        if (!markupDrawing || event.pointerType !== 'pen') {
            return;
        }
        event.preventDefault();

        var samples = (typeof event.getCoalescedEvents === 'function')
            ? event.getCoalescedEvents()
            : [];
        if (!samples || samples.length === 0) {
            samples = [event];
        }

        for (var i = 0; i < samples.length; ++i) {
            var point = markupPoint(samples[i]);
            drawMarkupSegment(
                samples[i],
                markupLastX,
                markupLastY,
                point.x,
                point.y);
            markupLastX = point.x;
            markupLastY = point.y;
        }
    }, { passive: false });

    function endMarkupStroke(event) {
        if (event.pointerType !== 'pen') {
            return;
        }
        event.preventDefault();
        if (markupDrawing) {
            markupDrawing = false;
        }
        try { markupCanvas.releasePointerCapture(event.pointerId); } catch (_) {}
    }

    markupCanvas.addEventListener('pointerup', endMarkupStroke, { passive: false });
    markupCanvas.addEventListener('pointercancel', endMarkupStroke, { passive: false });

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
        if (markupView.style.display !== 'none' && window.getSelection) {
            var selection = window.getSelection();
            if (selection && selection.rangeCount > 0) {
                selection.removeAllRanges();
            }
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

    function commitPendingFingerDown() {
        clearPendingFingerTimer();
        if (!pendingFingerDown || !fingerMouse.checked || touchGestureMaxCount >= 2) {
            pendingFingerDown = null;
            return;
        }

        activeFingerSample = pendingFingerDown;
        sendEvent(pendingFingerDown, 'd');
        pendingFingerDown = null;
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

            if (activeTouchId !== null && activeFingerSample) {
                sendEvent(activeFingerSample, 'c');
                activeFingerSample = null;
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
            return fingerMouse.checked;
        }
        return false;
    }

    function sendEvent(event, phase) {
        if (!socket || socket.readyState !== WebSocket.OPEN || !shouldForward(event)) {
            return;
        }

        if (event.pointerType === 'pen') {
            if (phase === 'd') {
                penActive = true;
            } else if (phase === 'm' && !penActive) {
                // Safari can occasionally cancel/recreate a Pencil pointer during a
                // continuous physical contact. If pressure says the tip is still down,
                // transparently begin a fresh synthetic stroke instead of going silent.
                if (Number(event.pressure || 0) > 0.001) {
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
        if (event.pointerType === 'pen') {
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

        var device = event.pointerType === 'pen' ? 'p' : 't';
        var message = [
            device,
            phase,
            point.x.toFixed(6),
            point.y.toFixed(6),
            (event.pointerType === 'pen'
                ? mappedPressure(event.pressure)
                : Number(event.pressure || 0)).toFixed(6),
            String(Math.round(event.tiltX || 0)),
            String(Math.round(event.tiltY || 0)),
            String(event.pointerId || 0)
        ].join(',');

        socket.send(message);

        if (event.pointerType === 'pen' && (phase === 'u' || phase === 'c')) {
            penActive = false;
        }

        if (event.pointerType === 'touch' && (phase === 'u' || phase === 'c')) {
            activeTouchId = null;
        }
    }

    pad.addEventListener('pointerdown', function (event) {
        if (event.pointerType === 'touch') {
            event.preventDefault();
            beginTouchGesture(event);
            try { pad.setPointerCapture(event.pointerId); } catch (_) {}

            if (touchGestureMaxCount >= 2) {
                return;
            }

            if (fingerMouse.checked) {
                pendingFingerDown = snapshotPointer(event);
                clearPendingFingerTimer();
                pendingFingerTimer = setTimeout(
                    commitPendingFingerDown,
                    FINGER_MOUSE_DELAY_MS);
            }
            return;
        }

        if (!shouldForward(event)) {
            return;
        }

        event.preventDefault();
        try { pad.setPointerCapture(event.pointerId); } catch (_) {}
        sendEvent(event, 'd');
    }, { passive: false });

    pad.addEventListener('pointermove', function (event) {
        if (event.pointerType === 'touch') {
            event.preventDefault();
            moveTouchGesture(event);

            if (touchGestureMaxCount >= 2) {
                return;
            }

            if (fingerMouse.checked) {
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

    pad.addEventListener('pointerup', function (event) {
        if (event.pointerType === 'touch') {
            event.preventDefault();
            var multiTouch = finishTouchGesture(event, false);

            if (!multiTouch && fingerMouse.checked) {
                if (pendingFingerDown) {
                    commitPendingFingerDown();
                }

                if (activeTouchId !== null) {
                    var sample = snapshotPointer(event);
                    sendEvent(sample, 'u');
                    activeFingerSample = null;
                }
            } else {
                clearPendingFingerTimer();
                pendingFingerDown = null;
            }

            try { pad.releasePointerCapture(event.pointerId); } catch (_) {}
            return;
        }

        if (!shouldForward(event)) {
            return;
        }

        event.preventDefault();
        sendEvent(event, 'u');
        try { pad.releasePointerCapture(event.pointerId); } catch (_) {}
    }, { passive: false });

    pad.addEventListener('pointercancel', function (event) {
        if (event.pointerType === 'touch') {
            event.preventDefault();
            finishTouchGesture(event, true);
            clearPendingFingerTimer();
            pendingFingerDown = null;

            if (activeTouchId !== null && activeFingerSample) {
                sendEvent(activeFingerSample, 'c');
                activeFingerSample = null;
            }
            return;
        }

        if (!shouldForward(event)) {
            return;
        }
        if (event.pointerType === 'pen') {
            penCancels += 1;
        }
        sendEvent(event, 'c');
        penActive = false;
        activeTouchId = null;
    }, { passive: false });

    pad.addEventListener('lostpointercapture', function (event) {
        if (event.pointerType === 'pen' && penActive && Number(event.pressure || 0) > 0.001) {
            penCancels += 1;
        }
    });

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
        setTimeout(refreshPadRect, 50);
    });

    refreshPadRect();
    requestAnimationFrame(telemetryFrame);

    window.addEventListener('pagehide', function () {
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
