import React, { useState } from 'react';
import {
  View, Text, FlatList, StyleSheet,
  SafeAreaView, StatusBar, TouchableOpacity,
} from 'react-native';
import { useHealth } from '../context/HealthContext';
import { C, R, S } from '../utils/theme';

function rowStatus(r) {
  if (r.validSPO2 && r.spo2 > 0 && r.spo2 < 94)    return { label: 'خطر',    color: C.red    };
  if (r.temperature > 38.5)                           return { label: 'حمى',    color: C.red    };
  if ((r.validSPO2 && r.spo2 < 96) || r.temperature > 37.5 || (r.validHR && r.heartRate > 100))
    return { label: 'مراقبة', color: C.orange };
  if (r.validHR || r.validSPO2)
    return { label: 'طبيعي', color: C.green };
  return { label: '—', color: C.textDim };
}

function Row({ item, index }) {
  const st   = rowStatus(item);
  const time = item.ts ? new Date(item.ts).toLocaleTimeString('ar') : '--';
  return (
    <View style={[styles.row, index % 2 === 1 && styles.rowAlt]}>
      <Text style={[styles.cell, styles.cellTime]}>{time}</Text>
      <Text style={[styles.cell, { color: C.red    }]}>{item.validHR   && item.heartRate   > 0 ? item.heartRate   : '—'}</Text>
      <Text style={[styles.cell, { color: C.cyan   }]}>{item.validSPO2 && item.spo2        > 0 ? item.spo2        : '—'}</Text>
      <Text style={[styles.cell, { color: C.orange }]}>{item.temperature > 0 ? item.temperature.toFixed(1) : '—'}</Text>
      <View style={[styles.pill, { backgroundColor: st.color + '1a', borderColor: st.color + '44' }]}>
        <Text style={[styles.pillText, { color: st.color }]}>{st.label}</Text>
      </View>
    </View>
  );
}

export default function HistoryScreen() {
  const { history, alerts, clearHistory } = useHealth();
  const [tab, setTab] = useState('readings');

  return (
    <SafeAreaView style={styles.safe}>
      <StatusBar barStyle="light-content" backgroundColor={C.bg} />
      <View style={styles.header}>
        <Text style={styles.title}>السجل الطبي</Text>
        {tab === 'readings' && history.length > 0 && (
          <TouchableOpacity onPress={clearHistory}>
            <Text style={styles.clearBtn}>مسح</Text>
          </TouchableOpacity>
        )}
      </View>

      <View style={styles.tabs}>
        {['readings', 'alerts'].map(t => (
          <TouchableOpacity key={t}
            style={[styles.tab, tab === t && styles.tabActive]}
            onPress={() => setTab(t)}>
            <Text style={[styles.tabText, tab === t && styles.tabTextActive]}>
              {t === 'readings' ? `📊 القراءات (${history.length})` : `🔔 التنبيهات (${alerts.length})`}
            </Text>
          </TouchableOpacity>
        ))}
      </View>

      {tab === 'readings' ? (
        <>
          <View style={styles.thead}>
            {['الوقت', 'القلب', 'SpO₂', '°C', 'الحالة'].map(h => (
              <Text key={h} style={styles.th}>{h}</Text>
            ))}
          </View>
          {history.length === 0
            ? <Empty icon="📋" text="لا توجد قراءات" sub="ابدأ المراقبة من الشاشة الرئيسية" />
            : <FlatList data={history} keyExtractor={(_, i) => String(i)}
                renderItem={({ item, index }) => <Row item={item} index={index} />}
                showsVerticalScrollIndicator={false} />
          }
        </>
      ) : (
        alerts.length === 0
          ? <Empty icon="🔔" text="لا توجد تنبيهات" sub="" />
          : <FlatList data={alerts} keyExtractor={a => String(a.id)}
              contentContainerStyle={{ padding: S.md }}
              showsVerticalScrollIndicator={false}
              renderItem={({ item: a }) => {
                const colors = { danger: C.red, warning: C.orange, info: C.cyan };
                const cc = colors[a.type] || C.cyan;
                return (
                  <View style={[styles.alertCard, { borderLeftColor: cc, backgroundColor: cc + '10' }]}>
                    <Text style={[styles.alertTitle, { color: cc }]}>{a.title}</Text>
                    <Text style={styles.alertBody}>{a.body}</Text>
                    <Text style={styles.alertTime}>{a.time ? new Date(a.time).toLocaleTimeString('ar') : ''}</Text>
                  </View>
                );
              }}
            />
      )}
    </SafeAreaView>
  );
}

function Empty({ icon, text, sub }) {
  return (
    <View style={styles.empty}>
      <Text style={styles.emptyIcon}>{icon}</Text>
      <Text style={styles.emptyText}>{text}</Text>
      {!!sub && <Text style={styles.emptySub}>{sub}</Text>}
    </View>
  );
}

const styles = StyleSheet.create({
  safe:   { flex: 1, backgroundColor: C.bg },
  header: {
    flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center',
    paddingHorizontal: S.md, paddingTop: S.md, paddingBottom: 4,
  },
  title:    { color: C.text, fontSize: 22, fontWeight: '900' },
  clearBtn: { color: C.red, fontSize: 13, fontWeight: '700' },
  tabs: {
    flexDirection: 'row', marginHorizontal: S.md, marginBottom: 10,
    backgroundColor: C.surface, borderRadius: R.md, padding: 4,
  },
  tab:           { flex: 1, paddingVertical: 9, alignItems: 'center', borderRadius: R.sm },
  tabActive:     { backgroundColor: C.surface2 },
  tabText:       { color: C.textDim, fontSize: 13, fontWeight: '600' },
  tabTextActive: { color: C.text },
  thead: {
    flexDirection: 'row', paddingHorizontal: S.md,
    paddingVertical: 8, borderBottomWidth: 1, borderBottomColor: C.border,
  },
  th:   { flex: 1, color: C.textDim, fontSize: 10, fontWeight: '700', textTransform: 'uppercase', letterSpacing: 0.5 },
  row:  { flexDirection: 'row', paddingHorizontal: S.md, paddingVertical: 11, alignItems: 'center' },
  rowAlt: { backgroundColor: 'rgba(7,17,30,0.7)' },
  cell: { flex: 1, fontSize: 13, fontWeight: '600', color: C.textMid },
  cellTime: { color: C.textDim, fontSize: 11, fontFamily: 'monospace' },
  pill: { flex: 1, borderRadius: R.full, borderWidth: 1, paddingHorizontal: 6, paddingVertical: 2, alignItems: 'center' },
  pillText: { fontSize: 10, fontWeight: '700' },
  empty:     { alignItems: 'center', paddingTop: 80 },
  emptyIcon: { fontSize: 44, marginBottom: 14 },
  emptyText: { color: C.textMid, fontSize: 16, fontWeight: '700' },
  emptySub:  { color: C.textDim, fontSize: 13, marginTop: 6 },
  alertCard: { borderRadius: R.md, borderLeftWidth: 3, padding: 14, marginBottom: 10 },
  alertTitle: { fontWeight: '800', fontSize: 14, marginBottom: 3 },
  alertBody:  { color: C.textMid, fontSize: 13 },
  alertTime:  { color: C.textDim, fontSize: 10, marginTop: 4, fontFamily: 'monospace' },
});
