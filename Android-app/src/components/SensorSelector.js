import React from 'react';
import { View, Text, TouchableOpacity, StyleSheet, ScrollView } from 'react-native';
import { useHealth, SENSORS } from '../context/HealthContext';
import { C, R } from '../utils/theme';

export default function SensorSelector() {
  const { activeSensor, selectSensor } = useHealth();

  return (
    <View style={styles.wrap}>
      <Text style={styles.label}>اختر الحساس</Text>
      <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.row}>
        {Object.values(SENSORS).map(s => {
          const active = activeSensor === s.key;
          return (
            <TouchableOpacity
              key={s.key}
              style={[styles.btn, active && styles.btnActive]}
              onPress={() => selectSensor(s.key)}
              activeOpacity={0.7}
            >
              <Text style={styles.icon}>{s.icon}</Text>
              <Text style={[styles.btnText, active && styles.btnTextActive]}>{s.label}</Text>
              {active && <View style={styles.activeDot} />}
            </TouchableOpacity>
          );
        })}
      </ScrollView>
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: {
    marginBottom: 16,
  },
  label: {
    color: C.textDim, fontSize: 11, fontWeight: '700',
    textTransform: 'uppercase', letterSpacing: 1.2,
    marginBottom: 10,
  },
  row: {
    gap: 8, paddingHorizontal: 2,
  },
  btn: {
    alignItems: 'center', justifyContent: 'center',
    backgroundColor: C.surface, borderRadius: R.lg,
    borderWidth: 1, borderColor: C.border,
    paddingHorizontal: 16, paddingVertical: 10,
    minWidth: 72, gap: 4,
    position: 'relative',
  },
  btnActive: {
    backgroundColor: C.cyanBg,
    borderColor: C.cyan,
  },
  icon: { fontSize: 20 },
  btnText: {
    color: C.textDim, fontSize: 12, fontWeight: '700',
  },
  btnTextActive: {
    color: C.cyan,
  },
  activeDot: {
    position: 'absolute', bottom: 6,
    width: 4, height: 4, borderRadius: 2,
    backgroundColor: C.cyan,
  },
});
