import 'package:flutter_test/flutter_test.dart';
import 'package:saems_app/src/models/telemetry.dart';

void main() {
  group('Telemetry parser', () {
    test('parses a complete schema v1 payload', () {
      final telemetry = Telemetry.fromJson({
        'schema_version': 1,
        'device_id': 'farm-01',
        'boot_id': 'abc',
        'seq': 12,
        'timestamp': '2026-10-02T08:00:00Z',
        'received_at': '2026-10-02T08:00:01Z',
        'data_mode': 'real',
        'sensor_health': 'ok',
        'temperature_c': 27.4,
        'humidity_pct': 70,
        'soil_moisture_pct': 51.2,
        'light_lux': 812,
        'battery_pct': 90,
        'pump_state': 'off',
      });

      expect(telemetry.deviceId, 'farm-01');
      expect(telemetry.sequence, 12);
      expect(telemetry.dataMode, DataMode.real);
      expect(telemetry.lightLux, 812);
    });

    test('accepts missing and null sensor fields from current server', () {
      final telemetry = Telemetry.fromJson({
        'device_id': 'farm-01',
        'received_at': '2026-10-02T08:00:01Z',
        'temperature_c': null,
        'humidity_pct': null,
        'light_value': 123.5,
        'unknown_future_field': true,
      });

      expect(telemetry.temperatureC, isNull);
      expect(telemetry.humidityPct, isNull);
      expect(telemetry.lightLux, 123.5);
      expect(telemetry.dataMode, DataMode.unknown);
    });

    test('rejects invalid percentages', () {
      expect(
        () => Telemetry.fromJson({'device_id': 'farm-01', 'humidity_pct': 120}),
        throwsFormatException,
      );
    });

    test('detects stale telemetry', () {
      final telemetry = Telemetry.fromJson({
        'device_id': 'farm-01',
        'timestamp': '2026-10-02T08:00:00Z',
      });
      expect(telemetry.isStale(DateTime.utc(2026, 10, 2, 8, 16)), isTrue);
    });
  });
}
