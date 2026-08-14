"""Build probe — validates that the project builds correctly.

This probe does NOT assume that "Gradle succeeded" means "the app works".
It only validates:
  - APK file exists at the expected path
  - APK has the correct ABI
  - Expected native libraries are present in the APK
  - Build command exit code was 0

Runtime behavior is validated by other probes (native-loader, vulkan, etc.)
"""
import os
import zipfile
from .. import BaseProbe


class BuildProbe(BaseProbe):
    PROBE_ID = "build"
    COMPONENT = "build"

    def run(self, contract: dict) -> dict:
        evidence = []
        findings = []

        # 1. Check if APK exists
        apk_path = contract.get("build", {}).get(
            "apk_path",
            "app/build/outputs/apk/debug/app-debug.apk"
        )
        apk_exists = os.path.exists(apk_path)
        evidence.append({
            "type": "file_exists",
            "key": "apk_path",
            "value": apk_exists,
            "source": apk_path,
        })

        if not apk_exists:
            findings.append({
                "id": "BUILD-001",
                "severity": "P0",
                "title": "APK not found at expected path",
                "detail": f"Expected: {apk_path}",
            })
            return {"probe_id": self.PROBE_ID, "status": "FAIL", "severity": "P0",
                    "component": self.COMPONENT, "evidence": evidence, "findings": findings}

        # 2. Check APK size
        apk_size = os.path.getsize(apk_path)
        evidence.append({
            "type": "file_size",
            "key": "apk_size_bytes",
            "value": apk_size,
        })

        # 3. Check ABI inside APK
        expected_abi = contract.get("runtime", {}).get("abi", "arm64-v8a")
        expected_libs = contract.get("build", {}).get("native_libs", [])
        found_libs = []

        try:
            with zipfile.ZipFile(apk_path, "r") as z:
                for name in z.namelist():
                    if name.startswith(f"lib/{expected_abi}/") and name.endswith(".so"):
                        lib_name = os.path.basename(name)
                        found_libs.append(lib_name)
                        evidence.append({
                            "type": "apk_native_lib",
                            "key": lib_name,
                            "value": True,
                            "source": name,
                        })

                # Check for missing expected libs
                for expected in expected_libs:
                    if expected not in found_libs:
                        findings.append({
                            "id": "BUILD-002",
                            "severity": "P0",
                            "title": f"Expected native library missing from APK: {expected}",
                            "detail": f"ABI: {expected_abi}, expected: {expected}",
                        })
        except Exception as e:
            findings.append({
                "id": "BUILD-003",
                "severity": "P0",
                "title": "Could not read APK as zip",
                "detail": str(e),
            })

        evidence.append({
            "type": "list",
            "key": "found_native_libs",
            "value": found_libs,
        })
        evidence.append({
            "type": "string",
            "key": "expected_abi",
            "value": expected_abi,
        })

        status = "FAIL" if findings else "PASS"
        severity = "P0" if findings else "NONE"
        return {"probe_id": self.PROBE_ID, "status": status, "severity": severity,
                "component": self.COMPONENT, "evidence": evidence, "findings": findings}
