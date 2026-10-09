import 'package:flutter_test/flutter_test.dart';
import 'package:saems_app/src/data/ble/flutter_ble_adapter.dart';

void main() {
  test('disposed BLE adapter cannot reconnect and dispose is idempotent', () async {
    final adapter = FlutterBleAdapter('1C:DB:D4:76:61:6E');
    await adapter.dispose();
    await adapter.dispose();
    await expectLater(adapter.connect(), throwsStateError);
    await adapter.disconnect();
  });
}
