import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../data/ble/ble_contract.dart';
import '../data/ble/flutter_ble_adapter.dart';
import '../models/data_source.dart';
import '../state/app_state.dart';
import 'provisioning_screen.dart';

class NearbyScreen extends ConsumerStatefulWidget {
  const NearbyScreen({super.key});

  @override
  ConsumerState<NearbyScreen> createState() => _NearbyScreenState();
}

class _NearbyScreenState extends ConsumerState<NearbyScreen> {
  String _permissionState = 'Chưa quét';
  List<ScanResult> _results = [];
  StreamSubscription<List<ScanResult>>? _scan;
  bool _busy = false;

  @override
  void dispose() {
    _scan?.cancel();
    super.dispose();
  }

  Future<void> _prepareBle() async {
    setState(() {
      _busy = true;
      _permissionState = 'Đang quét ESP32...';
    });
    try {
      await FlutterBleAdapter.prepareBluetooth();
      await _scan?.cancel();
      _scan = FlutterBluePlus.onScanResults.listen(
        (results) {
          if (mounted) setState(() => _results = results);
        },
        onError: (Object error) {
          if (mounted) setState(() => _permissionState = error.toString());
        },
      );
      await FlutterBluePlus.startScan(
        withServices: [Guid(BleContract.service)],
        timeout: const Duration(seconds: 8),
      );
      await FlutterBluePlus.isScanning.where((value) => !value).first;
      if (mounted) {
        setState(
          () => _permissionState = 'Tìm thấy ${_results.length} thiết bị',
        );
      }
    } catch (error) {
      if (mounted) setState(() => _permissionState = error.toString());
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _connect(ScanResult result) async {
    setState(() {
      _busy = true;
      _permissionState = 'Đang kết nối...';
    });
    try {
      await FlutterBluePlus.stopScan();
      final adapter = FlutterBleAdapter(result.device.remoteId.str);
      Map<String, dynamic> info;
      try {
        info = await adapter.readDeviceInfo();
      } finally {
        await adapter.dispose();
      }
      final current = ref.read(currentSettingsProvider);
      await ref
          .read(appSettingsProvider.notifier)
          .save(
            current.copyWith(
              source: DataSourceType.ble,
              deviceId: info['device_id']?.toString() ?? current.deviceId,
              bleRemoteId: result.device.remoteId.str,
            ),
          );
      await ref.read(bleAdapterProvider).connect();
      if (mounted) {
        setState(() => _permissionState = 'Đã kết nối ${info['device_id']}');
      }
    } catch (error) {
      if (mounted) setState(() => _permissionState = error.toString());
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _select(DataSourceType source) async {
    final current = ref.read(currentSettingsProvider);
    final next = current.copyWith(source: source);
    await ref.read(appSettingsProvider.notifier).save(next);
    ref
        .read(diagnosticsProvider.notifier)
        .add('Đổi nguồn dữ liệu sang ${source.label}');
  }

  @override
  Widget build(BuildContext context) {
    final settings = ref.watch(currentSettingsProvider);
    return Scaffold(
      appBar: AppBar(title: const Text('Kết nối trực tiếp')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            child: Padding(
              padding: const EdgeInsets.all(18),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const ListTile(
                    contentPadding: EdgeInsets.zero,
                    leading: CircleAvatar(
                      child: Icon(Icons.bluetooth_searching),
                    ),
                    title: Text('Bluetooth Low Energy'),
                    subtitle: Text('Kết nối gần ESP32-S3, không cần Internet'),
                  ),
                  Text(_permissionState),
                  const SizedBox(height: 8),
                  Text(
                    'Service: ${BleContract.service}',
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                  const SizedBox(height: 12),
                  FilledButton.icon(
                    onPressed: _busy ? null : _prepareBle,
                    icon: const Icon(Icons.radar),
                    label: const Text('Quét ESP32'),
                  ),
                  for (final result in _results)
                    ListTile(
                      title: Text(
                        result.advertisementData.advName.isEmpty
                            ? result.device.remoteId.str
                            : result.advertisementData.advName,
                      ),
                      subtitle: Text('${result.rssi} dBm'),
                      trailing: const Icon(Icons.link),
                      onTap: _busy ? null : () => _connect(result),
                    ),
                ],
              ),
            ),
          ),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(18),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const ListTile(
                    contentPadding: EdgeInsets.zero,
                    leading: CircleAvatar(child: Icon(Icons.wifi_tethering)),
                    title: Text('Wi-Fi Access Point'),
                    subtitle: Text('Kết nối điện thoại vào Wi-Fi của ESP32'),
                  ),
                  const Text('Địa chỉ mặc định: http://192.168.4.1'),
                  const SizedBox(height: 12),
                  FilledButton.tonalIcon(
                    onPressed: () => _select(DataSourceType.wifiAp),
                    icon: const Icon(Icons.link),
                    label: Text(
                      settings.source == DataSourceType.wifiAp
                          ? 'Đang dùng Wi-Fi trực tiếp'
                          : 'Dùng Wi-Fi trực tiếp',
                    ),
                  ),
                ],
              ),
            ),
          ),
          if (settings.source.isDirect)
            FilledButton.tonalIcon(
              onPressed: () => Navigator.of(context).push<void>(
                MaterialPageRoute(builder: (_) => const ProvisioningScreen()),
              ),
              icon: const Icon(Icons.settings_input_antenna),
              label: const Text('Cấu hình Wi-Fi / MQTT cho ESP32'),
            ),
        ],
      ),
    );
  }
}
