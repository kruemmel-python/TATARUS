"""Discovered scalar kernel only; no assigned domain wrapper."""
import math

def sanitize(v: float) -> float:
    try:
        x = float(v)
    except Exception:
        return 0.0
    return max(-1_000_000.0, min(1_000_000.0, x)) if math.isfinite(x) else 0.0

def expclamp(x: float) -> float:
    return math.exp(max(-20.0, min(20.0, sanitize(x))))

def logabs(x: float) -> float:
    return math.log(abs(sanitize(x)) + 1e-9)

def sdiv(a: float, b: float) -> float:
    return sanitize(a) / (abs(sanitize(b)) + 1e-6)

def kernel(x: float) -> float:
    x = sanitize(x)
    try:
        return sanitize(((logabs(math.sin(math.cos(sdiv(x,x)))) - math.tanh(sdiv((math.sin(sdiv(x,x)) * -0.357064),(logabs(math.sin(math.cos(sdiv(x,x)))) - logabs(math.sin(math.cos(sdiv(x,x)))))))) - math.sin((logabs(math.sin(math.cos(sdiv(x,x)))) * (math.sin(math.cos(sdiv(x,x))) + math.cos(math.sin(sdiv(x,x))))))))
    except Exception:
        return 0.0
