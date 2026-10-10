import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/data_source.dart';
import '../data/settings_store.dart';
import '../state/app_state.dart';
import 'diagnostics_screen.dart';

class SettingsScreen extends ConsumerStatefulWidget {
  const SettingsScreen({super.key});

  @override
  ConsumerState<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends ConsumerState<SettingsScreen> {
  final _formKey = GlobalKey<FormState>();
  late final TextEditingController _apiController;
  late final TextEditingController _deviceController;
  final _tokenController = TextEditingController();
  bool _tokenChanged = false;
  DataSourceType? _source;
  bool _initialized = false;
  bool _dirty = false;
  AppSettings? _observedSettings;

  @override
  void initState() {
    super.initState();
    _apiController = TextEditingController();
    _deviceController = TextEditingController();
  }

  @override
  void dispose() {
    _apiController.dispose();
    _deviceController.dispose();
    _tokenController.dispose();
    super.dispose();
  }

  Future<void> _save() async {
    if (!_formKey.currentState!.validate()) return;
    final settings = ref
        .read(currentSettingsProvider)
        .copyWith(
          apiBaseUrl: _apiController.text.trim(),
          deviceId: _deviceController.text.trim(),
          source: _source ?? ref.read(currentSettingsProvider).source,
        );
    if (_tokenChanged) {
      await ref
          .read(settingsStoreProvider)
          .saveToken(_tokenController.text.trim());
      ref.invalidate(apiTokenProvider);
      _tokenController.clear();
      _tokenChanged = false;
    }
    _dirty = false;
    await ref.read(appSettingsProvider.notifier).save(settings);
    ref.invalidate(telemetryProvider);
    ref.invalidate(historyProvider);
    ref.read(diagnosticsProvider.notifier).add('Đã lưu cài đặt kết nối');
    if (mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('Đã lưu cài đặt')));
    }
  }

  @override
  Widget build(BuildContext context) {
    final loaded = ref.watch(appSettingsProvider).value;
    if (loaded == null) {
      return const Scaffold(body: Center(child: CircularProgressIndicator()));
    }
    final current = ref.watch(currentSettingsProvider);
    if (!_initialized || (!_dirty && _observedSettings != current)) {
      _initialized = true;
      _observedSettings = current;
      _apiController.text = current.apiBaseUrl;
      _deviceController.text = current.deviceId;
      _source = current.source;
    }
    return Scaffold(
      appBar: AppBar(title: const Text('Cài đặt')),
      body: Form(
        key: _formKey,
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            TextFormField(
              controller: _apiController,
              onChanged: (_) => _dirty = true,
              keyboardType: TextInputType.url,
              decoration: const InputDecoration(
                labelText: 'Server API URL',
                prefixIcon: Icon(Icons.dns_outlined),
                helperText: 'Dùng HTTPS khi kết nối server qua Internet',
              ),
              validator: (value) {
                final uri = Uri.tryParse(value?.trim() ?? '');
                return uri == null ||
                        !['https', 'http'].contains(uri.scheme) ||
                        uri.host.isEmpty
                    ? 'Nhập URL hợp lệ'
                    : null;
              },
            ),
            const SizedBox(height: 16),
            TextFormField(
              controller: _deviceController,
              onChanged: (_) => _dirty = true,
              decoration: const InputDecoration(
                labelText: 'Device ID',
                prefixIcon: Icon(Icons.memory),
              ),
              validator: (value) => value == null || value.trim().isEmpty
                  ? 'Device ID không được để trống'
                  : null,
            ),
            const SizedBox(height: 16),
            TextFormField(
              controller: _tokenController,
              obscureText: true,
              autocorrect: false,
              enableSuggestions: false,
              onChanged: (_) {
                _tokenChanged = true;
                _dirty = true;
              },
              decoration: const InputDecoration(
                labelText: 'API access token',
                helperText: 'Để nguyên ô trống để giữ token đã lưu',
              ),
            ),
            const SizedBox(height: 16),
            DropdownButtonFormField<DataSourceType>(
              key: ValueKey(_observedSettings?.source),
              initialValue: _source,
              decoration: const InputDecoration(
                labelText: 'Nguồn dữ liệu',
                prefixIcon: Icon(Icons.hub_outlined),
              ),
              items:
                  const [
                        DataSourceType.mock,
                        DataSourceType.server,
                        DataSourceType.wifiAp,
                        DataSourceType.ble,
                      ]
                      .map(
                        (source) => DropdownMenuItem(
                          value: source,
                          child: Text(source.label),
                        ),
                      )
                      .toList(),
              onChanged: (value) => setState(() {
                _source = value;
                _dirty = true;
              }),
            ),
            const SizedBox(height: 20),
            FilledButton.icon(
              onPressed: _save,
              icon: const Icon(Icons.save_outlined),
              label: const Text('Lưu và kết nối lại'),
            ),
            const SizedBox(height: 20),
            Card(
              child: ListTile(
                leading: const Icon(Icons.monitor_heart_outlined),
                title: const Text('Chẩn đoán'),
                subtitle: const Text(
                  'Xem cấu hình không nhạy cảm và nhật ký phiên',
                ),
                trailing: const Icon(Icons.chevron_right),
                onTap: () => Navigator.of(context).push<void>(
                  MaterialPageRoute(builder: (_) => const DiagnosticsScreen()),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
