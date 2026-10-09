import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../../core/app_exception.dart';
import '../direct_config_transport.dart';
import 'ble_contract.dart';
import 'ble_device_transport.dart';

class FlutterBleAdapter implements BleAdapter, DirectConfigTransport {
  FlutterBleAdapter(this.remoteId);

  final String? remoteId;
  final _frames = StreamController<List<int>>.broadcast();
  BluetoothDevice? _device;
  BluetoothCharacteristic? _telemetry;
  BluetoothCharacteristic? _info;
  BluetoothCharacteristic? _config;
  BluetoothCharacteristic? _command;
  StreamSubscription<List<int>>? _values;
  StreamSubscription<BluetoothConnectionState>? _connection;
  Future<void>? _connecting;
  bool _ready = false;
  bool _closing = false;
  bool _disposed = false;
  Future<void>? _disposing;

  void _ensureActive() {
    if (_disposed) throw StateError('BLE adapter đã được giải phóng');
  }

  void _reportError(Object error) {
    if (!_closing && !_disposed && !_frames.isClosed && _frames.hasListener) {
      _frames.addError(error);
    }
  }

  static const _permissions = MethodChannel('saems/permissions');

  static Future<void> prepareBluetooth() async {
    if (Platform.isAndroid) {
      final allowed = await _permissions.invokeMethod<bool>('requestBle');
      if (allowed != true) {
        throw const AppException(
          AppErrorKind.unauthorized,
          'Cần cấp quyền Bluetooth/Thiết bị ở gần để kết nối ESP32.',
        );
      }
    }
    if (!await FlutterBluePlus.isSupported) {
      throw const AppException(
        AppErrorKind.unavailable,
        'Thiết bị này không hỗ trợ Bluetooth Low Energy.',
      );
    }
    final state = await FlutterBluePlus.adapterState
        .where((state) => state != BluetoothAdapterState.unknown)
        .first
        .timeout(const Duration(seconds: 8));
    if (state != BluetoothAdapterState.on) {
      throw const AppException(
        AppErrorKind.unavailable,
        'Hãy bật Bluetooth rồi quét lại.',
      );
    }
  }

  @override
  Stream<List<int>> get telemetryFrames => _frames.stream;

  @override
  Future<void> connect() async {
    _ensureActive();
    if (_ready && _device?.isConnected == true) return;
    if (_connecting != null) return _connecting!;
    final operation = _open();
    _connecting = operation;
    try {
      await operation;
    } finally {
      _connecting = null;
    }
  }

  Future<void> _open() async {
    if (remoteId == null || remoteId!.isEmpty) {
      throw const AppException(
        AppErrorKind.unavailable,
        'Chọn ESP32 trong mục Kết nối trực tiếp trước.',
      );
    }
    await prepareBluetooth();
    _ensureActive();
    _closing = false;
    final device = BluetoothDevice.fromId(remoteId!);
    _device = device;
    try {
      await device.connect(
        license: License.nonprofit,
        timeout: const Duration(seconds: 15),
        mtu: Platform.isAndroid ? 247 : null,
      );
      _ensureActive();
      final services = await device.discoverServices();
      _ensureActive();
      final service = services
          .where((value) => value.uuid == Guid(BleContract.service))
          .firstOrNull;
      if (service == null) {
        throw const FormatException('ESP32 không có service SAEMS');
      }
      BluetoothCharacteristic find(String uuid) => service.characteristics
          .firstWhere((value) => value.uuid == Guid(uuid));
      _info = find(BleContract.deviceInfo);
      _telemetry = find(BleContract.latestTelemetry);
      _config = find(BleContract.deviceConfig);
      _command = find(BleContract.command);
      await _values?.cancel();
      _values = _telemetry!.onValueReceived.listen((bytes) {
        if (bytes.isEmpty || _closing || _disposed || _frames.isClosed) return;
        // Characteristic reads return plain JSON; notifications use frames.
        // A frame's message-id high byte can also equal '{'. Parse the whole
        // document before deciding, rather than dropping that valid frame.
        if (bytes.first == 123) {
          try {
            if (jsonDecode(utf8.decode(bytes)) is Map) return;
          } on FormatException {
            // Framed notification, not a characteristic-read response.
          }
        }
        _frames.add(bytes);
      }, onError: _reportError);
      await _telemetry!.setNotifyValue(true);
      _ensureActive();
      await _connection?.cancel();
      _connection = device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _ready = false;
          if (!_closing && !_disposed) {
            _reportError(
              const AppException(
                AppErrorKind.unavailable,
                'ESP32 đã ngắt Bluetooth; đang thử kết nối lại.',
              ),
            );
          }
        }
      });
      _ready = true;
    } catch (_) {
      await disconnect();
      rethrow;
    }
  }

  Future<Map<String, dynamic>> _read(BluetoothCharacteristic? value) async {
    if (value == null) throw StateError('BLE chưa kết nối');
    return Map<String, dynamic>.from(
      jsonDecode(utf8.decode(await value.read())) as Map,
    );
  }

  Future<Map<String, dynamic>> readTelemetry() async {
    await connect();
    return _read(_telemetry);
  }

  @override
  Future<Map<String, dynamic>> readDeviceInfo() async {
    await connect();
    return _read(_info);
  }

  @override
  Future<Map<String, dynamic>> readDeviceConfig() async {
    await connect();
    return _read(_config);
  }

  Future<void> _write(
    BluetoothCharacteristic? characteristic,
    Map<String, dynamic> document,
  ) async {
    final bytes = utf8.encode(jsonEncode(document));
    if (bytes.length > 512) {
      throw const FormatException(
        'Cấu hình BLE vượt 512 byte; hãy gửi từng nhóm hoặc dùng Wi-Fi AP.',
      );
    }
    // Permit ATT long writes only when the payload exceeds one negotiated
    // packet; ordinary configuration values use a write with response.
    await characteristic!.write(
      bytes,
      allowLongWrite: bytes.length > _device!.mtuNow - 3,
    );
  }

  @override
  Future<void> writeDeviceConfig(Map<String, dynamic> config) async {
    await connect();
    if (Platform.isAndroid) await _device!.createBond();
    await _write(_config, config);
  }

  @override
  Future<void> writeCommand(Map<String, dynamic> command) async {
    await connect();
    if (Platform.isAndroid) await _device!.createBond();
    await _write(_command, command);
  }

  @override
  Future<void> disconnect() async {
    _closing = true;
    _ready = false;
    await _values?.cancel();
    await _connection?.cancel();
    _values = null;
    _connection = null;
    if (_device?.isConnected == true) await _device!.disconnect();
  }

  Future<void> dispose() => _disposing ??= _dispose();

  Future<void> _dispose() async {
    _disposed = true;
    await disconnect();
    // An in-flight discovery must finish before final listener cleanup. It
    // checks _disposed after each await and cannot reopen this controller.
    final opening = _connecting;
    if (opening != null) {
      try {
        await opening;
      } catch (_) {
        // The original caller still receives its connection failure.
      }
    }
    await disconnect();
    await _frames.close();
  }
}
