import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../state/app_state.dart';

class DiagnosticsScreen extends ConsumerWidget {
  const DiagnosticsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final settings = ref.watch(currentSettingsProvider);
    final events = ref.watch(diagnosticsProvider);
    return Scaffold(
      appBar: AppBar(title: const Text('Chẩn đoán')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            child: Column(
              children: [
                ListTile(
                  title: const Text('API'),
                  subtitle: Text(settings.apiBaseUrl),
                ),
                ListTile(
                  title: const Text('Thiết bị'),
                  subtitle: Text(settings.deviceId),
                ),
                ListTile(
                  title: const Text('Nguồn'),
                  subtitle: Text(settings.source.label),
                ),
              ],
            ),
          ),
          const Padding(
            padding: EdgeInsets.fromLTRB(8, 18, 8, 8),
            child: Text('Nhật ký phiên'),
          ),
          if (events.isEmpty)
            const Card(child: ListTile(title: Text('Chưa có sự kiện')))
          else
            ...events.map(
              (event) => Card(
                child: ListTile(
                  leading: const Icon(Icons.info_outline),
                  title: Text(event.message),
                  subtitle: Text(event.timestamp.toLocal().toIso8601String()),
                ),
              ),
            ),
          const Padding(
            padding: EdgeInsets.all(8),
            child: Text(
              'Token API được lưu trong secure storage và không xuất hiện ở màn hình hoặc nhật ký.',
            ),
          ),
        ],
      ),
    );
  }
}
