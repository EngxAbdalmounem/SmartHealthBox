import React from 'react';
import {
  View, Text, ScrollView, StyleSheet,
  TouchableOpacity, SafeAreaView, StatusBar, Image,
} from 'react-native';
import { LinearGradient } from 'expo-linear-gradient';
import { useHealth } from '../context/HealthContext';
import VitalCard      from '../components/VitalCard';
import ECGChart       from '../components/ECGChart';
import AlertBanner    from '../components/AlertBanner';
import SensorSelector from '../components/SensorSelector';
import { C, R, S }   from '../utils/theme';

const LOGO = require('../../assets/hlogo.png');

function statusInfo(s) {
  switch (s) {
    case 'connected':  return { label: 'متصل ✓',         color: C.green  };
    case 'connecting': return { label: 'جارٍ الاتصال...', color: C.cyan   };
    case 'error':      return { label: 'خطأ في الاتصال',  color: C.red    };
    default:           return { label: 'غير متصل',        color: C.textDim };
  }
}

export default function HomeScreen({ navigation }) {
  const {
    status, errorMsg,
    vitals, ecgBuffer, alerts, activeSensor,
    connect, disconnect, dismissAlert,
  } = useHealth();

  const st        = statusInfo(status);
  const connected = status === 'connected';

  const showHR   = activeSensor === 'all' || activeSensor === 'hr'   || activeSensor === 'spo2';
  const showSPO2 = activeSensor === 'all' || activeSensor === 'spo2' || activeSensor === 'hr';
  const showTemp = activeSensor === 'all' || activeSensor === 'temp';
  const showECG  = activeSensor === 'all' || activeSensor === 'ecg';

  return (
    <SafeAreaView style={styles.safe}>
      <StatusBar barStyle="light-content" backgroundColor={C.bg} />

      {alerts.length > 0 && (
        <View style={styles.alertsBox}>
          {alerts.slice(0, 3).map(a =>
            <AlertBanner key={a.id} alert={a} onDismiss={dismissAlert} />
          )}
        </View>
      )}

      <ScrollView style={styles.scroll} contentContainerStyle={styles.content}
        showsVerticalScrollIndicator={false}>

        {/* Header */}
        <View style={styles.header}>
          <View style={styles.headerLeft}>
            <Image source={LOGO} style={styles.logo} resizeMode="contain" />
            <View>
              <Text style={styles.appTitle}>Smart Health Box</Text>
              <Text style={styles.appSub}>Arduino Nano 33 IoT</Text>
            </View>
          </View>
          <TouchableOpacity
            style={[styles.statusPill, { borderColor: st.color + '55' }]}
            onPress={() => navigation.navigate('Settings')}
            activeOpacity={0.7}
          >
            <View style={[styles.dot, { backgroundColor: st.color }]} />
            <Text style={[styles.statusLabel, { color: st.color }]}>{st.label}</Text>
          </TouchableOpacity>
        </View>

        {status === 'error' && (
          <View style={styles.errorBar}>
            <Text style={styles.errorText}>{"⚠️  "}{errorMsg}</Text>
            <TouchableOpacity onPress={connect}>
              <Text style={styles.retryText}>إعادة المحاولة</Text>
            </TouchableOpacity>
          </View>
        )}

        {(status === 'idle' || status === 'error') && (
          <View style={styles.connectBox}>
            <Image source={LOGO} style={styles.connectLogo} resizeMode="contain" />
            <Text style={styles.connectTitle}>Smart Health Box</Text>
            <Text style={styles.connectDesc}>
              {"تأكد أن الهاتف والجهاز على نفس شبكة WiFi،\nثم اضغط اتصال أو عدّل الإعدادات."}
            </Text>
            <TouchableOpacity onPress={connect} activeOpacity={0.85}>
              <LinearGradient colors={['#00b4d8', '#007ea8']} style={styles.connectBtn}
                start={{ x: 0, y: 0 }} end={{ x: 1, y: 0 }}>
                <Text style={styles.connectBtnText}>{"🔌  اتصال بـ Arduino"}</Text>
              </LinearGradient>
            </TouchableOpacity>
            <TouchableOpacity style={styles.settingsLink} onPress={() => navigation.navigate('Settings')}>
              <Text style={styles.settingsLinkText}>{"⚙️  إعداد IP / المنفذ"}</Text>
            </TouchableOpacity>
          </View>
        )}

        {status === 'connecting' && (
          <View style={styles.connectingBox}>
            <Image source={LOGO} style={styles.connectingLogo} resizeMode="contain" />
            <Text style={styles.connectingText}>{"⏳  جارٍ الاتصال بالجهاز..."}</Text>
          </View>
        )}

        {connected && (
          <React.Fragment>
            <SensorSelector />

            {vitals.leadsOff && showECG && (
              <View style={styles.leadsOffBar}>
                <Text style={styles.leadsOffText}>
                  {"⚡ أقطاب ECG منفصلة — تحقق من توصيل RA / LA / RL"}
                </Text>
              </View>
            )}

            <Text style={styles.sectionTitle}>العلامات الحيوية</Text>

            {(showHR || showSPO2) && (
              <View style={styles.row}>
                {showHR && (
                  <VitalCard type="hr" icon="❤️" label="معدل القلب"
                    value={vitals.validHR ? vitals.heartRate : 0}
                    unit="ضربة / دقيقة" accent={C.red} />
                )}
                {showSPO2 && (
                  <VitalCard type="spo2" icon="🫁" label="الأكسجين SpO₂"
                    value={vitals.validSPO2 ? vitals.spo2 : 0}
                    unit="بالمئة %" accent={C.cyan} />
                )}
              </View>
            )}

            {(showTemp || (showECG && !showHR && !showSPO2)) && (
              <View style={styles.row}>
                {showTemp && (
                  <VitalCard type="temp" icon="🌡️" label="درجة الحرارة"
                    value={vitals.temperature}
                    unit="درجة مئوية °C" accent={C.orange} />
                )}
                {showECG && !showHR && !showSPO2 && (
                  <VitalCard type="ecg" icon="📡" label="ECG الخام"
                    value={vitals.leadsOff ? 0 : vitals.ecgVoltage}
                    unit="فولت (0–3.3V)" accent={C.green} />
                )}
              </View>
            )}

            {activeSensor === 'all' && (
              <View style={styles.row}>
                <VitalCard type="temp" icon="🌡️" label="درجة الحرارة"
                  value={vitals.temperature}
                  unit="درجة مئوية °C" accent={C.orange} />
                <VitalCard type="ecg" icon="📡" label="ECG الخام"
                  value={vitals.leadsOff ? 0 : vitals.ecgVoltage}
                  unit="فولت (0–3.3V)" accent={C.green} />
              </View>
            )}

            {showECG && <ECGChart data={ecgBuffer} leadsOff={vitals.leadsOff} />}

            {(showHR || showSPO2) && !vitals.validHR && !vitals.validSPO2 && !vitals.leadsOff && (
              <View style={styles.fingerNote}>
                <Text style={styles.fingerText}>
                  {"💡 ضع إصبعك على MAX30102 للحصول على قراءة القلب والأكسجين"}
                </Text>
              </View>
            )}

            <TouchableOpacity style={styles.stopBtn} onPress={disconnect}>
              <Text style={styles.stopText}>{"⏹  إيقاف المراقبة"}</Text>
            </TouchableOpacity>
          </React.Fragment>
        )}

        <View style={{ height: 24 }} />
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safe:    { flex: 1, backgroundColor: C.bg },
  scroll:  { flex: 1 },
  content: { paddingHorizontal: S.md, paddingTop: S.md },
  alertsBox: { position: 'absolute', top: 54, left: 16, right: 16, zIndex: 999 },
  header: {
    flexDirection: 'row', justifyContent: 'space-between',
    alignItems: 'center', paddingVertical: S.md, marginBottom: 6,
  },
  headerLeft: { flexDirection: 'row', alignItems: 'center', gap: 10 },
  logo:     { width: 42, height: 42, borderRadius: 10 },
  appTitle: { color: C.text, fontSize: 17, fontWeight: '900', letterSpacing: -0.3 },
  appSub:   { color: C.textDim, fontSize: 10, marginTop: 1, letterSpacing: 0.5 },
  statusPill: {
    flexDirection: 'row', alignItems: 'center', gap: 6,
    backgroundColor: C.surface, borderRadius: R.full,
    paddingHorizontal: 12, paddingVertical: 7, borderWidth: 1,
  },
  dot:         { width: 8, height: 8, borderRadius: 4 },
  statusLabel: { fontSize: 12, fontWeight: '600' },
  errorBar: {
    backgroundColor: C.redBg, borderRadius: R.md,
    borderWidth: 1, borderColor: C.red + '44',
    padding: 12, marginBottom: 12,
    flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center',
  },
  errorText: { color: C.red,  fontSize: 13, flex: 1 },
  retryText: { color: C.cyan, fontSize: 13, fontWeight: '700', marginLeft: 10 },
  connectBox: {
    backgroundColor: C.surface, borderRadius: R.xl,
    borderWidth: 1, borderColor: C.border,
    padding: S.lg, alignItems: 'center', marginTop: S.md,
  },
  connectLogo:    { width: 100, height: 100, marginBottom: 16, borderRadius: 22 },
  connectTitle:   { color: C.text,    fontSize: 22, fontWeight: '900', marginBottom: 8 },
  connectDesc:    { color: C.textMid, fontSize: 13, textAlign: 'center', lineHeight: 21, marginBottom: 22 },
  connectBtn:     { borderRadius: R.md, paddingVertical: 14, paddingHorizontal: 40 },
  connectBtnText: { color: '#000', fontWeight: '900', fontSize: 15 },
  settingsLink:   { marginTop: 14 },
  settingsLinkText: { color: C.textMid, fontSize: 13 },
  connectingBox: {
    backgroundColor: C.surface, borderRadius: R.md,
    padding: 24, alignItems: 'center', marginTop: S.md,
    borderWidth: 1, borderColor: C.border,
  },
  connectingLogo: { width: 60, height: 60, marginBottom: 14, borderRadius: 14, opacity: 0.7 },
  connectingText: { color: C.cyan, fontSize: 15, fontWeight: '600' },
  leadsOffBar: {
    backgroundColor: C.redBg, borderRadius: R.md,
    borderWidth: 1, borderColor: C.red + '55',
    padding: 12, marginBottom: 12,
  },
  leadsOffText: { color: C.red, fontSize: 13, fontWeight: '600', textAlign: 'center' },
  sectionTitle: {
    color: C.textMid, fontSize: 11, fontWeight: '700',
    textTransform: 'uppercase', letterSpacing: 1.2,
    marginBottom: 8, marginTop: 4,
  },
  row: { flexDirection: 'row', marginBottom: 2 },
  fingerNote: {
    backgroundColor: C.cyanBg, borderRadius: R.md,
    borderWidth: 1, borderColor: C.cyan + '33',
    padding: 12, marginBottom: 10,
  },
  fingerText: { color: C.cyan, fontSize: 12, textAlign: 'center', lineHeight: 18 },
  stopBtn: {
    backgroundColor: C.redBg, borderRadius: R.md,
    borderWidth: 1, borderColor: C.red + '44',
    paddingVertical: 13, alignItems: 'center', marginTop: 6,
  },
  stopText: { color: C.red, fontWeight: '700', fontSize: 14 },
});
