import React, { createContext, useContext, useState, useRef, useCallback } from 'react';

const Ctx = createContext(null);

const FETCH_INTERVAL_MS = 1000;

const THRESHOLDS = {
  spo2_danger:  94,
  spo2_warning: 96,
  temp_fever:   38.5,
  temp_warn:    37.5,
  hr_high:      120,
  hr_low:       50,
};

// الحساسات المتاحة
export const SENSORS = {
  all:  { key: 'all',  label: 'الكل',       icon: '📊' },
  hr:   { key: 'hr',   label: 'القلب',       icon: '❤️' },
  spo2: { key: 'spo2', label: 'الأكسجين',    icon: '🫁' },
  temp: { key: 'temp', label: 'الحرارة',     icon: '🌡️' },
  ecg:  { key: 'ecg',  label: 'ECG',         icon: '📈' },
};

export function HealthProvider({ children }) {
  const [ip,   setIp]   = useState('');
  const [port, setPort] = useState('80');

  const [status,        setStatus]        = useState('idle');
  const [errorMsg,      setErrorMsg]      = useState('');
  const [activeSensor,  setActiveSensor]  = useState('all'); // الحساس النشط حالياً

  const [vitals, setVitals] = useState({
    heartRate: 0, spo2: 0, temperature: 0,
    ecgVoltage: 0, ecgRaw: 0, leadsOff: true,
    validHR: false, validSPO2: false,
  });

  const [ecgBuffer, setEcgBuffer] = useState([]);
  const [history,   setHistory]   = useState([]);
  const [alerts,    setAlerts]    = useState([]);

  const timerRef   = useRef(null);
  const cooldowns  = useRef({});
  const lastUpdate = useRef(null);

  // ── تنبيهات ──────────────────────────────────────────────────
  const pushAlert = useCallback((type, title, body) => {
    const now = Date.now();
    if (cooldowns.current[title] && now - cooldowns.current[title] < 30000) return;
    cooldowns.current[title] = now;
    setAlerts(prev => [{ id: now, type, title, body, time: new Date() }, ...prev].slice(0, 30));
  }, []);

  // ── معالجة البيانات ───────────────────────────────────────────
  const processData = useCallback((data) => {
    lastUpdate.current = new Date();

    const cleaned = {
      ...data,
      heartRate:   data.validHR   ? data.heartRate : 0,
      spo2:        data.validSPO2 ? data.spo2      : 0,
      temperature: (data.temperature > 10 && data.temperature < 50) ? data.temperature : 0,
    };

    setVitals(cleaned);

    if (!cleaned.leadsOff && cleaned.ecgVoltage > 0) {
      setEcgBuffer(prev => {
        const next = [...prev, cleaned.ecgVoltage];
        return next.length > 300 ? next.slice(-300) : next;
      });
    }

    setHistory(prev => [{ ...cleaned, ts: new Date() }, ...prev].slice(0, 100));

    if (cleaned.validSPO2 && cleaned.spo2 > 0) {
      if (cleaned.spo2 < THRESHOLDS.spo2_danger)
        pushAlert('danger', '🚨 أكسجين منخفض جداً', `SpO₂ = ${cleaned.spo2}%`);
      else if (cleaned.spo2 < THRESHOLDS.spo2_warning)
        pushAlert('warning', '⚠️ SpO₂ منخفض', `${cleaned.spo2}%`);
    }
    if (cleaned.temperature > THRESHOLDS.temp_fever)
      pushAlert('danger', '🌡️ حمى', `${cleaned.temperature.toFixed(1)} °C`);
    else if (cleaned.temperature > THRESHOLDS.temp_warn)
      pushAlert('warning', '🌡️ ارتفاع بسيط', `${cleaned.temperature.toFixed(1)} °C`);
    if (cleaned.validHR && cleaned.heartRate > 0) {
      if (cleaned.heartRate > THRESHOLDS.hr_high)
        pushAlert('warning', '💓 تسارع القلب', `${cleaned.heartRate} bpm`);
      else if (cleaned.heartRate < THRESHOLDS.hr_low)
        pushAlert('warning', '💓 بطء القلب', `${cleaned.heartRate} bpm`);
    }
    if (cleaned.leadsOff)
      pushAlert('info', '📡 أقطاب ECG منفصلة', 'تحقق من توصيل RA / LA / RL');
  }, [pushAlert]);

  // ── جلب بيانات ───────────────────────────────────────────────
  const fetchOnce = useCallback(async (targetIp, targetPort) => {
    const url = `http://${targetIp}:${targetPort}/data`;
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 4000);
    try {
      const res = await fetch(url, { signal: ctrl.signal });
      clearTimeout(timeout);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = await res.json();
      processData(data);
    } catch (e) {
      clearTimeout(timeout);
      throw e;
    }
  }, [processData]);

  // ── إرسال أمر تشغيل حساس للأردوينو ──────────────────────────
  const selectSensor = useCallback(async (sensorKey) => {
    const url = `http://${ip}:${port}/cmd?sensor=${sensorKey}`;
    try {
      const ctrl = new AbortController();
      const timeout = setTimeout(() => ctrl.abort(), 3000);
      await fetch(url, { signal: ctrl.signal });
      clearTimeout(timeout);
      setActiveSensor(sensorKey);

      // مسح بفر ECG عند تغيير الحساس
      if (sensorKey !== 'ecg' && sensorKey !== 'all') {
        setEcgBuffer([]);
      }
    } catch (e) {
      // إذا فشل الأمر نغيّر الحساس في التطبيق فقط
      setActiveSensor(sensorKey);
    }
  }, [ip, port]);

  // ── بدء الاتصال ──────────────────────────────────────────────
  const connect = useCallback(async () => {
    if (timerRef.current) clearInterval(timerRef.current);
    if (!ip) { setStatus('error'); setErrorMsg('أدخل IP الجهاز من الإعدادات أولاً'); return; }
    setStatus('connecting');
    setErrorMsg('');

    try {
      await fetchOnce(ip, port);
      setStatus('connected');

      timerRef.current = setInterval(async () => {
        try {
          await fetchOnce(ip, port);
          setStatus('connected');
          setErrorMsg('');
        } catch (e) {
          setStatus('error');
          setErrorMsg('انقطع الاتصال');
        }
      }, FETCH_INTERVAL_MS);

    } catch (e) {
      setStatus('error');
      setErrorMsg(!ip ? 'أدخل IP الجهاز من الإعدادات أولاً' : `تعذر الاتصال بـ ${ip}:${port}`);
    }
  }, [ip, port, fetchOnce]);

  // ── قطع الاتصال ──────────────────────────────────────────────
  const disconnect = useCallback(() => {
    if (timerRef.current) { clearInterval(timerRef.current); timerRef.current = null; }
    setStatus('idle');
    setErrorMsg('');
    setActiveSensor('all');
  }, []);

  const dismissAlert = useCallback((id) => setAlerts(prev => prev.filter(a => a.id !== id)), []);
  const clearHistory = useCallback(() => setHistory([]), []);

  return (
    <Ctx.Provider value={{
      ip, setIp, port, setPort,
      status, errorMsg, lastUpdate,
      activeSensor, selectSensor,
      vitals, ecgBuffer, history, alerts,
      connect, disconnect, dismissAlert, clearHistory,
    }}>
      {children}
    </Ctx.Provider>
  );
}

export const useHealth = () => useContext(Ctx);
