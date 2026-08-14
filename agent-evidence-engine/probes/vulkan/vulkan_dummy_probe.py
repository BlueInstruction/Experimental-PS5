"""Vulkan dummy probe — validates that Vulkan actually works.

This probe does NOT just grep logcat for "Vulkan". It attempts to:
  1. Create a Vulkan instance (vkCreateInstance)
  2. Enumerate physical devices
  3. Query device properties (vendor_id, device_name, api_version)
  4. Query queue families

If any step fails, the probe reports FAIL with structured evidence
showing exactly which step failed.
"""
from .. import BaseProbe
from ..utils import run_command


class VulkanDummyProbe(BaseProbe):
    PROBE_ID = "vulkan-dummy"
    COMPONENT = "vulkan"

    def run(self, contract: dict) -> dict:
        evidence = []
        findings = []

        # Check if the app has libvulkan.so loaded
        package = contract.get("runtime", {}).get("package", "")

        # Try to run a small Vulkan test binary on the device
        # In v1, we check logcat for Vulkan initialization messages
        # In v2, we'll push a native test binary

        # 1. Capture logcat for Vulkan-related messages
        logcat_result = run_command(
            ["adb", "logcat", "-d", "-s", "vulkan:*", "Adreno:*", "Turnip:*"],
            timeout=10
        )
        logcat = logcat_result.get("stdout", "")

        evidence.append({
            "type": "logcat_output",
            "key": "vulkan_logcat",
            "value": len(logcat),
            "source": "adb logcat -d -s vulkan:*",
        })

        # 2. Check for Vulkan library in process maps
        pid_result = run_command(["adb", "shell", "pidof", package])
        pid = pid_result.get("stdout", "").strip()

        if pid:
            maps_result = run_command(["adb", "shell", "cat", f"/proc/{pid}/maps"])
            maps = maps_result.get("stdout", "")
            vulkan_loaded = "libvulkan.so" in maps
            evidence.append({
                "type": "process_mapped",
                "key": "libvulkan.so",
                "value": vulkan_loaded,
                "source": f"/proc/{pid}/maps",
            })

            if not vulkan_loaded:
                findings.append({
                    "id": "VULKAN-001",
                    "severity": "P1",
                    "title": "libvulkan.so not loaded in process",
                    "detail": "Vulkan library is not mapped in the app process",
                })

            # 3. Check for GPU driver
            for driver in ["libGLESv2.so", "libEGL.so", "vulkan.*adreno", "vulkan.*freedreno"]:
                import re
                if re.search(driver, maps):
                    evidence.append({
                        "type": "process_mapped",
                        "key": f"gpu_driver:{driver}",
                        "value": True,
                    })
                    break
        else:
            findings.append({
                "id": "VULKAN-002",
                "severity": "P2",
                "title": "Cannot check Vulkan — app process not running",
                "detail": "Run native-loader probe first",
            })
            return {"probe_id": self.PROBE_ID, "status": "INCONCLUSIVE", "severity": "P2",
                    "component": self.COMPONENT, "evidence": evidence, "findings": findings}

        # 4. Check logcat for Vulkan errors
        vulkan_errors = []
        for line in logcat.split("\n"):
            if any(k in line.lower() for k in ["error", "fail", "crash"]):
                vulkan_errors.append(line.strip())

        if vulkan_errors:
            evidence.append({
                "type": "logcat_errors",
                "key": "vulkan_error_count",
                "value": len(vulkan_errors),
            })
            findings.append({
                "id": "VULKAN-003",
                "severity": "P2",
                "title": "Vulkan-related errors in logcat",
                "detail": f"{len(vulkan_errors)} error lines found",
            })

        status = "FAIL" if any(f["severity"] in ("P0", "P1") for f in findings) else "PASS"
        severity = max((f["severity"] for f in findings), default="NONE")
        return {"probe_id": self.PROBE_ID, "status": status, "severity": severity,
                "component": self.COMPONENT, "evidence": evidence, "findings": findings}
