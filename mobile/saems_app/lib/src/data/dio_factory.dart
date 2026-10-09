import 'package:dio/dio.dart';

import '../core/app_exception.dart';

Dio createDio(String baseUrl, {String? token}) {
  final dio = Dio(
    BaseOptions(
      baseUrl: baseUrl.replaceFirst(RegExp(r'/$'), ''),
      connectTimeout: const Duration(seconds: 6),
      receiveTimeout: const Duration(seconds: 8),
      sendTimeout: const Duration(seconds: 6),
      headers: token == null || token.isEmpty
          ? null
          : {'Authorization': 'Bearer $token'},
    ),
  );
  dio.interceptors.add(
    LogInterceptor(
      requestHeader: false,
      responseHeader: false,
      requestBody: false,
      responseBody: false,
      logPrint: (_) {},
    ),
  );
  return dio;
}

Never mapDioError(DioException error) {
  final status = error.response?.statusCode;
  if (error.type == DioExceptionType.connectionTimeout ||
      error.type == DioExceptionType.receiveTimeout ||
      error.type == DioExceptionType.sendTimeout) {
    throw const AppException(
      AppErrorKind.timeout,
      'Kết nối đã hết thời gian chờ.',
    );
  }
  if (status == 401 || status == 403) {
    throw AppException(
      AppErrorKind.unauthorized,
      'Không có quyền truy cập server.',
      statusCode: status,
    );
  }
  if (status == 404) {
    throw AppException(
      AppErrorKind.notFound,
      'Không tìm thấy thiết bị hoặc dữ liệu.',
      statusCode: status,
    );
  }
  if (status != null && status >= 500) {
    throw AppException(
      AppErrorKind.unavailable,
      'Server đang tạm thời không khả dụng.',
      statusCode: status,
    );
  }
  throw AppException(
    AppErrorKind.unavailable,
    'Không thể kết nối tới ${error.requestOptions.baseUrl}.',
    statusCode: status,
  );
}

String redactSecrets(String value) {
  return value
      .replaceAll(
        RegExp(r'Bearer\s+[^\s,}]+', caseSensitive: false),
        'Bearer ***',
      )
      .replaceAll(
        RegExp(
          r'("?(?:token|password|api_key)"?\s*[:=]\s*)[^\s,}]+',
          caseSensitive: false,
        ),
        r'$1***',
      );
}

Map<String, dynamic> jsonObject(Object? value) {
  if (value is Map<String, dynamic>) return value;
  if (value is Map) return Map<String, dynamic>.from(value);
  throw const AppException(
    AppErrorKind.invalidData,
    'Server trả về JSON không hợp lệ.',
  );
}
