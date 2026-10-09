enum DataSourceType {
  mock,
  server,
  wifiAp,
  ble,
  cache;

  String get label => switch (this) {
    DataSourceType.mock => 'Mô phỏng',
    DataSourceType.server => 'Server',
    DataSourceType.wifiAp => 'Wi-Fi trực tiếp',
    DataSourceType.ble => 'BLE trực tiếp',
    DataSourceType.cache => 'Bộ nhớ cục bộ',
  };

  bool get isDirect =>
      this == DataSourceType.wifiAp || this == DataSourceType.ble;
}

class Sourced<T> {
  const Sourced({
    required this.value,
    required this.source,
    this.fromCache = false,
  });

  final T value;
  final DataSourceType source;
  final bool fromCache;
}
