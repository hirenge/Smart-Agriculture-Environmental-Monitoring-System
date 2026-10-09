import 'dart:async';

import 'package:dio/dio.dart';

import '../core/app_config.dart';
import '../models/device_models.dart';
import '../models/telemetry.dart';
import 'device_transport.dart';
import 'dio_factory.dart';

class ServerApiTransport implements DeviceTransport {
  ServerApiTransport({required String baseUrl, String? token, Dio? dio})
    : _dio = dio ?? createDio(baseUrl, token: token);

  final Dio _dio;

  @override
  Future<void> connect() async {
    try {
      await _dio.get<Object?>('/health');
    } on DioException catch (error) {
      mapDioError(error);
    }
  }

  @override
  Future<void> disconnect() async => _dio.close();

  @override
  Future<Telemetry> readLatest(String deviceId) async {
    try {
      final response = await _dio.get<Object?>(
        '/api/v1/devices/$deviceId/telemetry/latest',
      );
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
        '/api/v1/devices/$deviceId/telemetry',
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
      final response = await _dio.get<Object?>(
        '/api/v1/devices/$deviceId/status',
      );
      return DeviceStatus.fromJson(jsonObject(response.data));
    } on DioException catch (error) {
      mapDioError(error);
    }
  }

  @override
  Future<DeviceConfig?> readConfig(String deviceId) async => null;

  @override
  Future<CommandResult> setPump(
    String deviceId, {
    required bool enabled,
    int durationSeconds = 60,
  }) async {
    try {
      final response = await _dio.post<Object?>(
        '/api/v1/devices/$deviceId/commands/pump',
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
