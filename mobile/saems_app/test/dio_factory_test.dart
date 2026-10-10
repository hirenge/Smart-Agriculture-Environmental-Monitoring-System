import 'package:dio/dio.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:saems_app/src/core/app_exception.dart';
import 'package:saems_app/src/data/dio_factory.dart';

void main() {
  test('maps timeout without leaking request details', () {
    final error = DioException(
      requestOptions: RequestOptions(baseUrl: 'https://example.test'),
      type: DioExceptionType.connectionTimeout,
    );
    expect(
      () => mapDioError(error),
      throwsA(
        isA<AppException>().having(
          (value) => value.kind,
          'kind',
          AppErrorKind.timeout,
        ),
      ),
    );
  });

  test('maps HTTP 404 to a not-found error', () {
    final options = RequestOptions(baseUrl: 'https://example.test');
    final error = DioException(
      requestOptions: options,
      response: Response<void>(requestOptions: options, statusCode: 404),
      type: DioExceptionType.badResponse,
    );
    expect(
      () => mapDioError(error),
      throwsA(
        isA<AppException>().having(
          (value) => value.kind,
          'kind',
          AppErrorKind.notFound,
        ),
      ),
    );
  });

  test('redacts bearer tokens and passwords', () {
    final sanitized = redactSecrets(
      'Authorization: Bearer secret-token password=super-secret',
    );
    expect(sanitized, isNot(contains('secret-token')));
    expect(sanitized, isNot(contains('super-secret')));
  });
}
