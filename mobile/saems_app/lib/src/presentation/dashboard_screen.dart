import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/data_source.dart';
import '../models/telemetry.dart';
import '../state/app_state.dart';

class DashboardScreen extends ConsumerWidget {
  const DashboardScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final settings = ref.watch(currentSettingsProvider);
    final telemetry = ref.watch(telemetryProvider);
    return RefreshIndicator(
      onRefresh: () async {
        ref.invalidate(telemetryProvider);
        await ref.read(telemetryProvider.future);
      },
      child: CustomScrollView(
        physics: const AlwaysScrollableScrollPhysics(),
        slivers: [
          SliverAppBar.large(
            title: const Text('SAEMS'),
            actions: [
              Padding(
                padding: const EdgeInsets.only(right: 16),
                child: Chip(
                  avatar: const Icon(Icons.eco, size: 18),
                  label: Text(settings.deviceId),
                ),
              ),
            ],
          ),
          SliverPadding(
            padding: const EdgeInsets.fromLTRB(16, 0, 16, 24),
            sliver: SliverToBoxAdapter(
              child: telemetry.when(
                loading: () => const _LoadingDashboard(),
                error: (error, _) => _ErrorDashboard(error: error),
                data: (reading) => _DashboardContent(reading: reading),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _DashboardContent extends StatelessWidget {
  const _DashboardContent({required this.reading});

  final Sourced<Telemetry> reading;

  @override
  Widget build(BuildContext context) {
    final telemetry = reading.value;
    final modeColor = telemetry.dataMode == DataMode.simulated
        ? Colors.orange
        : telemetry.dataMode == DataMode.real
        ? Colors.green
        : Colors.blueGrey;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            Chip(
              avatar: Icon(
                reading.source.isDirect ? Icons.link : Icons.cloud_outlined,
                size: 18,
              ),
              label: Text(reading.source.label),
            ),
            Chip(
              avatar: Icon(Icons.science_outlined, color: modeColor, size: 18),
              label: Text(telemetry.dataMode.label),
              side: BorderSide(color: modeColor.withValues(alpha: .4)),
            ),
            if (reading.fromCache || telemetry.isStale())
              const Chip(
                avatar: Icon(Icons.history, size: 18),
                label: Text('Dữ liệu cũ / cache'),
              ),
          ],
        ),
        const SizedBox(height: 16),
        GridView.count(
          crossAxisCount: 2,
          shrinkWrap: true,
          physics: const NeverScrollableScrollPhysics(),
          crossAxisSpacing: 12,
          mainAxisSpacing: 12,
          childAspectRatio: 1.18,
          children: [
            _SensorCard(
              title: 'Nhiệt độ',
              value: _format(telemetry.temperatureC, '°C'),
              icon: Icons.thermostat,
              color: Colors.deepOrange,
            ),
            _SensorCard(
              title: 'Độ ẩm không khí',
              value: _format(telemetry.humidityPct, '%'),
              icon: Icons.water_drop_outlined,
              color: Colors.blue,
            ),
            _SensorCard(
              title: 'Độ ẩm đất',
              value: _format(telemetry.soilMoisturePct, '%'),
              icon: Icons.grass,
              color: Colors.green,
            ),
            _SensorCard(
              title: 'Ánh sáng',
              value: _format(telemetry.lightLux, ' lux'),
              icon: Icons.light_mode_outlined,
              color: Colors.amber.shade800,
            ),
          ],
        ),
        const SizedBox(height: 12),
        Card(
          child: ListTile(
            leading: CircleAvatar(
              child: Icon(
                telemetry.pumpState == 'on'
                    ? Icons.water
                    : Icons.water_outlined,
              ),
            ),
            title: const Text('Máy bơm'),
            subtitle: Text(
              telemetry.pumpState == null
                  ? 'Chưa có trạng thái'
                  : 'Đang ${telemetry.pumpState}',
            ),
            trailing: Text(_time(telemetry.receivedAt ?? telemetry.timestamp)),
          ),
        ),
      ],
    );
  }

  static String _format(double? value, String unit) =>
      value == null ? '--' : '${value.toStringAsFixed(1)}$unit';

  static String _time(DateTime value) {
    final local = value.toLocal();
    String two(int number) => number.toString().padLeft(2, '0');
    return '${two(local.hour)}:${two(local.minute)}:${two(local.second)}';
  }
}

class _SensorCard extends StatelessWidget {
  const _SensorCard({
    required this.title,
    required this.value,
    required this.icon,
    required this.color,
  });

  final String title;
  final String value;
  final IconData icon;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            CircleAvatar(
              backgroundColor: color.withValues(alpha: .12),
              foregroundColor: color,
              child: Icon(icon),
            ),
            Text(title, style: Theme.of(context).textTheme.bodyMedium),
            Text(value, style: Theme.of(context).textTheme.headlineSmall),
          ],
        ),
      ),
    );
  }
}

class _LoadingDashboard extends StatelessWidget {
  const _LoadingDashboard();

  @override
  Widget build(BuildContext context) => const Padding(
    padding: EdgeInsets.only(top: 100),
    child: Center(child: CircularProgressIndicator()),
  );
}

class _ErrorDashboard extends StatelessWidget {
  const _ErrorDashboard({required this.error});

  final Object error;

  @override
  Widget build(BuildContext context) => Card(
    child: Padding(
      padding: const EdgeInsets.all(24),
      child: Column(
        children: [
          const Icon(Icons.cloud_off, size: 48),
          const SizedBox(height: 12),
          const Text('Không lấy được dữ liệu'),
          const SizedBox(height: 8),
          Text('$error', textAlign: TextAlign.center),
        ],
      ),
    ),
  );
}
