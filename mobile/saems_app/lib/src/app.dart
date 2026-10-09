import 'package:flutter/material.dart';

import 'core/app_theme.dart';
import 'presentation/main_shell.dart';

class SAEMSApp extends StatelessWidget {
  const SAEMSApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'SAEMS',
      debugShowCheckedModeBanner: false,
      theme: AppTheme.light,
      home: const MainShell(),
    );
  }
}
