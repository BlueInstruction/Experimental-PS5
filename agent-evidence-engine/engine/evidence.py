"""Evidence collector — stores structured evidence from probes."""
from datetime import datetime, timezone


class EvidenceCollector:
    def __init__(self):
        self._items = []

    def add(self, evidence_type: str, key: str, value, source: str = ""):
        self._items.append({
            "type": evidence_type,
            "key": key,
            "value": value,
            "source": source,
            "timestamp": datetime.now(timezone.utc).isoformat(),
        })

    def all(self):
        return self._items

    def find(self, key: str):
        return [e for e in self._items if e["key"] == key]
