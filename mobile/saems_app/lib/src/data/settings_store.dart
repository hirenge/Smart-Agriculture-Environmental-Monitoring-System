import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../core/app_config.dart';
import '../models/data_source.dart';

class AppSettings {
  const AppSettings({
    required this.apiBaseUrl,
    required this.deviceId,
    required this.source,
    this.bleRemoteId,
  });

  final String apiBaseUrl;
  final String deviceId;
  final DataSourceType source;
  final String? bleRemoteId;

  AppSettings copyWith({
    String? apiBaseUrl,
    String? deviceId,
    DataSourceType? source,
    String? bleRemoteId,
  }) => AppSettings(
    apiBaseUrl: apiBaseUrl ?? this.apiBaseUrl,
    deviceId: deviceId ?? this.deviceId,
    source: source ?? this.source,
    bleRemoteId: bleRemoteId ?? this.bleRemoteId,
  );
}

class SettingsStore {
  static const _apiKey = 'api_base_url';
  static const _deviceKey = 'device_id';
  static const _sourceKey = 'data_source';
  static const _tokenKey = 'api_token';
  static const _bleKey = 'ble_remote_id';

  final FlutterSecureStorage _secureStorage = const FlutterSecureStorage();

  Future<AppSettings> load() async {
    final preferences = await SharedPreferences.getInstance();
    final sourceName = preferences.getString(_sourceKey);
    return AppSettings(
      apiBaseUrl: preferences.getString(_apiKey) ?? AppConfig.apiBaseUrl,
      deviceId: preferences.getString(_deviceKey) ?? AppConfig.defaultDeviceId,
      bleRemoteId: preferences.getString(_bleKey),
      source:
          DataSourceType.values
              .where((value) => value.name == sourceName)
              .firstOrNull ??
          (AppConfig.enableMockMode
              ? DataSourceType.mock
              : DataSourceType.server),
    );
  }

  Future<void> save(AppSettings settings) async {
    final preferences = await SharedPreferences.getInstance();
    await preferences.setString(_apiKey, settings.apiBaseUrl);
    await preferences.setString(_deviceKey, settings.deviceId);
    await preferences.setString(_sourceKey, settings.source.name);
    if (settings.bleRemoteId != null) {
      await preferences.setString(_bleKey, settings.bleRemoteId!);
    }
  }

  Future<String?> readToken() => _secureStorage.read(key: _tokenKey);

  Future<void> saveToken(String? token) => token == null || token.isEmpty
      ? _secureStorage.delete(key: _tokenKey)
      : _secureStorage.write(key: _tokenKey, value: token);
}
