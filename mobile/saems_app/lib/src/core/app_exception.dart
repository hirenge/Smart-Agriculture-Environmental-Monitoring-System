enum AppErrorKind {
  timeout,
  unauthorized,
  notFound,
  unavailable,
  invalidData,
  unknown,
}

class AppException implements Exception {
  const AppException(this.kind, this.message, {this.statusCode});

  final AppErrorKind kind;
  final String message;
  final int? statusCode;

  @override
  String toString() => message;
}
