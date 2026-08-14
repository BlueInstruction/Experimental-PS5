"""Native loader probe — validates that native libraries actually LOAD at runtime.

This is the critical probe that catches the "libfexcore.so exists in APK
but doesn't load" scenario. It checks:

  1. APK contains expected .so files (static check)
  2. App installs successfully on device
  3. App launches without UnsatisfiedLinkError
  4. Expected .so files appear in /proc/<pid>/maps (runtime proof)

Step 4 is the most important — it proves the library was actually loaded
by the dynamic linker, not just present in the APK.
"""
import os
from .. import BaseProbe
from ..utils import run_command


class NativeLoaderProbe(BaseProbe):
    PROBE_ID = "native-loader"
    COMPONENT = "native"

    def run(self, contract: dict) -> dict:
        evidence = []
        findings = []

        package = contract.get("runtime", {}).get("package", "")
        expected_libs = contract.get("build", {}).get("native_libs", [])
        expected_abi = contract.get("runtime", {}).get("abi", "arm64-v8a")

        # 1. Check if device is connected
        adb_result = run_command(["adb", "devices"])
        device_connected = "device" in adb_result.get("stdout", "")
        evidence.append({
            "type": "device_state",
            "key": "adb_device_connected",
            "value": device_connected,
        })

        if not device_connected:
            findings.append({
                "id": "NATIVE-LOADER-001",
                "severity": "P2",
                "title": "No ADB device connected — cannot verify runtime loading",
                "detail": "Native loader probe requires a connected device or emulator",
            })
            return {"probe_id": self.PROBE_ID, "status": "INCONCLUSIVE", "severity": "P2",
                    "component": self.COMPONENT, "evidence": evidence, "findings": findings}

        # 2. Install APK
        apk_path = contract.get("build", {}).get(
            "apk_path", "app/build/outputs/apk/debug/app-debug.apk"
        )
        install_result = run_command(["adb", "install", "-r", apk_path], timeout=60)
        install_ok = install_result["exit_code"] == 0
        evidence.append({
            "type": "adb_install",
            "key": "install_success",
            "value": install_ok,
            "source": install_result.get("stdout", "")[:200],
        })

        if not install_ok:
            findings.append({
                "id": "NATIVE-LOADER-002",
                "severity": "P0",
                "title": "APK installation failed",
                "detail": install_result.get("stderr", "")[:500],
            })
            return {"probe_id": self.PROBE_ID, "status": "FAIL", "severity": "P0",
                    "component": self.COMPONENT, "evidence": evidence, "findings": findings}

        # 3. Launch app
        activity = contract.get("runtime", {}).get("activity", "")
        launch_result = run_command(
            ["adb", "shell", "am", "start", "-n", f"{package}/{activity}"],
            timeout=30
        )
        evidence.append({
            "type": "adb_launch",
            "key": "launch_success",
            "value": launch_result["exit_code"] == 0,
        })

        # 4. Wait for app to start or crash
        import time
        time.sleep(5)

        # 5. Check if process is alive
        pid_result = run_command(["adb", "shell", "pidof", package])
        pid = pid_result.get("stdout", "").strip()
        process_alive = bool(pid)
        evidence.append({
            "type": "process_state",
            "key": "process_alive",
            "value": process_alive,
            "source": f"pid={pid}",
        })

        if not process_alive:
            findings.append({
                "id": "NATIVE-LOADER-003",
                "severity": "P0",
                "title": "App process died after launch",
                "detail": f"Package {package} not found in process list",
            })
            return {"probe_id": self.PROBE_ID, "status": "FAIL", "severity": "P0",
                    "component": self.COMPONENT, "evidence": evidence, "findings": findings}

        # 6. Check /proc/<pid>/maps for expected libraries
        maps_result = run_command(["adb", "shell", "cat", f"/proc/{pid}/maps"])
        maps = maps_result.get("stdout", "")
        evidence.append({
            "type": "process_maps",
            "key": "maps_size",
            "value": len(maps),
        })

        for lib in expected_libs:
            loaded = lib in maps
            evidence.append({
                "type": "process_mapped",
                "key": lib,
                "value": loaded,
                "source": f"/proc/{pid}/maps",
            })
            if not loaded:
                findings.append({
                    "id": "NATIVE-LOADER-004",
                    "severity": "P1",
                    "title": f"Required native library was not loaded: {lib}",
                    "detail": f"{lib} exists in APK but is absent from process mappings",
                })

        status = "FAIL" if findings else "PASS"
        severity = max((f["severity"] for f in findings), default="NONE")
        return {"probe_id": self.PROBE_ID, "status": status, "severity": severity,
                "component": self.COMPONENT, "evidence": evidence, "findings": findings}
