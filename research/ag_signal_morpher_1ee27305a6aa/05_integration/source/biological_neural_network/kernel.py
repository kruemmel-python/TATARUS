"""CPU reference for the original generated scalar kernel.

The expression and protected-operator semantics intentionally mirror the
generator export.  The kernel itself is not simplified or optimized; only the
separate ``synaptic_gate`` wrapper maps its output into a non-negative interval.
"""

from __future__ import annotations

import math

KERNEL_EXPRESSION = (
    "((logabs(sin(cos(sdiv(x,x)))) - "
    "tanh(sdiv((sin(sdiv(x,x)) * -0.357064),"
    "(logabs(sin(cos(sdiv(x,x)))) - "
    "logabs(sin(cos(sdiv(x,x)))))))) - "
    "sin((logabs(sin(cos(sdiv(x,x)))) * "
    "(sin(cos(sdiv(x,x))) + cos(sin(sdiv(x,x)))))))"
)


def sanitize(value: float) -> float:
    """Match the generated finite-value and clamp semantics."""

    try:
        converted = float(value)
    except (TypeError, ValueError, OverflowError):
        return 0.0
    if not math.isfinite(converted):
        return 0.0
    return max(-1_000_000.0, min(1_000_000.0, converted))


def expclamp(x: float) -> float:
    return math.exp(max(-20.0, min(20.0, sanitize(x))))


def logabs(x: float) -> float:
    return math.log(abs(sanitize(x)) + 1e-9)


def sdiv(a: float, b: float) -> float:
    return sanitize(a) / (abs(sanitize(b)) + 1e-6)


def kernel(x: float) -> float:
    """Evaluate the original generated expression with protected operators."""

    x = sanitize(x)
    try:
        return sanitize(
            (
                logabs(math.sin(math.cos(sdiv(x, x))))
                - math.tanh(
                    sdiv(
                        math.sin(sdiv(x, x)) * -0.357064,
                        logabs(math.sin(math.cos(sdiv(x, x))))
                        - logabs(math.sin(math.cos(sdiv(x, x)))),
                    )
                )
            )
            - math.sin(
                logabs(math.sin(math.cos(sdiv(x, x))))
                * (
                    math.sin(math.cos(sdiv(x, x)))
                    + math.cos(math.sin(sdiv(x, x)))
                )
            )
        )
    except (ArithmeticError, ValueError, OverflowError):
        return 0.0


def synaptic_gate(
    state: float,
    *,
    input_scale: float = 1.0,
    minimum: float = 0.05,
    maximum: float = 0.95,
) -> float:
    """Project the kernel to a bounded, sign-preserving efficacy gate.

    ``0.5 * (1 + tanh(K))`` converts the signed kernel response to a
    non-negative coefficient.  Clipping is wrapper behavior and does not alter
    :func:`kernel`.
    """

    if input_scale <= 0.0:
        raise ValueError("input_scale must be positive")
    if not 0.0 <= minimum < maximum <= 1.0:
        raise ValueError("gate bounds must satisfy 0 <= minimum < maximum <= 1")
    raw = 0.5 * (1.0 + math.tanh(kernel(sanitize(state) * input_scale)))
    return max(minimum, min(maximum, raw))

