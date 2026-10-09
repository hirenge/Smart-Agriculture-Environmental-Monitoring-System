abstract interface class DirectConfigTransport {
  Future<Map<String, dynamic>> readDeviceInfo();
  Future<Map<String, dynamic>> readDeviceConfig();
  Future<void> writeDeviceConfig(Map<String, dynamic> config);
}
