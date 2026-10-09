import '../models/device_models.dart';
import '../models/telemetry.dart';

abstract interface class DeviceTransport {
  Future<void> connect();
  Future<void> disconnect();
  Stream<Telemetry> watchTelemetry(String deviceId);
  Future<Telemetry> readLatest(String deviceId);
  Future<List<Telemetry>> readHistory(String deviceId, {int limit = 100});
  Future<DeviceStatus> readStatus(String deviceId);
  Future<DeviceConfig?> readConfig(String deviceId);
  Future<CommandResult> setPump(
    String deviceId, {
    required bool enabled,
    int durationSeconds = 60,
  });
}
