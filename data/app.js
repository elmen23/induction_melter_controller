/* =============================================================================
 *  Induction Melter — Web Console
 *  app.js — WebSocket client, control binding, live chart, OTA, WiFi
 * ===========================================================================*/

(() => {
    'use strict';

    const $  = (id) => document.getElementById(id);
    const $$ = (sel) => document.querySelectorAll(sel);
    const qs = (sel) => document.querySelector(sel);

    /* ------------ state ------------ */
    const state = {
        ws: null,
        url: (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/ws',
        retryMs: 1000,
        lastSnap: null,
        history: { ts: [], current: [], igbt: [], cool: [], vbus: [] },
        historyMax: 600,
        pendingFreq: null,
        pendingDuty: null,
        pendingPower: null,
        pendingTimer: null,
    };

    /* ------------ toast ------------ */
    function toast(msg, kind = '') {
        const t = $('toast');
        t.textContent = msg;
        t.className = 'toast show ' + kind;
        clearTimeout(toast._h);
        toast._h = setTimeout(() => t.classList.remove('show'), 2400);
    }

    /* ------------ tabs ------------ */
    $$('.tabs button').forEach(btn => {
        btn.addEventListener('click', () => {
            $$('.tabs button').forEach(b => b.classList.remove('active'));
            $$('.tab').forEach(t => t.classList.remove('active'));
            btn.classList.add('active');
            qs(`.tab[data-tab="${btn.dataset.tab}"]`).classList.add('active');
        });
    });

    /* ------------ WebSocket ------------ */
    function setConnState(cls, txt) {
        const p = $('connState');
        p.classList.remove('connected', 'connecting');
        if (cls) p.classList.add(cls);
        $('connText').textContent = txt;
    }

    function connect() {
        setConnState('connecting', 'جاري الاتصال…');
        try {
            state.ws = new WebSocket(state.url);
        } catch (e) {
            scheduleReconnect();
            return;
        }
        state.ws.onopen = () => {
            state.retryMs = 1000;
            setConnState('connected', 'متصل');
        };
        state.ws.onmessage = (ev) => {
            try { handle(JSON.parse(ev.data)); }
            catch (e) { console.error('Bad JSON', e); }
        };
        state.ws.onclose = () => { setConnState('', 'انقطع الاتصال'); scheduleReconnect(); };
        state.ws.onerror = () => { try { state.ws.close(); } catch (_) {} };
    }
    function scheduleReconnect() {
        state.retryMs = Math.min(state.retryMs * 2, 8000);
        setTimeout(connect, state.retryMs);
    }

    function send(obj) {
        if (state.ws && state.ws.readyState === WebSocket.OPEN) {
            state.ws.send(JSON.stringify(obj));
        } else {
            /* WS down — fall back to REST */
            const q = new URLSearchParams(obj).toString();
            fetch('/api/' + (obj._path || 'status') + '?' + q, {
                method: 'GET',
                headers: { 'Authorization': 'Basic ' + btoa('admin:melter') }
            }).catch(() => {});
        }
    }

    /* ------------ incoming snapshot ------------ */
    function handle(d) {
        state.lastSnap = d;
        updateDash(d);
        updateControls(d);
        updateLogs(d);
        pushHistory(d);
    }

    function fmt(n, dp = 1) {
        if (n == null || isNaN(n)) return '—';
        return Number(n).toFixed(dp);
    }

    function updateDash(d) {
        $('freqVal').textContent     = (d.freq_actual ?? d.freq_hz ?? 0).toLocaleString('en');
        $('freqTarget').textContent  = (d.freq_hz ?? 0).toLocaleString('en');
        $('dutyVal').textContent     = fmt(d.duty_actual ?? d.duty_pct, 0);
        $('dutyTarget').textContent  = fmt(d.duty_pct, 0);
        $('powerVal').textContent    = fmt(d.power_w, 0);
        $('powerTarget').textContent = fmt(d.power_w, 0);

        // big cards warnings
        const cFreq  = $('cardFreq');
        const cDuty  = $('cardDuty');
        const cPower = $('cardPower');
        cFreq.classList.toggle('warn', d.state === 'SoftStart');
        cDuty.classList.toggle('warn', d.state === 'SoftStart');
        cPower.classList.toggle('warn', d.state === 'SoftStart');
        cFreq.classList.toggle('fault', d.fault);
        cDuty.classList.toggle('fault', d.fault);
        cPower.classList.toggle('fault', d.fault);

        // state bar
        const bar  = $('stateBar');
        const txt  = $('stateText');
        const meta = $('stateMeta');
        bar.classList.remove('idle', 'running', 'soft', 'fault', 'estop');
        let label = '—';
        switch (d.state) {
            case 'Boot':     label = 'بدء التشغيل'; bar.classList.add('idle');   break;
            case 'Idle':     label = 'متوقف';      bar.classList.add('idle');   break;
            case 'Run':      label = 'يعمل';       bar.classList.add('running');break;
            case 'SoftStart':label = 'بداية ناعمة';bar.classList.add('soft');   break;
            case 'FAULT':    label = '⚠ خطأ';      bar.classList.add('fault');  break;
            case 'E-STOP':   label = '⛔ إيقاف طارئ'; bar.classList.add('estop');break;
            case 'Tuning':   label = 'معايرة';     bar.classList.add('soft');   break;
            default:         label = d.state;
        }
        txt.textContent = label;
        meta.textContent = `Faults: ${d.fault_count ?? 0} · RSSI: ${d.rssi ?? '—'} dBm`;

        $('currVal').textContent   = fmt(d.current_a, 1);
        $('currLimit').textContent = '30';
        $('currBar').style.width   = Math.min(100, (d.current_a / 30) * 100) + '%';
        $('vbusVal').textContent   = fmt(d.vbus_v, 1);
        $('vbusBar').style.width   = Math.min(100, (d.vbus_v / 120) * 100) + '%';
        $('flowVal').textContent   = fmt(d.flow_lpm, 1);
        $('flowBar').style.width   = Math.min(100, (d.flow_lpm / 5) * 100) + '%';
        $('igbtVal').textContent   = fmt(d.igbt_c, 0);
        $('igbtBar').style.width   = Math.min(100, Math.max(0, (d.igbt_c / 90) * 100)) + '%';
        $('coolVal').textContent   = fmt(d.coolant_c, 0);
        $('coolBar').style.width   = Math.min(100, Math.max(0, (d.coolant_c / 70) * 100)) + '%';

        $('pidOut').textContent = fmt(d.pid_out, 1);
        $('pidErr').textContent = fmt(d.pid_err, 0);

        $('btnClear').hidden = !d.fault && !d.estop;
    }

    function updateControls(d) {
        // Only push server values into the inputs when the user is NOT actively
        // editing them. We track focus state for that.
        if (document.activeElement !== $('slFreq') && document.activeElement !== $('nbFreq')) {
            $('slFreq').value = d.freq_hz;
            $('nbFreq').value = d.freq_hz;
        }
        if (document.activeElement !== $('slDuty') && document.activeElement !== $('nbDuty')) {
            $('slDuty').value = d.duty_pct;
            $('nbDuty').value = d.duty_pct;
        }
        if (document.activeElement !== $('slPower') && document.activeElement !== $('nbPower')) {
            $('slPower').value = d.power_w;
            $('nbPower').value = d.power_w;
        }
        $('pidEnable').checked = !!d.pid_on;
    }

    function updateLogs(d) {
        $('infoIp').textContent      = location.host;
        $('infoRssi').textContent    = d.rssi + ' dBm';
        const up = d.uptime ?? 0;
        const h = Math.floor(up / 3600000);
        const m = Math.floor((up % 3600000) / 60000);
        const s = Math.floor((up % 60000) / 1000);
        $('infoUptime').textContent  = `${h}س ${m}د ${s}ث`;
        $('infoHeap').textContent    = d.free_heap + ' / min ' + d.heap_min + ' bytes';
        $('infoClients').textContent = d.clients;
        $('infoFw').textContent      = d.version;
        $('infoFaults').textContent  = d.fault_count;
        $('verSpan').textContent     = 'v' + d.version;
        $('tsSpan').textContent      = new Date().toLocaleTimeString();
        $('faultNote').textContent   = d.fault
            ? `(${d.fault_code}) ${d.fault_note || '—'}`
            : 'لا يوجد عطل';
    }

    /* ------------ history + chart ------------ */
    function pushHistory(d) {
        const h = state.history;
        h.ts.push(d.ts);
        h.current.push(d.current_a);
        h.igbt.push(d.igbt_c);
        h.cool.push(d.coolant_c);
        h.vbus.push(d.vbus_v);
        for (const k of Object.keys(h)) {
            if (h[k].length > state.historyMax) h[k].shift();
        }
        drawChart();
    }

    function drawChart() {
        const cv = $('liveChart');
        if (!cv) return;
        const ctx = cv.getContext('2d');
        const W = cv.clientWidth, H = cv.clientHeight;
        cv.width  = W * (window.devicePixelRatio || 1);
        cv.height = H * (window.devicePixelRatio || 1);
        ctx.scale(window.devicePixelRatio || 1, window.devicePixelRatio || 1);
        ctx.clearRect(0, 0, W, H);

        // grid
        ctx.strokeStyle = '#1a2034';
        ctx.lineWidth = 1;
        for (let i = 0; i < 5; ++i) {
            const y = (H / 4) * i;
            ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke();
        }

        const series = [
            { data: state.history.current, color: '#ffb84d', scale: 35 },
            { data: state.history.igbt,    color: '#ff6b6b', scale: 90 },
            { data: state.history.cool,    color: '#4dc3ff', scale: 70 },
            { data: state.history.vbus,    color: '#9eff7e', scale: 130 },
        ];
        const N = state.history.ts.length;
        if (N < 2) return;
        series.forEach(s => {
            ctx.strokeStyle = s.color;
            ctx.lineWidth = 2;
            ctx.beginPath();
            for (let i = 0; i < N; ++i) {
                const x = (i / (N - 1)) * W;
                const y = H - Math.min(H, (s.data[i] / s.scale) * H);
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();
        });
    }

    window.addEventListener('resize', drawChart);

    /* ------------ sliders & buttons ------------ */
    function bindSlider(slider, number, key, validator) {
        const onChange = () => {
            let v = parseFloat(slider.value);
            if (validator) v = validator(v);
            number.value = v;
            queueSet(key, v);
        };
        slider.addEventListener('input', onChange);
        number.addEventListener('change', () => {
            let v = parseFloat(number.value);
            if (validator) v = validator(v);
            slider.value = v;
            queueSet(key, v);
        });
    }
    function queueSet(key, v) {
        if (key === 'freq')  state.pendingFreq  = v;
        if (key === 'duty')  state.pendingDuty  = v;
        if (key === 'power') state.pendingPower = v;
        clearTimeout(state.pendingTimer);
        state.pendingTimer = setTimeout(flushPending, 80);
    }
    function flushPending() {
        if (state.pendingFreq  != null) { send({ _path: 'set/freq', v: Math.round(state.pendingFreq) });  state.pendingFreq  = null; }
        if (state.pendingDuty  != null) { send({ _path: 'set/duty', v: Math.round(state.pendingDuty) });  state.pendingDuty  = null; }
        if (state.pendingPower != null) { send({ _path: 'set/power',v: Math.round(state.pendingPower) }); state.pendingPower = null; }
    }

    bindSlider($('slFreq'),  $('nbFreq'),  'freq',  v => Math.max(20000, Math.min(200000, v)));
    bindSlider($('slDuty'),  $('nbDuty'),  'duty',  v => Math.max(0, Math.min(95, v)));
    bindSlider($('slPower'), $('nbPower'), 'power', v => Math.max(0, Math.min(3000, v)));

    /* quick step buttons in control tab */
    $$('.card .btn-row').forEach(row => {
        row.querySelectorAll('button[data-step]').forEach(btn => {
            btn.addEventListener('click', () => {
                const step = parseFloat(btn.dataset.step);
                const target = row.parentElement.querySelector('input[type="range"]');
                if (!target) return;
                let v = parseFloat(target.value) + step;
                v = Math.max(parseFloat(target.min), Math.min(parseFloat(target.max), v));
                target.value = v;
                target.dispatchEvent(new Event('input'));
            });
        });
    });

    $('pidEnable').addEventListener('change', () => {
        send({ _path: 'pid/on', v: $('pidEnable').checked ? '1' : '0' });
        toast($('pidEnable').checked ? 'PID مفعّل' : 'PID معطّل');
    });

    $('btnArm').addEventListener('click',    () => { send({ _path: 'arm' });   toast('جاري التشغيل…'); });
    $('btnDisarm').addEventListener('click', () => { send({ _path: 'disarm'}); toast('تم الإيقاف', 'ok'); });
    $('btnEstop').addEventListener('click',  () => { send({ _path: 'estop' }); toast('إيقاف طارئ!', 'error'); });
    $('btnClear').addEventListener('click',  () => { send({ _path: 'clear' }); toast('تم مسح الفاولت', 'ok'); });
    $('btnApply').addEventListener('click',  () => { flushPending(); toast('تم التطبيق', 'ok'); });
    $('btnReset').addEventListener('click',  () => {
        $('slFreq').value  = 50000; $('nbFreq').value  = 50000;
        $('slDuty').value  = 40;    $('nbDuty').value  = 40;
        $('slPower').value = 1500;  $('nbPower').value = 1500;
        flushPending();
        toast('تمت استعادة الافتراضي', 'ok');
    });
    $('btnRamp').addEventListener('click',   () => { send({ _path: 'arm' }); toast('بداية ناعمة…', 'ok'); });

    $('btnPidTune').addEventListener('click', () => {
        send({ _path: 'pid/tune', kp: $('kp').value, ki: $('ki').value, kd: $('kd').value });
        toast('تم حفظ معاملات PID', 'ok');
    });

    /* ------------ presets ------------ */
    function loadPresets() {
        fetch('/api/presets', { headers: { 'Authorization': 'Basic ' + btoa('admin:melter') } })
            .then(r => r.json()).then(j => {
                const g = $('presetsGrid'); g.innerHTML = '';
                (j.presets || []).forEach(p => {
                    const el = document.createElement('div');
                    el.className = 'preset';
                    el.innerHTML = `
                        <div class="name">${p.name}</div>
                        <div class="meta">${p.freq.toLocaleString('en')} Hz<br>${p.duty}% · ${p.power} W</div>`;
                    el.addEventListener('click', () => {
                        send({ _path: 'preset', i: p.i });
                        $('slFreq').value  = p.freq;  $('nbFreq').value  = p.freq;
                        $('slDuty').value  = p.duty;  $('nbDuty').value  = p.duty;
                        $('slPower').value = p.power; $('nbPower').value = p.power;
                        toast(`تم تحميل ${p.name}`, 'ok');
                    });
                    g.appendChild(el);
                });
            });
    }
    loadPresets();

    /* ------------ WiFi ------------ */
    function scanWifi() {
        fetch('/api/wifi/scan').then(r => r.json()).then(j => {
            const s = $('ssidList'); s.innerHTML = '';
            (j.aps || []).forEach(ap => {
                const o = document.createElement('option');
                o.value = ap.ssid; o.textContent = `${ap.ssid} (${ap.rssi} dBm${ap.enc ? ' · 🔒' : ''})`;
                s.appendChild(o);
            });
            toast(`تم العثور على ${(j.aps || []).length} شبكة`);
        }).catch(() => toast('فشل الفحص', 'error'));
    }
    $('btnWifiScan').addEventListener('click', scanWifi);
    $('btnWifiSave').addEventListener('click', () => {
        const ssid = $('ssidList').value;
        const pass = $('wifiPass').value;
        send({ _path: 'wifi/save', ssid, pass });
        toast('تم الحفظ، جاري إعادة الاتصال…');
    });

    /* ------------ OTA ------------ */
    $('btnOta').addEventListener('click', () => {
        const f = $('otaFile').files[0];
        if (!f) { toast('اختر ملف أولاً', 'error'); return; }
        const fd = new FormData();
        fd.append('firmware', f);
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/api/ota');
        xhr.setRequestHeader('Authorization', 'Basic ' + btoa('admin:melter'));
        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) {
                $('otaBar').style.width = (e.loaded / e.total * 100) + '%';
            }
        };
        xhr.onload = () => {
            if (xhr.status === 200) toast('تم التحديث!', 'ok');
            else toast('فشل التحديث', 'error');
        };
        xhr.send(fd);
    });

    /* ------------ danger zone ------------ */
    $('btnReboot').addEventListener('click', () => {
        if (!confirm('تأكيد إعادة التشغيل؟')) return;
        send({ _path: 'reboot' });
    });
    $('btnFactory').addEventListener('click', () => {
        if (!confirm('سيتم مسح كل الإعدادات! متأكد؟')) return;
        send({ _path: 'factory' });
    });

    /* ------------ boot ------------ */
    connect();
    drawChart();
})();
