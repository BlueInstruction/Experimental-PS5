"""Shared utilities."""
import subprocess
import shutil


def run_command(cmd: list[str], timeout: int = 120) -> dict:
    """Run a shell command and return structured result."""
    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=timeout
        )
        return {
            "exit_code": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
        }
    except subprocess.TimeoutExpired:
        return {"exit_code": -1, "stdout": "", "stderr": "timeout"}
    except FileNotFoundError:
        return {"exit_code": -1, "stdout": "", "stderr": "command not found"}


def file_exists(path: str) -> bool:
    return shutil.which(path) is not None or __import__("os").path.exists(path)
