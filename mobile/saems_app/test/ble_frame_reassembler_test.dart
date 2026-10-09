import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:saems_app/src/data/ble/ble_frame_reassembler.dart';

List<int> frame(int id, int index, int total, String payload) => [
  id >> 8,
  id & 0xff,
  index,
  total,
  ...utf8.encode(payload),
];

void main() {
  test('reassembles frames received out of order', () {
    final reassembler = BleFrameReassembler();
    expect(reassembler.addFrame(frame(7, 1, 2, 'world')), isNull);
    expect(reassembler.addFrame(frame(7, 0, 2, 'hello ')), 'hello world');
  });

  test('drops an incomplete message after timeout', () {
    final reassembler = BleFrameReassembler(
      timeout: const Duration(seconds: 1),
    );
    final start = DateTime.utc(2026, 10, 2);
    expect(reassembler.addFrame(frame(1, 0, 2, 'old'), now: start), isNull);
    expect(
      reassembler.addFrame(
        frame(1, 1, 2, 'tail'),
        now: start.add(const Duration(seconds: 2)),
      ),
      isNull,
    );
  });

  test('rejects invalid ordering metadata', () {
    final reassembler = BleFrameReassembler();
    expect(
      () => reassembler.addFrame(frame(1, 2, 2, 'x')),
      throwsFormatException,
    );
  });
}
