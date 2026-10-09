class AppConfig {
  const AppConfig._();

  static const apiBaseUrl = String.fromEnvironment(
    'API_BASE_URL',
    defaultValue: 'http://192.168.1.4:3000',
  );
  static const defaultDeviceId = String.fromEnvironment(
    'DEFAULT_DEVICE_ID',
    defaultValue: 'farm-01',
  );
  static const enableMockMode = bool.fromEnvironment(
    'ENABLE_MOCK_MODE',
    defaultValue: true,
  );
  static const wifiApBaseUrl = 'http://192.168.4.1';
  static const pollInterval = Duration(seconds: 10);
  static const staleAfter = Duration(minutes: 15);
}
