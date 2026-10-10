import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:saems_app/src/data/settings_store.dart';
import 'package:saems_app/src/models/data_source.dart';
import 'package:saems_app/src/presentation/settings_screen.dart';
import 'package:saems_app/src/state/app_state.dart';

class MemorySettingsNotifier extends AppSettingsNotifier {
  MemorySettingsNotifier(this.initial);
  final Future<AppSettings> initial;

  @override
  Future<AppSettings> build() => initial;

  @override
  Future<void> save(AppSettings value) async => state = AsyncData(value);
}

void main() {
  testWidgets('settings waits for storage and follows externally selected source', (
    tester,
  ) async {
    final loaded = Completer<AppSettings>();
    await tester.pumpWidget(
      ProviderScope(
        overrides: [
          appSettingsProvider.overrideWith(() => MemorySettingsNotifier(loaded.future)),
        ],
        child: const MaterialApp(home: SettingsScreen()),
      ),
    );
    expect(find.byType(CircularProgressIndicator), findsOneWidget);
    const server = AppSettings(
      apiBaseUrl: 'https://example.test',
      deviceId: 'farm-01',
      source: DataSourceType.server,
    );
    loaded.complete(server);
    await tester.pumpAndSettle();
    expect(find.text('Server'), findsOneWidget);
    final container = ProviderScope.containerOf(tester.element(find.byType(SettingsScreen)));
    await container.read(appSettingsProvider.notifier).save(
      server.copyWith(source: DataSourceType.ble),
    );
    await tester.pumpAndSettle();
    expect(find.text('BLE trực tiếp'), findsOneWidget);
    expect(find.text('Mô phỏng'), findsNothing);
  });
}
