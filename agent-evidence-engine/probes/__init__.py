"""Base probe class — all probes inherit from this."""


class BaseProbe:
    """Base class for all probes.

    A probe COLLECTS evidence. It does NOT decide pass/fail.
    The PolicyEngine decides the verdict based on probe results.

    Subclasses must implement:
        PROBE_ID: str — unique identifier
        COMPONENT: str — which component this probe validates
        run(contract) -> dict — returns probe result with status, evidence, findings
    """
    PROBE_ID = "base"
    COMPONENT = "unknown"

    def run(self, contract: dict) -> dict:
        raise NotImplementedError("Subclasses must implement run()")
