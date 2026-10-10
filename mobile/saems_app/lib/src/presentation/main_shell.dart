import 'package:flutter/material.dart';

import 'dashboard_screen.dart';
import 'device_screen.dart';
import 'history_screen.dart';
import 'nearby_screen.dart';
import 'settings_screen.dart';

class MainShell extends StatefulWidget {
  const MainShell({super.key});

  @override
  State<MainShell> createState() => _MainShellState();
}

class _MainShellState extends State<MainShell> {
  int _index = 0;

  static const _screens = [
    DashboardScreen(),
    HistoryScreen(),
    NearbyScreen(),
    DeviceScreen(),
    SettingsScreen(),
  ];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: IndexedStack(index: _index, children: _screens),
      ),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _index,
        onDestinationSelected: (value) => setState(() => _index = value),
        destinations: const [
          NavigationDestination(
            icon: Icon(Icons.dashboard_outlined),
            label: 'Tổng quan',
          ),
          NavigationDestination(icon: Icon(Icons.show_chart), label: 'Lịch sử'),
          NavigationDestination(icon: Icon(Icons.sensors), label: 'Kết nối'),
          NavigationDestination(icon: Icon(Icons.memory), label: 'Thiết bị'),
          NavigationDestination(
            icon: Icon(Icons.settings_outlined),
            label: 'Cài đặt',
          ),
        ],
      ),
    );
  }
}
