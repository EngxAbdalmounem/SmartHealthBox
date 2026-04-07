import React, { useMemo } from 'react';
import { View, Text, StyleSheet, Dimensions } from 'react-native';
import Svg, { Polyline, Line, Path, Defs, LinearGradient, Stop } from 'react-native-svg';
import { C, R } from '../utils/theme';

const W = Dimensions.get('window').width - 32;
const H = 160;

export default function ECGChart({ data, leadsOff }) {
  const { pointsStr, areaPath } = useMemo(() => {
    if (!data || data.length < 2) return { pointsStr: '', areaPath: '' };
    const maxPts = Math.floor(W / 2.2);
    const slice  = data.slice(-maxPts);
    const step   = W / maxPts;
    const midY   = H / 2;
    const scaleY = (H * 0.38) / 1.65;
    let pts = '', area = `M 0 ${H}`;
    slice.forEach((v, i) => {
      const x = i * step;
      const y = midY - ((v - 1.65) * scaleY);
      pts  += `${x.toFixed(1)},${y.toFixed(1)} `;
      area += ` L ${x.toFixed(1)} ${y.toFixed(1)}`;
    });
    area += ` L ${((slice.length - 1) * step).toFixed(1)} ${H} Z`;
    return { pointsStr: pts.trim(), areaPath: area };
  }, [data]);

  const grid = [];
  for (let y = 0; y <= H; y += 32)
    grid.push(<Line key={`h${y}`} x1="0" y1={y} x2={W} y2={y} stroke="rgba(0,180,216,0.05)" strokeWidth="1" />);
  for (let x = 0; x <= W; x += 40)
    grid.push(<Line key={`v${x}`} x1={x} y1="0" x2={x} y2={H} stroke="rgba(0,180,216,0.05)" strokeWidth="1" />);

  return (
    <View style={styles.wrap}>
      <View style={styles.header}>
        <Text style={styles.title}>مخطط كهربائية القلب  ECG</Text>
        {leadsOff
          ? <View style={styles.offBadge}><Text style={styles.offText}>أقطاب منفصلة</Text></View>
          : <View style={styles.onBadge}><Text  style={styles.onText}>● بث مباشر</Text></View>
        }
      </View>
      <View style={styles.canvas}>
        <Svg width={W} height={H}>
          <Defs>
            <LinearGradient id="fill" x1="0" y1="0" x2="0" y2="1">
              <Stop offset="0" stopColor="#00e676" stopOpacity="0.18" />
              <Stop offset="1" stopColor="#00e676" stopOpacity="0" />
            </LinearGradient>
          </Defs>
          {grid}
          <Line x1="0" y1={H/2} x2={W} y2={H/2}
            stroke="rgba(0,230,118,0.12)" strokeWidth="1" strokeDasharray="6,5" />
          {areaPath  && <Path d={areaPath} fill="url(#fill)" />}
          {pointsStr && (
            <Polyline points={pointsStr} fill="none"
              stroke={leadsOff ? C.textDim : C.green}
              strokeWidth="1.8" strokeLinejoin="round" strokeLinecap="round" />
          )}
        </Svg>
      </View>
      <View style={styles.yAxis}>
        <Text style={styles.yLabel}>3.3V</Text>
        <Text style={styles.yLabel}>1.65V</Text>
        <Text style={styles.yLabel}>0V</Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: {
    backgroundColor: C.card, borderRadius: R.lg,
    borderWidth: 1, borderColor: C.border,
    padding: 14, marginBottom: 12,
  },
  header: {
    flexDirection: 'row', justifyContent: 'space-between',
    alignItems: 'center', marginBottom: 10,
  },
  title: { color: C.green, fontWeight: '700', fontSize: 13 },
  onBadge: {
    backgroundColor: C.greenBg, borderRadius: R.full,
    paddingHorizontal: 10, paddingVertical: 3,
    borderWidth: 1, borderColor: C.green + '44',
  },
  onText:  { color: C.green, fontSize: 10, fontWeight: '700' },
  offBadge: {
    backgroundColor: C.redBg, borderRadius: R.full,
    paddingHorizontal: 10, paddingVertical: 3,
    borderWidth: 1, borderColor: C.red + '55',
  },
  offText: { color: C.red, fontSize: 10, fontWeight: '700' },
  canvas:  { backgroundColor: '#010a12', borderRadius: R.md, overflow: 'hidden' },
  yAxis:   {
    position: 'absolute', right: 18, top: 46, bottom: 14,
    justifyContent: 'space-between',
  },
  yLabel: { color: C.textDim, fontSize: 9, fontFamily: 'monospace' },
});
