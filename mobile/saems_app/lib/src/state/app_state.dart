import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../core/app_config.dart';
import '../data/ble/ble_device_transport.dart';
import '../data/ble/flutter_ble_adapter.dart';
import '../data/mock_device_transport.dart';
import '../data/server_api_transport.dart';
import '../data/settings_store.dart';
import '../data/telemetry_cache.dart';
import '../data/wifi_ap_device_transport.dart';
import '../models/data_source.dart';
import '../models/telemetry.dart';
import '../repositories/telemetry_repository.dart';

final settingsStoreProvider = Provider<SettingsStore>((ref) => SettingsStore());
final telemetryCacheProvider = Provider<TelemetryCache>(
  (ref) => TelemetryCache(),
);

final bleAdapterProvider = Provider<FlutterBleAdapter>((ref) {
  final adapter = FlutterBleAdapter(
    ref.watch(currentSettingsProvider).bleRemoteId,
  );
  ref.onDispose(adapter.dispose);
  return adapter;
});

final apiTokenProvider = FutureProvider<String?>(
  (ref) => ref.watch(settingsStoreProvider).readToken(),
);

class AppSettingsNotifier extends AsyncNotifier<AppSettings> {
  @override
  Future<AppSettings> build() => ref.read(settingsStoreProvider).load();

  Future<void> save(AppSettings value) async {
    state = AsyncData(value);
    await ref.read(settingsStoreProvider).save(value);
  }
}

final appSettingsProvider =
    AsyncNotifierProvider<AppSettingsNotifier, AppSettings>(
      AppSettingsNotifier.new,
    );

final currentSettingsProvider = Provider<AppSettings>((ref) {
  return ref.watch(appSettingsProvider).value ??
      const AppSettings(
        apiBaseUrl: AppConfig.apiBaseUrl,
        deviceId: AppConfig.defaultDeviceId,
        source: AppConfig.enableMockMode
            ? DataSourceType.mock
            : DataSourceType.server,
      );
});

final telemetryRepositoryProvider = Provider<TelemetryRepository>((ref) {
  final settings = ref.watch(currentSettingsProvider);
  final transport = switch (settings.source) {
    DataSourceType.server => ServerApiTransport(
      baseUrl: settings.apiBaseUrl,
      token: ref.watch(apiTokenProvider).value,
    ),
    DataSourceType.wifiAp => WifiApDeviceTransport(),
    DataSourceType.mock => MockDeviceTransport(),
    DataSourceType.ble => BleDeviceTransport(ref.watch(bleAdapterProvider)),
    DataSourceType.cache => MockDeviceTransport(),
  };
  ref.onDispose(transport.disconnect);
  return TelemetryRepository(
    transport: transport,
    source: settings.source,
    cache: ref.watch(telemetryCacheProvider),
  );
});

final telemetryProvider = StreamProvider<Sourced<Telemetry>>((ref) {
  final settings = ref.watch(currentSettingsProvider);
  return ref.watch(telemetryRepositoryProvider).watch(settings.deviceId);
});

final historyProvider = FutureProvider<List<Telemetry>>((ref) {
  final settings = ref.watch(currentSettingsProvider);
  return ref
      .watch(telemetryRepositoryProvider)
      .history(settings.deviceId, limit: 96);
});

class DiagnosticEvent {
  const DiagnosticEvent(this.timestamp, this.message);

  final DateTime timestamp;
  final String message;
}

class DiagnosticsNotifier extends Notifier<List<DiagnosticEvent>> {
  @override
  List<DiagnosticEvent> build() => [];

  void add(String message) {
    state = [
      DiagnosticEvent(DateTime.now(), message),
      ...state,
    ].take(50).toList(growable: false);
  }
}

final diagnosticsProvider =
    NotifierProvider<DiagnosticsNotifier, List<DiagnosticEvent>>(
      DiagnosticsNotifier.new,
    );
