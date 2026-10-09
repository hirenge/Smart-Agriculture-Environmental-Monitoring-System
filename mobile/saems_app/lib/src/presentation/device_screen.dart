import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../state/app_state.dart';

class DeviceScreen extends ConsumerStatefulWidget {
  const DeviceScreen({super.key});

  @override
  ConsumerState<DeviceScreen> createState() => _DeviceScreenState();
}

class _DeviceScreenState extends ConsumerState<DeviceScreen> {
  bool _sending = false;

  Future<void> _setPump(bool enabled) async {
    setState(() => _sending = true);
    final settings = ref.read(currentSettingsProvider);
    try {
      final result = await ref
          .read(telemetryRepositoryProvider)
          .setPump(settings.deviceId, enabled: enabled, durationSeconds: 60);
      ref
          .read(diagnosticsProvider.notifier)
          .add('Lệnh bơm ${enabled ? 'ON' : 'OFF'}: ${result.status}');
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Đã gửi lệnh: ${result.status}')),
        );
      }
    } catch (error) {
      ref.read(diagnosticsProvider.notifier).add('Lỗi lệnh bơm: $error');
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('Không gửi được lệnh: $error')));
      }
    } finally {
      if (mounted) setState(() => _sending = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final settings = ref.watch(currentSettingsProvider);
    return Scaffold(
      appBar: AppBar(title: const Text('Thiết bị')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            child: ListTile(
              leading: const CircleAvatar(child: Icon(Icons.memory)),
              title: Text(settings.deviceId),
              subtitle: Text('Nguồn điều khiển: ${settings.source.label}'),
            ),
          ),
          const SizedBox(height: 8),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(18),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Text(
                    'Relay máy bơm',
                    style: Theme.of(context).textTheme.titleLarge,
                  ),
                  const SizedBox(height: 8),
                  const Text(
                    'Lệnh ON tự tắt tối đa theo giới hạn an toàn trong firmware. '
                    'App gửi thời lượng 60 giây.',
                  ),
                  const SizedBox(height: 20),
                  Row(
                    children: [
                      Expanded(
                        child: FilledButton.icon(
                          onPressed: _sending ? null : () => _setPump(true),
                          icon: const Icon(Icons.water),
                          label: const Text('Bật bơm'),
                        ),
                      ),
                      const SizedBox(width: 12),
                      Expanded(
                        child: OutlinedButton.icon(
                          onPressed: _sending ? null : () => _setPump(false),
                          icon: const Icon(Icons.stop_circle_outlined),
                          label: const Text('Tắt bơm'),
                        ),
                      ),
                    ],
                  ),
                  if (_sending) ...[
                    const SizedBox(height: 16),
                    const LinearProgressIndicator(),
                  ],
                ],
              ),
            ),
          ),
          const Card(
            child: ListTile(
              leading: Icon(Icons.tune),
              title: Text('Cấu hình cảm biến'),
              subtitle: Text(
                'API server hiện chưa có endpoint config. Hiệu chuẩn LM393 vẫn thực hiện trong menuconfig firmware.',
              ),
            ),
          ),
        ],
      ),
    );
  }
}
