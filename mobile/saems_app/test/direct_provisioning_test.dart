import 'package:dio/dio.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:saems_app/src/data/ble/ble_device_transport.dart';
import 'package:saems_app/src/data/direct_config_transport.dart';
import 'package:saems_app/src/data/settings_store.dart';
import 'package:saems_app/src/data/wifi_ap_device_transport.dart';
import 'package:saems_app/src/models/data_source.dart';

class ConfigurableBleAdapter extends FakeBleAdapter
    implements DirectConfigTransport {
  Map<String, dynamic>? written;

  @override
  Future<Map<String, dynamic>> readDeviceInfo() async => {
    'device_id': 'farm-01',
  };

  @override
  Future<Map<String, dynamic>> readDeviceConfig() async => {
    'device_id': 'farm-01',
    'telemetry_interval_s': 10,
    'power_mode': 'direct',
  };

  @override
  Future<void> writeDeviceConfig(Map<String, dynamic> config) async {
    written = config;
  }
}

void main() {
  test('BLE reconstructs live frames, not a generated mock reading', () async {
    final adapter = ConfigurableBleAdapter();
    final transport = BleDeviceTransport(adapter);
    final reading = transport.watchTelemetry('farm-01').first;
    await Future<void>.delayed(Duration.zero);
    adapter.emitJson({
      'device_id': 'farm-01',
      'data_mode': 'real',
      'temperature_c': 26.7,
      'soil_moisture_pct': null,
    }, messageId: 0x7b01);
    final value = await reading.timeout(const Duration(seconds: 1));
    expect(adapter.connected, isTrue);
    expect(value.temperatureC, 26.7);
    expect(value.soilMoisturePct, isNull);
    await adapter.dispose();
  });

  test('BLE forwards configuration and bounded pump commands', () async {
    final adapter = ConfigurableBleAdapter();
    final transport = BleDeviceTransport(adapter);
    expect((await transport.readDeviceInfo())['device_id'], 'farm-01');
    expect((await transport.readDeviceConfig())['telemetry_interval_s'], 10);
    expect((await transport.readDeviceConfig())['power_mode'], 'direct');
    await transport.writeDeviceConfig({
      'wifi_ssid': 'new-network',
      'power_mode': 'light',
    });
    expect(adapter.written, {
      'wifi_ssid': 'new-network',
      'power_mode': 'light',
    });
    await transport.setPump('farm-01', enabled: false, durationSeconds: 0);
    expect(adapter.lastCommand?['state'], 'off');
    await adapter.dispose();
  });

  test('AP reads and writes the firmware /v1/config contract', () async {
    final dio = Dio(BaseOptions(baseUrl: 'http://192.168.4.1'));
    RequestOptions? write;
    dio.interceptors.add(
      InterceptorsWrapper(
        onRequest: (request, handler) {
          if (request.method == 'PUT') write = request;
          handler.resolve(
            Response(
              requestOptions: request,
              statusCode: 200,
              data: {
                'device_id': 'farm-01',
                'telemetry_interval_s': 10,
                'power_mode': 'battery',
              },
            ),
          );
        },
      ),
    );
    final transport = WifiApDeviceTransport(dio: dio);
    expect((await transport.readDeviceConfig())['device_id'], 'farm-01');
    expect((await transport.readDeviceConfig())['power_mode'], 'battery');
    await transport.writeDeviceConfig({
      'wifi_ssid': 'hotspot',
      'power_mode': 'direct',
    });
    expect(write?.path, '/v1/config');
    expect(write?.data, {
      'wifi_ssid': 'hotspot',
      'power_mode': 'direct',
    });
    await transport.disconnect();
  });

  test('changing server settings preserves the chosen BLE device', () {
    const previous = AppSettings(
      apiBaseUrl: 'https://example.test',
      deviceId: 'farm-01',
      source: DataSourceType.ble,
      bleRemoteId: '11:22:33:44:55:66',
    );
    expect(
      previous.copyWith(source: DataSourceType.server).bleRemoteId,
      previous.bleRemoteId,
    );
  });
}
