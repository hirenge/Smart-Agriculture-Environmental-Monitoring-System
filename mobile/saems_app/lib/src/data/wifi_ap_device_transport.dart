import 'dart:async';

import 'package:dio/dio.dart';

import '../core/app_config.dart';
import '../models/device_models.dart';
import '../models/telemetry.dart';
import 'device_transport.dart';
import 'direct_config_transport.dart';
import 'dio_factory.dart';

class WifiApDeviceTransport implements DeviceTransport, DirectConfigTransport {
  WifiApDeviceTransport({String baseUrl = AppConfig.wifiApBaseUrl, Dio? dio})
    : _dio = dio ?? createDio(baseUrl);

  final Dio _dio;

  @override
  Future<Map<String, dynamic>> readDeviceInfo() async {
    final status = await readDeviceConfig();
    return {'device_id': status['device_id'], 'project': 'SAEMS'};
  }

  @override
  Future<Map<String, dynamic>> readDeviceConfig() async {
    try {
      return jsonObject((await _dio.get<Object?>('/v1/config')).data);
    } on DioException catch (error) {
      mapDioError(error);
    }
  }

  @override
  Future<void> writeDeviceConfig(Map<String, dynamic> config) async {
    try {
      await _dio.put<Object?>('/v1/config', data: config);
    } on DioException catch (error) {
      mapDioError(error);
    }
  }

  @override
  Future<void> connect() async {
    try {
      await _dio.get<Object?>('/v1/health');
    } on DioException catch (error) {
      mapDioError(error);
    }
  }

  @override
  Future<void> disconnect() async => _dio.close();

  @override
  Future<Telemetry> readLatest(String deviceId) async {
    try {
      final response = await _dio.get<Object?>('/v1/telemetry/latest');
      return Telemetry.fromJson(jsonObject(response.data));
    } on DioException catch (error) {
      mapDioError(error);
    }
  }

  @override
  Future<List<Telemetry>> readHistory(
    String deviceId, {
    int limit = 100,
  }) async {
    try {
      final response = await _dio.get<Object?>(
        '/v1/telemetry/history',
        queryParameters: {'limit': limit},
      );
      final body = jsonObject(response.data);
      final items = body['items'];
      if (items is! List) return const [];
      return items
          .whereType<Map>()
          .map((item) => Telemetry.fromJson(Map<String, dynamic>.from(item)))
          .toList(growable: false);
    } on DioException catch (error) {
      mapDioError(error);
    }
  }

  @override
  Future<DeviceStatus> readStatus(String deviceId) async {
    try {
      final response = await _dio.get<Object?>('/v1/status');
      return DeviceStatus.fromJson(jsonObject(response.data));
    } on DioException catch (error) {
      mapDioError(error);
    }
  }

  @override
  Future<DeviceConfig?> readConfig(String deviceId) async {
    try {
      final response = await _dio.get<Object?>('/v1/config');
      final body = jsonObject(response.data);
      return DeviceConfig(
        deviceId: body['device_id']?.toString() ?? deviceId,
        telemetryIntervalSeconds:
            int.tryParse(body['telemetry_interval_s']?.toString() ?? '') ?? 10,
        soilDryRaw: int.tryParse(body['soil_dry_raw']?.toString() ?? ''),
        soilWetRaw: int.tryParse(body['soil_wet_raw']?.toString() ?? ''),
      );
    } on DioException catch (error) {
      mapDioError(error);
    }
  }

  @override
  Future<CommandResult> setPump(
    String deviceId, {
    required bool enabled,
    int durationSeconds = 60,
  }) async {
    try {
      final response = await _dio.post<Object?>(
        '/v1/commands/pump',
        data: {'state': enabled ? 'on' : 'off', 'duration_s': durationSeconds},
      );
      return CommandResult.fromJson(jsonObject(response.data));
    } on DioException catch (error) {
      mapDioError(error);
    }
  }

  @override
  Stream<Telemetry> watchTelemetry(String deviceId) async* {
    while (true) {
      yield await readLatest(deviceId);
      await Future<void>.delayed(AppConfig.pollInterval);
    }
  }
}
