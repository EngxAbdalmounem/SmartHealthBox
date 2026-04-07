import React, { useEffect, useRef } from 'react';
import { View, Text, StyleSheet, Animated } from 'react-native';
import { C, R } from '../utils/theme';

function classify(type, value) {
  if (!value || value <= 0) return { label: '—', color: C.textDim };
  switch (type) {
    case 'hr':
      if (value < 50)  return { label: 'بطيء',     color: C.orange };
      if (value > 120) return { label: 'سريع جداً', color: C.red   };
      if (value > 100) return { label: 'سريع',      color: C.orange };
      return               { label: 'طبيعي',     color: C.green };
    case 'spo2':
      if (value < 94)  return { label: 'خطر',    color: C.red    };
      if (value < 96)  return { label: 'منخفض',  color: C.orange };
      return               { label: 'طبيعي',  color: C.green  };
    case 'temp':
      if (value > 38.5) return { label: 'حمى',    color: C.red    };
      if (value > 37.5) return { label: 'مرتفع',  color: C.orange };
      if (value < 36.0) return { label: 'منخفض',  color: C.cyan   };
      return                { label: 'طبيعي',  color: C.green  };
    case 'ecg':
      return value > 0
        ? { label: 'نشط',      color: C.green  }
        : { label: 'لا إشارة', color: C.textDim };
    default:
      return { label: '', color: C.textDim };
  }
}

export default function VitalCard({ type, icon, label, value, unit, accent }) {
  const scale = useRef(new Animated.Value(1)).current;

  useEffect(() => {
    if (!value || value <= 0) return;
    Animated.sequence([
      Animated.timing(scale, { toValue: 1.03, duration: 100, useNativeDriver: true }),
      Animated.timing(scale, { toValue: 1,    duration: 150, useNativeDriver: true }),
    ]).start();
  }, [value]);

  const cls = classify(type, value);

  let display = '--';
  if (value && value > 0) {
    if (type === 'temp') display = value.toFixed(1);
    else if (type === 'ecg') display = value.toFixed(2);
    else display = String(value);
  }

  return (
    <Animated.View style={[styles.card, { borderColor: accent + '30', transform: [{ scale }] }]}>
      <View style={[styles.stripe, { backgroundColor: accent }]} />
      <View style={styles.top}>
        <Text style={styles.icon}>{icon}</Text>
        <View style={[styles.badge, { backgroundColor: cls.color + '1a', borderColor: cls.color + '44' }]}>
          <Text style={[styles.badgeText, { color: cls.color }]}>{cls.label}</Text>
        </View>
      </View>
      <Text style={styles.label}>{label}</Text>
      <Text style={[styles.value, { color: accent }]}>{display}</Text>
      <Text style={styles.unit}>{unit}</Text>
    </Animated.View>
  );
}

const styles = StyleSheet.create({
  card: {
    flex: 1, margin: 5,
    backgroundColor: C.card,
    borderRadius: R.lg, borderWidth: 1,
    padding: 14, overflow: 'hidden', minHeight: 148,
  },
  stripe: {
    position: 'absolute', top: 0, left: 0, right: 0,
    height: 3, borderTopLeftRadius: R.lg, borderTopRightRadius: R.lg, opacity: 0.7,
  },
  top: {
    flexDirection: 'row', justifyContent: 'space-between',
    alignItems: 'flex-start', marginTop: 6, marginBottom: 8,
  },
  icon: { fontSize: 20 },
  badge: {
    borderRadius: R.full, borderWidth: 1,
    paddingHorizontal: 7, paddingVertical: 2,
  },
  badgeText: { fontSize: 10, fontWeight: '700' },
  label: {
    color: C.textDim, fontSize: 10, fontWeight: '700',
    textTransform: 'uppercase', letterSpacing: 0.8, marginBottom: 4,
  },
  value: { fontSize: 30, fontWeight: '800', letterSpacing: -0.5, lineHeight: 34 },
  unit:  { color: C.textDim, fontSize: 10, marginTop: 3 },
});
