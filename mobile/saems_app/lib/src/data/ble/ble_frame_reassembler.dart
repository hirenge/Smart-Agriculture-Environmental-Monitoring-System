import 'dart:convert';

class BleFrameReassembler {
  BleFrameReassembler({this.timeout = const Duration(seconds: 5)});

  final Duration timeout;
  final Map<int, _PendingMessage> _pending = {};

  String? addFrame(List<int> frame, {DateTime? now}) {
    if (frame.length < 5) {
      throw const FormatException('BLE frame is shorter than its header');
    }
    final currentTime = now ?? DateTime.now();
    _pending.removeWhere(
      (_, value) => currentTime.difference(value.updatedAt) > timeout,
    );

    final messageId = (frame[0] << 8) | frame[1];
    final index = frame[2];
    final total = frame[3];
    if (total == 0 || index >= total) {
      throw const FormatException('Invalid BLE frame index/total');
    }

    final pending = _pending.putIfAbsent(
      messageId,
      () => _PendingMessage(total: total, updatedAt: currentTime),
    );
    if (pending.total != total) {
      _pending.remove(messageId);
      throw const FormatException('BLE frame count changed mid-message');
    }
    pending.parts[index] = frame.sublist(4);
    pending.updatedAt = currentTime;

    if (pending.parts.length != total) return null;
    final bytes = <int>[];
    for (var part = 0; part < total; part++) {
      final chunk = pending.parts[part];
      if (chunk == null) return null;
      bytes.addAll(chunk);
    }
    _pending.remove(messageId);
    return utf8.decode(bytes);
  }
}

class _PendingMessage {
  _PendingMessage({required this.total, required this.updatedAt});

  final int total;
  DateTime updatedAt;
  final Map<int, List<int>> parts = {};
}
