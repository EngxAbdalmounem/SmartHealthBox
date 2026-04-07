import React, { useEffect, useRef } from 'react';
import { Animated, Text, View, TouchableOpacity, StyleSheet } from 'react-native';
import { C, R } from '../utils/theme';

const TYPE = {
  danger:  { border: C.red,    bg: C.redBg    },
  warning: { border: C.orange, bg: C.orangeBg },
  info:    { border: C.cyan,   bg: C.cyanBg   },
};

export default function AlertBanner({ alert, onDismiss }) {
  const ty = useRef(new Animated.Value(-70)).current;
  const op = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    Animated.parallel([
      Animated.spring(ty, { toValue: 0, tension: 80, friction: 9, useNativeDriver: true }),
      Animated.timing(op, { toValue: 1, duration: 200, useNativeDriver: true }),
    ]).start();
    const t = setTimeout(dismiss, 7000);
    return () => clearTimeout(t);
  }, []);

  const dismiss = () => {
    Animated.parallel([
      Animated.timing(ty, { toValue: -70, duration: 220, useNativeDriver: true }),
      Animated.timing(op, { toValue: 0,   duration: 180, useNativeDriver: true }),
    ]).start(() => onDismiss(alert.id));
  };

  const s = TYPE[alert.type] || TYPE.info;

  return (
    <Animated.View style={[
      styles.banner,
      { transform: [{ translateY: ty }], opacity: op,
        backgroundColor: s.bg, borderLeftColor: s.border }
    ]}>
      <View style={styles.body}>
        <Text style={styles.title}>{alert.title}</Text>
        <Text style={styles.bodyText}>{alert.body}</Text>
        <Text style={styles.time}>{alert.time?.toLocaleTimeString('ar')}</Text>
      </View>
      <TouchableOpacity onPress={dismiss} hitSlop={{ top:10, bottom:10, left:10, right:10 }}>
        <Text style={styles.close}>✕</Text>
      </TouchableOpacity>
    </Animated.View>
  );
}

const styles = StyleSheet.create({
  banner: {
    flexDirection: 'row', alignItems: 'center',
    borderRadius: R.md, borderLeftWidth: 3,
    paddingHorizontal: 14, paddingVertical: 10,
    marginBottom: 6, borderWidth: 1, borderColor: 'rgba(255,255,255,0.06)',
  },
  body:     { flex: 1 },
  title:    { color: C.text, fontWeight: '800', fontSize: 13 },
  bodyText: { color: C.textMid, fontSize: 12, marginTop: 1 },
  time:     { color: C.textDim, fontSize: 10, marginTop: 3, fontFamily: 'monospace' },
  close:    { color: C.textDim, fontSize: 16, paddingLeft: 10 },
});
