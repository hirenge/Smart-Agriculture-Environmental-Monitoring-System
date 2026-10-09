import 'dart:async';
import 'dart:convert';

import '../../models/device_models.dart';
import '../../models/telemetry.dart';
import '../device_transport.dart';
import '../direct_config_transport.dart';
import 'ble_frame_reassembler.dart';
import 'flutter_ble_adapter.dart';

abstract interface class BleAdapter {
  Future<void> connect();
  Future<void> disconnect();
  Stream<List<int>> get telemetryFrames;
  Future<void> writeCommand(Map<String, dynamic> command);
}

class BleDeviceTransport implements DeviceTransport, DirectConfigTransport {
  BleDeviceTransport(this._adapter);

  final BleAdapter _adapter;
  Telemetry? _latest;

  @override
  Future<void> connect() => _adapter.connect();

  @override
  Future<void> disconnect() => _adapter.disconnect();

  @override
  Stream<Telemetry> watchTelemetry(String deviceId) async* {
    await connect();
    final reassembler = BleFrameReassembler();
    if (_adapter is FlutterBleAdapter) {
      final json = await _adapter.readTelemetry();
      if (json['device_id'] != null) {
        _latest = Telemetry.fromJson(json);
        yield _latest!;
      }
    }
    await for (final frame in _adapter.telemetryFrames) {
      final payload = reassembler.addFrame(frame);
      if (payload == null) continue;
      _latest = Telemetry.fromJson(jsonDecode(payload) as Map<String, dynamic>);
      yield _latest!;
    }
  }

  @override
  Future<Telemetry> readLatest(String deviceId) async {
    if (_adapter is FlutterBleAdapter) {
      _latest = Telemetry.fromJson(await _adapter.readTelemetry());
      return _latest!;
    }
    if (_latest != null) return _latest!;
    return watchTelemetry(deviceId).first.timeout(const Duration(seconds: 8));
  }

  @override
  Future<List<Telemetry>> readHistory(
    String deviceId, {
    int limit = 100,
  }) async => _latest == null ? const [] : [_latest!];

  @override
  Future<DeviceStatus> readStatus(String deviceId) async {
    final latest = await readLatest(deviceId);
    return DeviceStatus(
      deviceId: deviceId,
      online: true,
      lastSeen: latest.receivedAt ?? latest.timestamp,
      telemetry: latest,
    );
  }

  @override
  Future<DeviceConfig?> readConfig(String deviceId) async =>
      DeviceConfig.fromJson(await readDeviceConfig());

  DirectConfigTransport get _configuration => _adapter as DirectConfigTransport;

  @override
  Future<Map<String, dynamic>> readDeviceInfo() =>
      _configuration.readDeviceInfo();

  @override
  Future<Map<String, dynamic>> readDeviceConfig() =>
      _configuration.readDeviceConfig();

  @override
  Future<void> writeDeviceConfig(Map<String, dynamic> config) =>
      _configuration.writeDeviceConfig(config);

  @override
  Future<CommandResult> setPump(
    String deviceId, {
    required bool enabled,
    int durationSeconds = 60,
  }) async {
    await _adapter.writeCommand({
      'command': 'pump',
      'state': enabled ? 'on' : 'off',
      'duration_s': durationSeconds,
    });
    return CommandResult(status: 'sent', deviceId: deviceId);
  }
}

class FakeBleAdapter implements BleAdapter {
  final StreamController<List<int>> _controller = StreamController.broadcast();
  bool connected = false;
  Map<String, dynamic>? lastCommand;

  @override
  Stream<List<int>> get telemetryFrames => _controller.stream;

  @override
  Future<void> connect() async => connected = true;

  @override
  Future<void> disconnect() async => connected = false;

  @override
  Future<void> writeCommand(Map<String, dynamic> command) async {
    lastCommand = command;
  }

  void emitJson(
    Map<String, dynamic> json, {
    int chunkSize = 14,
    int messageId = 1,
  }) {
    final bytes = utf8.encode(jsonEncode(json));
    final total = (bytes.length / chunkSize).ceil();
    for (var index = 0; index < total; index++) {
      final start = index * chunkSize;
      final end = (start + chunkSize).clamp(0, bytes.length);
      _controller.add([
        messageId >> 8,
        messageId & 0xff,
        index,
        total,
        ...bytes.sublist(start, end),
      ]);
    }
  }

  Future<void> dispose() => _controller.close();
}
