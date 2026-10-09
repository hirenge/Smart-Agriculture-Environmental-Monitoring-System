import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/telemetry.dart';
import '../state/app_state.dart';

class HistoryScreen extends ConsumerWidget {
  const HistoryScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final history = ref.watch(historyProvider);
    return Scaffold(
      appBar: AppBar(
        title: const Text('Lịch sử'),
        actions: [
          IconButton(
            onPressed: () => ref.invalidate(historyProvider),
            icon: const Icon(Icons.refresh),
          ),
        ],
      ),
      body: history.when(
        loading: () => const Center(child: CircularProgressIndicator()),
        error: (error, _) =>
            Center(child: Text('Không tải được lịch sử: $error')),
        data: (items) => items.isEmpty
            ? const Center(child: Text('Chưa có dữ liệu lịch sử'))
            : ListView(
                padding: const EdgeInsets.all(16),
                children: [
                  _ChartCard(
                    title: 'Nhiệt độ (°C)',
                    color: Colors.deepOrange,
                    items: items,
                    valueOf: (item) => item.temperatureC,
                  ),
                  _ChartCard(
                    title: 'Độ ẩm đất (%)',
                    color: Colors.green,
                    items: items,
                    valueOf: (item) => item.soilMoisturePct,
                  ),
                  _ChartCard(
                    title: 'Ánh sáng (lux)',
                    color: Colors.amber.shade800,
                    items: items,
                    valueOf: (item) => item.lightLux,
                  ),
                ],
              ),
      ),
    );
  }
}

class _ChartCard extends StatelessWidget {
  const _ChartCard({
    required this.title,
    required this.color,
    required this.items,
    required this.valueOf,
  });

  final String title;
  final Color color;
  final List<Telemetry> items;
  final double? Function(Telemetry) valueOf;

  @override
  Widget build(BuildContext context) {
    final spots = <FlSpot>[];
    for (var index = 0; index < items.length; index++) {
      final value = valueOf(items[index]);
      if (value != null) spots.add(FlSpot(index.toDouble(), value));
    }
    return Card(
      margin: const EdgeInsets.only(bottom: 16),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 16, 16, 12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 20),
            SizedBox(
              height: 190,
              child: spots.isEmpty
                  ? const Center(child: Text('Cảm biến chưa có dữ liệu'))
                  : LineChart(
                      LineChartData(
                        borderData: FlBorderData(show: false),
                        gridData: const FlGridData(drawVerticalLine: false),
                        titlesData: const FlTitlesData(
                          topTitles: AxisTitles(
                            sideTitles: SideTitles(showTitles: false),
                          ),
                          rightTitles: AxisTitles(
                            sideTitles: SideTitles(showTitles: false),
                          ),
                          bottomTitles: AxisTitles(
                            sideTitles: SideTitles(showTitles: false),
                          ),
                        ),
                        lineBarsData: [
                          LineChartBarData(
                            spots: spots,
                            color: color,
                            barWidth: 3,
                            dotData: const FlDotData(show: false),
                            belowBarData: BarAreaData(
                              show: true,
                              color: color.withValues(alpha: .12),
                            ),
                          ),
                        ],
                      ),
                    ),
            ),
          ],
        ),
      ),
    );
  }
}
