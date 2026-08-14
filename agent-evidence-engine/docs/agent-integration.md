# Agent Integration

The AI agent reads `agent-report.json` and:
1. Checks `status` (PASS/FAIL/INCONCLUSIVE)
2. Reads `findings` for specific issues
3. Reads `agent_action.instruction` for guidance
4. Examines `probes` to see which probe failed
5. Requests additional evidence if needed (v3)

The agent CANNOT:
- Override a FAIL verdict
- Skip probes
- Change severity levels
