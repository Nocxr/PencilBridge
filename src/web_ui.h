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
html, body { margin: 0; width: 100%; height: 100%; overflow: hidden; background: #111318; }
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
    var recentMaxGapMs = 0;

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

    function setConnection(isLive, text) {
        dot.classList.toggle('live', isLive);
        statusText.textContent = text;
    }

    function connect() {
        clearTimeout(reconnectTimer);
        setConnection(false, 'Connecting…');

        try {
            socket = new WebSocket('ws://' + location.host + '/ws');
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
    }

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
            '  gap≤' + recentMaxGapMs.toFixed(0) + 'ms';

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
        if (lastInputTime > 0) {
            var gap = now - lastInputTime;
            if (gap > recentMaxGapMs) {
                recentMaxGapMs = gap;
            }
        }
        lastInputTime = now;

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
        if (!shouldForward(event)) {
            return;
        }
        event.preventDefault();
        try { pad.setPointerCapture(event.pointerId); } catch (_) {}
        sendEvent(event, 'd');
    }, { passive: false });

    pad.addEventListener('pointermove', function (event) {
        if (!shouldForward(event)) {
            return;
        }
        event.preventDefault();

        sendEvent(event, 'm');
    }, { passive: false });

    pad.addEventListener('pointerup', function (event) {
        if (!shouldForward(event)) {
            return;
        }
        event.preventDefault();
        sendEvent(event, 'u');
        try { pad.releasePointerCapture(event.pointerId); } catch (_) {}
    }, { passive: false });

    pad.addEventListener('pointercancel', function (event) {
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

    function telemetryFrame() {
        if (latestTelemetry) {
            updateReadout(latestTelemetry);
            latestTelemetry = null;
        }
        // Decay the displayed worst gap so old stalls don't stick forever.
        recentMaxGapMs *= 0.985;
        requestAnimationFrame(telemetryFrame);
    }

    window.addEventListener('resize', refreshPadRect);
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
