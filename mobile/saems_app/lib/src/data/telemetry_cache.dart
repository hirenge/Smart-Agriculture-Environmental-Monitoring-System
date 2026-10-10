import 'package:sqflite/sqflite.dart';

import '../models/telemetry.dart';

class TelemetryCache {
  Database? _database;

  Future<Database> _open() async {
    if (_database != null) return _database!;
    final path = '${await getDatabasesPath()}/saems_mobile.db';
    _database = await openDatabase(
      path,
      version: 1,
      onCreate: (database, _) async {
        await database.execute('''
          CREATE TABLE telemetry_cache (
            device_id TEXT PRIMARY KEY,
            payload TEXT NOT NULL,
            cached_at TEXT NOT NULL
          )
        ''');
      },
    );
    return _database!;
  }

  Future<void> save(Telemetry telemetry) async {
    final database = await _open();
    await database.insert('telemetry_cache', {
      'device_id': telemetry.deviceId,
      'payload': telemetry.encode(),
      'cached_at': DateTime.now().toUtc().toIso8601String(),
    }, conflictAlgorithm: ConflictAlgorithm.replace);
  }

  Future<Telemetry?> readLatest(String deviceId) async {
    final database = await _open();
    final rows = await database.query(
      'telemetry_cache',
      where: 'device_id = ?',
      whereArgs: [deviceId],
      limit: 1,
    );
    if (rows.isEmpty) return null;
    return Telemetry.decode(rows.first['payload']! as String);
  }
}
