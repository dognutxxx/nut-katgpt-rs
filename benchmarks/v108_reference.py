"""Deterministic Python reference benchmark for the v1.08 memory policy."""
from collections import defaultdict
import random

random.seed(108)
bank = [[(s * 11 + d * 3) % 127 - 63 for d in range(16)] for s in range(32)]
memory = defaultdict(lambda: [0, 0])
raw_ok = aided_ok = activated = harmed = 0
for i in range(100_000):
    truth = (i // 5 + 1) % 32
    query = tuple(max(-128, min(127, v + random.randint(-7, 7))) for v in bank[truth])
    distances = [sum(abs(a - b) for a, b in zip(query, ref)) for ref in bank]
    raw = min(range(32), key=distances.__getitem__)
    key = tuple(v // 16 for v in query)
    successor, confidence = memory[key]
    aided = successor if confidence >= 2 else raw
    raw_ok += raw == truth
    aided_ok += aided == truth
    activated += confidence >= 2
    harmed += aided != truth and raw == truth
    if memory[key][0] == truth:
        memory[key][1] = min(255, memory[key][1] + 1)
    else:
        memory[key] = [truth, 1]

print("v1.08 engram-assisted benchmark")
print(f"samples=100000")
print(f"raw_accuracy={raw_ok / 1000:.4f}%")
print(f"aided_accuracy={aided_ok / 1000:.4f}%")
print(f"engram_activation_pct={activated / 1000:.4f}%")
print(f"harmed_queries={harmed}")
print("BOARD_PASS=NO (no physical serial evidence)")
