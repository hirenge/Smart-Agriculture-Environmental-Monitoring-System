import '../data/device_transport.dart';
import '../data/telemetry_cache.dart';
import '../models/data_source.dart';
import '../models/device_models.dart';
import '../models/telemetry.dart';

class TelemetryRepository {
  TelemetryRepository({
    required this.transport,
    required this.source,
    required this.cache,
  });

  final DeviceTransport transport;
  final DataSourceType source;
  final TelemetryCache cache;

  Stream<Sourced<Telemetry>> watch(String deviceId) async* {
    while (true) {
      try {
        await for (final telemetry in transport.watchTelemetry(deviceId)) {
          await cache.save(telemetry);
          yield Sourced(value: telemetry, source: source);
        }
      } catch (_) {
        final cached = await cache.readLatest(deviceId);
        if (cached != null) {
          yield Sourced(
            value: cached,
            source: DataSourceType.cache,
            fromCache: true,
          );
        }
      }
      await Future<void>.delayed(const Duration(seconds: 5));
    }
  }

  Future<List<Telemetry>> history(String deviceId, {int limit = 100}) =>
      transport.readHistory(deviceId, limit: limit);

  Future<DeviceStatus> status(String deviceId) =>
      transport.readStatus(deviceId);

  Future<CommandResult> setPump(
    String deviceId, {
    required bool enabled,
    int durationSeconds = 60,
  }) => transport.setPump(
    deviceId,
    enabled: enabled,
    durationSeconds: durationSeconds,
  );
}

Sourced<Telemetry>? selectPreferredReading(
  Iterable<Sourced<Telemetry>> readings,
) {
  final available = readings.toList(growable: false);
  if (available.isEmpty) return null;
  available.sort((left, right) {
    final sourceComparison = _priority(right.source)
        .compareTo(_priority(left.source));
    if (sourceComparison != 0) return sourceComparison;
    return right.value.timestamp.compareTo(left.value.timestamp);
  });
  return available.first;
}

int _priority(DataSourceType source) => switch (source) {
  DataSourceType.ble || DataSourceType.wifiAp => 4,
  DataSourceType.server => 3,
  DataSourceType.mock => 2,
  DataSourceType.cache => 1,
};
