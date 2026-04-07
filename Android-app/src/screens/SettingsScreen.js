import React from 'react';
import {
  View, Text, TextInput, StyleSheet,
  TouchableOpacity, SafeAreaView, ScrollView,
  StatusBar, Alert,
} from 'react-native';
import { LinearGradient } from 'expo-linear-gradient';
import { useHealth } from '../context/HealthContext';
import { C, R, S } from '../utils/theme';

function Field({ label, hint, value, onChange, keyboard, placeholder }) {
  return (
    <View style={styles.field}>
      <Text style={styles.fieldLabel}>{label}</Text>
      {!!hint && <Text style={styles.fieldHint}>{hint}</Text>}
      <TextInput
        style={styles.input} value={value} onChangeText={onChange}
        keyboardType={keyboard || 'default'} placeholder={placeholder}
        placeholderTextColor={C.textDim} autoCorrect={false}
        autoCapitalize="none" textAlign="left"
      />
    </View>
  );
}

function InfoRow({ label, val, color }) {
  return (
    <View style={styles.infoRow}>
      <Text style={styles.infoLabel}>{label}</Text>
      <Text style={[styles.infoVal, { color: color || C.textMid }]}>{val}</Text>
    </View>
  );
}

export default function SettingsScreen({ navigation }) {
  const { ip, setIp, port, setPort, connect, disconnect, status } = useHealth();
  const connected = status === 'connected' || status === 'connecting';

  const handleConnect = () => { connect(); navigation.navigate('Home'); };
  const handleDisconnect = () => {
    Alert.alert('إيقاف الاتصال', 'هل تريد قطع الاتصال بالجهاز؟', [
      { text: 'إلغاء', style: 'cancel' },
      { text: 'قطع', style: 'destructive', onPress: disconnect },
    ]);
  };

  return (
    <SafeAreaView style={styles.safe}>
      <StatusBar barStyle="light-content" backgroundColor={C.bg} />
      <ScrollView contentContainerStyle={styles.content} showsVerticalScrollIndicator={false}>
        <Text style={styles.title}>⚙️ الإعدادات</Text>

        {/* إعدادات الاتصال */}
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>إعدادات Arduino</Text>
          <Field label="عنوان IP" hint="افتح Serial Monitor في Arduino IDE — ستجد IP مثل: 192.168.1.105"
            value={ip} onChange={setIp} keyboard="numeric" placeholder="192.168.1.100" />
          <Field label="المنفذ (Port)" hint="الافتراضي 80"
            value={port} onChange={setPort} keyboard="numeric" placeholder="80" />
          <TouchableOpacity onPress={handleConnect} activeOpacity={0.85}>
            <LinearGradient colors={['#00b4d8', '#006e99']} style={styles.connectBtn}
              start={{ x: 0, y: 0 }} end={{ x: 1, y: 0 }}>
              <Text style={styles.connectBtnText}>🔌  حفظ والاتصال</Text>
            </LinearGradient>
          </TouchableOpacity>
          {connected && (
            <TouchableOpacity style={styles.disconnectBtn} onPress={handleDisconnect}>
              <Text style={styles.disconnectText}>قطع الاتصال</Text>
            </TouchableOpacity>
          )}
        </View>

        {/* الحالة الحالية */}
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>الحالة الحالية</Text>
          <InfoRow label="حالة الاتصال"
            val={status === 'connected' ? 'متصل ✓' : status === 'connecting' ? 'جارٍ الاتصال...' : status === 'error' ? 'خطأ' : 'غير متصل'}
            color={status === 'connected' ? C.green : status === 'error' ? C.red : C.textMid} />
          <InfoRow label="الجهاز"    val={`${ip}:${port}`} color={C.cyan} />
          <InfoRow label="التحديث"   val="كل 1 ثانية" />
          <InfoRow label="نقطة البيانات" val="/data" color={C.green} />
        </View>

        {/* الحساسات المتاحة */}
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>الحساسات المتاحة</Text>
          {[
            { name: '📊 الكل',        desc: 'تشغيل جميع الحساسات معاً',   cmd: 'all'  },
            { name: '❤️ القلب',       desc: 'معدل القلب والأكسجين فقط',   cmd: 'hr'   },
            { name: '🫁 الأكسجين',    desc: 'SpO₂ فقط',                  cmd: 'spo2' },
            { name: '🌡️ الحرارة',    desc: 'درجة الحرارة فقط',           cmd: 'temp' },
            { name: '📈 ECG',         desc: 'مخطط كهربائية القلب فقط',    cmd: 'ecg'  },
          ].map(s => (
            <View key={s.cmd} style={styles.sensorRow}>
              <Text style={styles.sensorName}>{s.name}</Text>
              <Text style={styles.sensorDesc}>{s.desc}</Text>
            </View>
          ))}
          <Text style={styles.sensorNote}>
            اختر الحساس من الشاشة الرئيسية بعد الاتصال
          </Text>
        </View>

        {/* مرجع التوصيل */}
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>توصيل الأجهزة</Text>
          {[
            { name: 'AD8232 ECG', color: C.green,
              wires: ['3.3V → 3V3', 'GND → GND', 'OUTPUT → A0', 'LO+ → D2', 'LO− → D3'] },
            { name: 'MAX30102  HR / SpO₂', color: C.cyan,
              wires: ['VIN → 3.3V', 'GND → GND', 'SDA → SDA', 'SCL → SCL'] },
            { name: 'MAX30205  Temperature', color: C.orange,
              wires: ['VIN → 3.3V', 'GND → GND', 'SDA → SDA (مشترك)', 'SCL → SCL (مشترك)'] },
          ].map(s => (
            <View key={s.name} style={[styles.sensorCard, { borderLeftColor: s.color }]}>
              <Text style={[styles.sensorCardName, { color: s.color }]}>{s.name}</Text>
              {s.wires.map(w => <Text key={w} style={styles.wire}>• {w}</Text>)}
            </View>
          ))}
        </View>

        <View style={styles.footer}>
          <Text style={styles.footerText}>Smart Health Box v1.0 · Arduino Nano 33 IoT</Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safe:    { flex: 1, backgroundColor: C.bg },
  content: { padding: S.md, paddingBottom: 40 },
  title:   { color: C.text, fontSize: 24, fontWeight: '900', marginBottom: S.lg },
  section: {
    backgroundColor: C.surface, borderRadius: R.lg,
    borderWidth: 1, borderColor: C.border,
    padding: S.md, marginBottom: S.md,
  },
  sectionTitle: {
    color: C.cyan, fontSize: 11, fontWeight: '800',
    textTransform: 'uppercase', letterSpacing: 1.2, marginBottom: 14,
  },
  field:      { marginBottom: 14 },
  fieldLabel: { color: C.textMid, fontSize: 12, fontWeight: '700', marginBottom: 2 },
  fieldHint:  { color: C.textDim, fontSize: 11, marginBottom: 5 },
  input: {
    backgroundColor: C.surface2, borderRadius: R.sm,
    borderWidth: 1, borderColor: C.border,
    paddingHorizontal: 12, paddingVertical: 10,
    color: C.text, fontSize: 14, fontFamily: 'monospace',
  },
  connectBtn:     { borderRadius: R.md, paddingVertical: 13, alignItems: 'center', marginTop: 4 },
  connectBtnText: { color: '#000', fontWeight: '900', fontSize: 15 },
  disconnectBtn: {
    marginTop: 10, paddingVertical: 11, alignItems: 'center',
    borderRadius: R.md, borderWidth: 1, borderColor: C.red + '44',
    backgroundColor: C.redBg,
  },
  disconnectText: { color: C.red, fontWeight: '700', fontSize: 13 },
  infoRow: {
    flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center',
    paddingVertical: 9, borderBottomWidth: 1, borderBottomColor: C.border + '55',
  },
  infoLabel: { color: C.textDim, fontSize: 13 },
  infoVal:   { fontSize: 13, fontWeight: '600', fontFamily: 'monospace', maxWidth: '60%', textAlign: 'right' },
  sensorRow: {
    paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: C.border + '33',
  },
  sensorName: { color: C.text, fontSize: 13, fontWeight: '700' },
  sensorDesc: { color: C.textDim, fontSize: 11, marginTop: 2 },
  sensorNote: { color: C.cyan, fontSize: 11, marginTop: 10, fontStyle: 'italic' },
  sensorCard: {
    backgroundColor: C.surface2, borderRadius: R.md,
    borderLeftWidth: 3, padding: 12, marginBottom: 10,
  },
  sensorCardName: { fontWeight: '800', fontSize: 13, marginBottom: 7 },
  wire:           { color: C.textDim, fontSize: 12, lineHeight: 22, fontFamily: 'monospace' },
  footer:     { alignItems: 'center', paddingTop: 8 },
  footerText: { color: C.textDim, fontSize: 11 },
});
