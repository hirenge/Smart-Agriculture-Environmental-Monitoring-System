import 'dart:convert';

enum DataMode {
  simulated,
  real,
  unknown;

  static DataMode parse(Object? value) => switch (value) {
    'simulated' => DataMode.simulated,
    'real' => DataMode.real,
    _ => DataMode.unknown,
  };

  String get label => switch (this) {
    DataMode.simulated => 'Dữ liệu giả lập',
    DataMode.real => 'Dữ liệu cảm biến thật',
    DataMode.unknown => 'Chưa xác nhận nguồn dữ liệu',
  };
}

enum SensorHealth {
  ok,
  degraded,
  unavailable,
  unknown;

  static SensorHealth parse(Object? value) => switch (value) {
    'ok' => SensorHealth.ok,
    'degraded' => SensorHealth.degraded,
    'unavailable' => SensorHealth.unavailable,
    _ => SensorHealth.unknown,
  };
}

class Telemetry {
  const Telemetry({
    required this.deviceId,
    required this.timestamp,
    required this.dataMode,
    required this.sensorHealth,
    this.schemaVersion = 1,
    this.bootId,
    this.sequence,
    this.receivedAt,
    this.temperatureC,
    this.humidityPct,
    this.soilMoisturePct,
    this.lightLux,
    this.batteryPct,
    this.pumpState,
  });

  final int schemaVersion;
  final String deviceId;
  final String? bootId;
  final int? sequence;
  final DateTime timestamp;
  final DateTime? receivedAt;
  final DataMode dataMode;
  final SensorHealth sensorHealth;
  final double? temperatureC;
  final double? humidityPct;
  final double? soilMoisturePct;
  final double? lightLux;
  final double? batteryPct;
  final String? pumpState;

  bool isStale([DateTime? now]) {
    final reference = now ?? DateTime.now().toUtc();
    return reference.difference(receivedAt ?? timestamp).abs() >
        const Duration(minutes: 15);
  }

  factory Telemetry.fromJson(Map<String, dynamic> json) {
    final deviceId = _string(json['device_id']);
    if (deviceId == null || deviceId.isEmpty) {
      throw const FormatException('device_id is required');
    }

    final timestamp =
        _date(json['timestamp']) ??
        _date(json['received_at']) ??
        DateTime.now().toUtc();
    return Telemetry(
      schemaVersion: _integer(json['schema_version']) ?? 1,
      deviceId: deviceId,
      bootId: _string(json['boot_id']),
      sequence: _integer(json['seq']),
      timestamp: timestamp,
      receivedAt: _date(json['received_at']),
      dataMode: DataMode.parse(json['data_mode']),
      sensorHealth: SensorHealth.parse(json['sensor_health']),
      temperatureC: _number(json['temperature_c']),
      humidityPct: _percentage(json['humidity_pct']),
      soilMoisturePct: _percentage(json['soil_moisture_pct']),
      lightLux: _number(
        json['light_lux'] ?? json['light_value'] ?? json['light'],
      ),
      batteryPct: _percentage(json['battery_pct']),
      pumpState: _string(json['pump_state']),
    );
  }

  Map<String, dynamic> toJson() => {
    'schema_version': schemaVersion,
    'device_id': deviceId,
    'boot_id': bootId,
    'seq': sequence,
    'timestamp': timestamp.toUtc().toIso8601String(),
    'received_at': receivedAt?.toUtc().toIso8601String(),
    'data_mode': dataMode.name,
    'sensor_health': sensorHealth.name,
    'temperature_c': temperatureC,
    'humidity_pct': humidityPct,
    'soil_moisture_pct': soilMoisturePct,
    'light_lux': lightLux,
    'battery_pct': batteryPct,
    'pump_state': pumpState,
  };

  String encode() => jsonEncode(toJson());

  static Telemetry decode(String value) =>
      Telemetry.fromJson(jsonDecode(value) as Map<String, dynamic>);
}

double? _number(Object? value) {
  if (value == null) return null;
  if (value is num) return value.toDouble();
  return double.tryParse(value.toString());
}

double? _percentage(Object? value) {
  final parsed = _number(value);
  if (parsed == null) return null;
  if (parsed < 0 || parsed > 100) {
    throw FormatException('Percentage outside 0..100: $parsed');
  }
  return parsed;
}

int? _integer(Object? value) {
  if (value is int) return value;
  return int.tryParse(value?.toString() ?? '');
}

String? _string(Object? value) => value?.toString();

DateTime? _date(Object? value) {
  if (value == null || value.toString().isEmpty) return null;
  return DateTime.tryParse(value.toString())?.toUtc();
}
