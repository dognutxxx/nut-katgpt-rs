"""v1.09 deterministic context-Engram reference benchmark.
Host simulation only; board timing/memory must come from ESP-IDF serial evidence.
"""
import random

STATES = 32
DWELL_BUCKETS = 5
CONF_MIN = 3
MARGIN = 16
SAMPLES = 100_000

bank = [[(s * 11 + d * 3) % 127 - 63 for d in range(16)] for s in range(STATES)]


def nearest(query):
    ds = [sum(abs(a-b) for a,b in zip(query, ref)) for ref in bank]
    raw = min(range(STATES), key=ds.__getitem__)
    return raw, ds


def run(seed, random_transition=False):
    rng = random.Random(seed)
    candidate = [[0]*DWELL_BUCKETS for _ in range(STATES)]
    confidence = [[0]*DWELL_BUCKETS for _ in range(STATES)]
    raw_ok = aided_ok = used = harmed = corrected = 0
    previous = 0
    dwell = 0
    truth = 0
    for i in range(SAMPLES):
        if random_transition:
            truth = rng.randrange(STATES)
            dwell = min(DWELL_BUCKETS-1, dwell+1) if truth == previous else 0
        else:
            if i % 5 == 0:
                truth = (truth + 1) % STATES
                dwell = 0
            else:
                dwell = min(DWELL_BUCKETS-1, dwell+1)
        query = [max(-128, min(127, v+rng.randint(-7,7))) for v in bank[truth]]
        raw, ds = nearest(query)
        c = confidence[previous][dwell]
        cand = candidate[previous][dwell]
        aided = raw
        if c >= CONF_MIN and ds[cand]-ds[raw] <= MARGIN:
            aided = cand
            used += aided != raw
        raw_ok += raw == truth
        aided_ok += aided == truth
        harmed += raw == truth and aided != truth
        corrected += raw != truth and aided == truth
        if c == 0:
            candidate[previous][dwell] = truth; confidence[previous][dwell] = 1
        elif cand == truth:
            confidence[previous][dwell] = min(255, c+1)
        else:
            confidence[previous][dwell] = c-1
            if confidence[previous][dwell] == 0:
                candidate[previous][dwell] = truth; confidence[previous][dwell] = 1
        previous = truth
    return raw_ok/SAMPLES, aided_ok/SAMPLES, used/SAMPLES, corrected, harmed

for mode in (False, True):
    vals = [run(109+s, mode) for s in range(10)]
    print("random_transition" if mode else "sequential")
    print("raw_accuracy=%.4f%%" % (100*sum(v[0] for v in vals)/len(vals)))
    print("aided_accuracy=%.4f%%" % (100*sum(v[1] for v in vals)/len(vals)))
    print("activation=%.4f%%" % (100*sum(v[2] for v in vals)/len(vals)))
    print("corrected=%d harmed=%d" % (sum(v[3] for v in vals), sum(v[4] for v in vals)))
print("persistent_context_memory_bytes=320")
print("BOARD_PASS=NO")
