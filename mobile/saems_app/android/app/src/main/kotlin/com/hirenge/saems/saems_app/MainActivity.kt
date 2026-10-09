package com.hirenge.saems.saems_app

import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel
import android.Manifest
import android.content.pm.PackageManager
import android.os.Build

class MainActivity : FlutterActivity() {
    private var pendingPermission: MethodChannel.Result? = null

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "saems/permissions")
            .setMethodCallHandler { call, result ->
                if (call.method != "requestBle") {
                    result.notImplemented()
                } else if (pendingPermission != null) {
                    result.error("busy", "Đang yêu cầu quyền Bluetooth", null)
                } else {
                    val permissions = if (Build.VERSION.SDK_INT >= 31) {
                        arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
                    } else {
                        arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
                    }
                    if (Build.VERSION.SDK_INT < 23 || permissions.all {
                        checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED
                    }) {
                        result.success(true)
                    } else {
                        pendingPermission = result
                        requestPermissions(permissions, 4201)
                    }
                }
            }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<out String>, grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 4201) {
            pendingPermission?.success(grantResults.isNotEmpty() &&
                grantResults.all { it == PackageManager.PERMISSION_GRANTED })
            pendingPermission = null
        }
    }
}
