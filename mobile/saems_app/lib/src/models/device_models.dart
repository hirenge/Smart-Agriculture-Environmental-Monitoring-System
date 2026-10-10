import 'telemetry.dart';

class DeviceStatus {
  const DeviceStatus({
    required this.deviceId,
    required this.online,
    required this.lastSeen,
    this.telemetry,
  });

  final String deviceId;
  final bool online;
  final DateTime? lastSeen;
  final Telemetry? telemetry;

  factory DeviceStatus.fromJson(Map<String, dynamic> json) => DeviceStatus(
    deviceId: json['device_id']?.toString() ?? '',
    online: json['online'] == true,
    lastSeen: DateTime.tryParse(json['last_seen']?.toString() ?? '')?.toUtc(),
    telemetry: json['telemetry'] is Map
        ? Telemetry.fromJson(
            Map<String, dynamic>.from(json['telemetry'] as Map),
          )
        : null,
  );
}

class DeviceConfig {
  const DeviceConfig({
    required this.deviceId,
    this.telemetryIntervalSeconds = 10,
    this.soilDryRaw,
    this.soilWetRaw,
    this.powerMode = 'direct',
  });

  final String deviceId;
  final int telemetryIntervalSeconds;
  final int? soilDryRaw;
  final int? soilWetRaw;
  final String powerMode;

  factory DeviceConfig.fromJson(Map<String, dynamic> json) => DeviceConfig(
    deviceId: json['device_id']?.toString() ?? '',
    telemetryIntervalSeconds:
        int.tryParse(json['telemetry_interval_s']?.toString() ?? '') ?? 10,
    soilDryRaw: int.tryParse(json['soil_dry_raw']?.toString() ?? ''),
    soilWetRaw: int.tryParse(json['soil_wet_raw']?.toString() ?? ''),
    powerMode: json['power_mode']?.toString() ?? 'direct',
  );

  Map<String, dynamic> toJson() => {
    'device_id': deviceId,
    'telemetry_interval_s': telemetryIntervalSeconds,
    'soil_dry_raw': soilDryRaw,
    'soil_wet_raw': soilWetRaw,
    'power_mode': powerMode,
  };
}

class CommandResult {
  const CommandResult({
    required this.status,
    required this.deviceId,
    this.requestId,
    this.message,
  });

  final String status;
  final String deviceId;
  final String? requestId;
  final String? message;

  factory CommandResult.fromJson(Map<String, dynamic> json) => CommandResult(
    status: json['status']?.toString() ?? 'unknown',
    deviceId: json['device_id']?.toString() ?? '',
    requestId: json['request_id']?.toString(),
    message: json['message']?.toString(),
  );
}
