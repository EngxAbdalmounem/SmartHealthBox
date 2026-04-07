import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { StatusBar } from 'expo-status-bar';
import { Text } from 'react-native';

import { HealthProvider } from './src/context/HealthContext';
import HomeScreen     from './src/screens/HomeScreen';
import HistoryScreen  from './src/screens/HistoryScreen';
import SettingsScreen from './src/screens/SettingsScreen';
import { C } from './src/utils/theme';

const Tab = createBottomTabNavigator();

const ICONS  = { Home: '❤️', History: '📊', Settings: '⚙️' };
const LABELS = { Home: 'الرئيسية', History: 'السجل', Settings: 'الإعدادات' };

export default function App() {
  return (
    <HealthProvider>
      <NavigationContainer>
        <StatusBar style="light" backgroundColor={C.bg} />
        <Tab.Navigator
          screenOptions={({ route }) => ({
            headerShown: false,
            tabBarStyle: {
              backgroundColor: C.surface,
              borderTopColor: C.border,
              borderTopWidth: 1,
              height: 62, paddingBottom: 8,
            },
            tabBarActiveTintColor:   C.cyan,
            tabBarInactiveTintColor: C.textDim,
            tabBarLabelStyle: { fontSize: 11, fontWeight: '700' },
            tabBarIcon: ({ focused }) => (
              <Text style={{ fontSize: 19, opacity: focused ? 1 : 0.4 }}>
                {ICONS[route.name]}
              </Text>
            ),
          })}
        >
          <Tab.Screen name="Home"     component={HomeScreen}     options={{ tabBarLabel: LABELS.Home     }} />
          <Tab.Screen name="History"  component={HistoryScreen}  options={{ tabBarLabel: LABELS.History  }} />
          <Tab.Screen name="Settings" component={SettingsScreen} options={{ tabBarLabel: LABELS.Settings }} />
        </Tab.Navigator>
      </NavigationContainer>
    </HealthProvider>
  );
}
