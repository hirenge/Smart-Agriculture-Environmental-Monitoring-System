import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../data/direct_config_transport.dart';
import '../state/app_state.dart';

class ProvisioningScreen extends ConsumerStatefulWidget {
  const ProvisioningScreen({super.key});

  @override
  ConsumerState<ProvisioningScreen> createState() => _ProvisioningScreenState();
}

class _ProvisioningScreenState extends ConsumerState<ProvisioningScreen> {
  final _ssid = TextEditingController();
  final _wifiPassword = TextEditingController();
  final _mqttUri = TextEditingController();
  final _mqttUser = TextEditingController();
  final _mqttPassword = TextEditingController();
  final _interval = TextEditingController(text: '10');
  String _powerMode = 'direct';
  bool _busy = false;
  String _message = 'Đọc cấu hình để kiểm tra kết nối trước khi lưu.';

  @override
  void dispose() {
    for (final field in [
      _ssid,
      _wifiPassword,
      _mqttUri,
      _mqttUser,
      _mqttPassword,
      _interval,
    ]) {
      field.dispose();
    }
    super.dispose();
  }

  Future<void> _run(bool save) async {
    setState(() => _busy = true);
    try {
      final transport = ref.read(telemetryRepositoryProvider).transport;
      if (transport is! DirectConfigTransport) {
        throw StateError('Chọn BLE hoặc Wi-Fi AP để cấu hình thiết bị.');
      }
      await transport.connect();
      final direct = transport as DirectConfigTransport;
      if (save) {
        final interval = int.tryParse(_interval.text.trim());
        if (interval == null || interval < 1 || interval > 3600) {
          throw const FormatException('Chu kỳ gửi phải từ 1 đến 3600 giây.');
        }
        final config = <String, dynamic>{
          'telemetry_interval_s': interval,
          'power_mode': _powerMode,
        };
        if (_ssid.text.trim().isNotEmpty) {
          config['wifi_ssid'] = _ssid.text.trim();
          config['wifi_password'] = _wifiPassword.text;
        }
        if (_mqttUri.text.trim().isNotEmpty) {
          final uri = Uri.tryParse(_mqttUri.text.trim());
          if (uri == null ||
              !['mqtt', 'mqtts'].contains(uri.scheme) ||
              uri.host.isEmpty) {
            throw const FormatException(
              'Nhập URI mqtt:// hoặc mqtts:// hợp lệ.',
            );
          }
          config['mqtt_uri'] = _mqttUri.text.trim();
        }
        if (_mqttUser.text.trim().isNotEmpty) {
          config['mqtt_username'] = _mqttUser.text.trim();
          config['mqtt_password'] = _mqttPassword.text;
        }
        // Split credential groups to fit the BLE ATT long-write limit.
        if (config.containsKey('wifi_ssid')) {
          await direct.writeDeviceConfig({
            'wifi_ssid': config.remove('wifi_ssid'),
            'wifi_password': config.remove('wifi_password'),
          });
        }
        await direct.writeDeviceConfig(config);
        _wifiPassword.clear();
        _mqttPassword.clear();
        if (mounted) {
          setState(
            () => _message = 'Đã lưu vào ESP32. Chế độ năng lượng áp dụng ở cuối chu kỳ lấy mẫu; Wi-Fi/MQTT mới cần khởi động lại.',
          );
        }
      } else {
        final config = await direct.readDeviceConfig();
        _interval.text = '${config['telemetry_interval_s'] ?? 10}';
        final powerMode = config['power_mode']?.toString();
        if (powerMode == 'direct' ||
            powerMode == 'light' ||
            powerMode == 'battery') {
          _powerMode = powerMode!;
        }
        if (mounted) {
          setState(
            () => _message =
                'Thiết bị: ${config['device_id']}\n'
                'Chế độ năng lượng: $_powerMode\n'
                'Wi-Fi đã cấu hình: ${config['wifi_configured'] == true}\n'
                'MQTT đã cấu hình: ${config['mqtt_configured'] == true}',
          );
        }
      }
    } catch (error) {
      if (mounted) setState(() => _message = error.toString());
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Widget _field(
    TextEditingController controller,
    String label, {
    bool secret = false,
  }) => Padding(
    padding: const EdgeInsets.only(bottom: 12),
    child: TextField(
      controller: controller,
      obscureText: secret,
      autocorrect: !secret,
      enableSuggestions: !secret,
      decoration: InputDecoration(labelText: label),
    ),
  );

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: AppBar(title: const Text('Cấu hình ESP32')),
    body: ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text(_message),
        const SizedBox(height: 16),
        _field(_ssid, 'Wi-Fi SSID mới (để trống nếu giữ nguyên)'),
        _field(_wifiPassword, 'Mật khẩu Wi-Fi', secret: true),
        _field(_mqttUri, 'MQTT URI, ví dụ mqtts://server:8443'),
        _field(_mqttUser, 'MQTT username'),
        _field(_mqttPassword, 'MQTT password', secret: true),
        _field(_interval, 'Chu kỳ telemetry (giây)'),
        Padding(
          padding: const EdgeInsets.only(bottom: 12),
          child: DropdownButtonFormField<String>(
            key: ValueKey(_powerMode),
            initialValue: _powerMode,
            decoration: const InputDecoration(labelText: 'Chế độ năng lượng'),
            items: const [
              DropdownMenuItem(
                value: 'direct',
                child: Text('Direct – kết nối liên tục'),
              ),
              DropdownMenuItem(
                value: 'light',
                child: Text('Light Sleep giữa các chu kỳ'),
              ),
              DropdownMenuItem(
                value: 'battery',
                child: Text('Battery – Deep Sleep'),
              ),
            ],
            onChanged: _busy
                ? null
                : (value) => setState(() => _powerMode = value ?? 'direct'),
          ),
        ),
        OutlinedButton(
          onPressed: _busy ? null : () => _run(false),
          child: const Text('Đọc cấu hình thiết bị'),
        ),
        FilledButton(
          onPressed: _busy ? null : () => _run(true),
          child: const Text('Lưu cấu hình vào ESP32'),
        ),
      ],
    ),
  );
}
