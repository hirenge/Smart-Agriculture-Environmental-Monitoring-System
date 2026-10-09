import 'dart:async';
import 'dart:math';

import '../core/app_config.dart';
import '../models/device_models.dart';
import '../models/telemetry.dart';
import 'device_transport.dart';

class MockDeviceTransport implements DeviceTransport {
  MockDeviceTransport({Random? random}) : _random = random ?? Random();

  final Random _random;
  int _sequence = 0;
  bool _pumpEnabled = false;

  Telemetry _sample(String deviceId) {
    _sequence++;
    final now = DateTime.now().toUtc();
    return Telemetry(
      schemaVersion: 1,
      deviceId: deviceId,
      bootId: 'mock-session',
      sequence: _sequence,
      timestamp: now,
      receivedAt: now,
      dataMode: DataMode.simulated,
      sensorHealth: SensorHealth.ok,
      temperatureC: 25 + _random.nextDouble() * 5,
      humidityPct: 58 + _random.nextDouble() * 18,
      soilMoisturePct: 35 + _random.nextDouble() * 35,
      lightLux: 250 + _random.nextDouble() * 900,
      batteryPct: 82,
      pumpState: _pumpEnabled ? 'on' : 'off',
    );
  }

  @override
  Future<void> connect() async {}

  @override
  Future<void> disconnect() async {}

  @override
  Future<Telemetry> readLatest(String deviceId) async => _sample(deviceId);

  @override
  Future<List<Telemetry>> readHistory(
    String deviceId, {
    int limit = 100,
  }) async {
    final now = DateTime.now().toUtc();
    return List.generate(min(limit, 48), (index) {
      final sample = _sample(deviceId);
      final timestamp = now.subtract(Duration(minutes: (47 - index) * 10));
      return Telemetry(
        deviceId: sample.deviceId,
        timestamp: timestamp,
        receivedAt: timestamp,
        dataMode: sample.dataMode,
        sensorHealth: sample.sensorHealth,
        sequence: sample.sequence,
        temperatureC: sample.temperatureC,
        humidityPct: sample.humidityPct,
        soilMoisturePct: sample.soilMoisturePct,
        lightLux: sample.lightLux,
        batteryPct: sample.batteryPct,
        pumpState: sample.pumpState,
      );
    });
  }

  @override
  Future<DeviceStatus> readStatus(String deviceId) async {
    final telemetry = _sample(deviceId);
    return DeviceStatus(
      deviceId: deviceId,
      online: true,
      lastSeen: telemetry.receivedAt,
      telemetry: telemetry,
    );
  }

  @override
  Future<DeviceConfig> readConfig(String deviceId) async =>
      DeviceConfig(deviceId: deviceId);

  @override
  Future<CommandResult> setPump(
    String deviceId, {
    required bool enabled,
    int durationSeconds = 60,
  }) async {
    _pumpEnabled = enabled;
    return CommandResult(
      status: 'applied',
      deviceId: deviceId,
      requestId: 'mock-$_sequence',
    );
  }

  @override
  Stream<Telemetry> watchTelemetry(String deviceId) async* {
    yield _sample(deviceId);
    yield* Stream<Telemetry>.periodic(
      AppConfig.pollInterval,
      (_) => _sample(deviceId),
    );
  }
}
