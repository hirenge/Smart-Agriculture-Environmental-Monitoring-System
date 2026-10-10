import 'package:flutter_test/flutter_test.dart';
import 'package:saems_app/src/models/data_source.dart';
import 'package:saems_app/src/models/telemetry.dart';
import 'package:saems_app/src/repositories/telemetry_repository.dart';

Telemetry sample(DateTime timestamp) => Telemetry(
  deviceId: 'farm-01',
  timestamp: timestamp,
  dataMode: DataMode.real,
  sensorHealth: SensorHealth.ok,
);

void main() {
  test('direct source wins over newer server and cache values', () {
    final selected = selectPreferredReading([
      Sourced(
        value: sample(DateTime.utc(2026, 10, 2, 10)),
        source: DataSourceType.server,
      ),
      Sourced(
        value: sample(DateTime.utc(2026, 10, 2, 9)),
        source: DataSourceType.ble,
      ),
      Sourced(
        value: sample(DateTime.utc(2026, 10, 2, 11)),
        source: DataSourceType.cache,
      ),
    ]);

    expect(selected?.source, DataSourceType.ble);
  });

  test('newest reading wins within the same source', () {
    final selected = selectPreferredReading([
      Sourced(
        value: sample(DateTime.utc(2026, 10, 2, 8)),
        source: DataSourceType.server,
      ),
      Sourced(
        value: sample(DateTime.utc(2026, 10, 2, 9)),
        source: DataSourceType.server,
      ),
    ]);

    expect(selected?.value.timestamp, DateTime.utc(2026, 10, 2, 9));
  });
}
