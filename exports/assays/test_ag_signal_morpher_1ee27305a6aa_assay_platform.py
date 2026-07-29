# Assigned wrapper hypothesis: signal_morpher
import math
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ag_signal_morpher_1ee27305a6aa_assay_platform as alg

def test_finite_kernel():
    for x in [-10,-1,0,0.5,1,10,float('nan'),float('inf')]:
        y = alg.kernel(x)
        assert math.isfinite(y)

def test_gas_limit():
    y, err = alg.transform_scalar(0.5, 0)
    assert err == 1

def test_algorithm_smoke():
    y, err = alg.fixed_point(0.25, iterations=16)
    assert math.isfinite(y)
    ys, err = alg.signal_morph([0.0, 0.5, 1.0])
    assert len(ys) == 3
    assert all(math.isfinite(v) for v in ys)
    z, err = alg.predict_next([0.0, 0.5, 0.75])
    assert math.isfinite(z)
    r, err = alg.root_refine(lambda x: x*x - 2.0, 0.0, 2.0, iterations=64)
    assert err in (0, 1)
    assert 0.0 <= r <= 2.0
    assert abs(r*r - 2.0) < 1e-4
    ctxv = alg.context_kernel(0.5, -0.2, -1.0, 1.0, 0.1, 2.0)
    assert math.isfinite(ctxv)
    ctx = alg.make_root_context(0.5, -0.2, 0.0, 2.0, -2.0, 2.0, 0.4, -0.3, 0.3, -0.4, 0.5, 0.2, 0.0, 0.5, 0.0, 2.0)
    assert all(k in ctx for k in alg.ROOT_FEATURES)
    pol = alg.root_policy(ctx)
    assert all(math.isfinite(v) for v in pol.values())
    sorted_vals, err = alg.adaptive_sort([3, 1, 2, 2])
    assert sorted_vals == sorted(sorted_vals)
    graph = {'a': [('b', 1.0), ('c', 5.0)], 'b': [('c', 1.0)], 'c': []}
    path, err = alg.graph_shortest_path(graph, 'a', 'c')
    assert path['reached'] and path['path'][0] == 'a' and path['path'][-1] == 'c'
    sched, err = alg.schedule_jobs([3, 1, 2, 4], workers=2)
    assert len(sched['assignment']) == 4 and len(sched['loads']) == 2
    parsed, err = alg.parse_and_repair(['(', 'x', '+', '1', ']'])
    assert 'tokens' in parsed and isinstance(parsed['repairs'], list)
    sol, err = alg.solve_constraints({'x': [1,2,3], 'y': [1,2,3]}, [lambda a: 'x' not in a or 'y' not in a or a['x'] < a['y']])
    assert sol['satisfied']
    stream, err = alg.stream_analyze([1,1,1,10,1,1])
    assert stream['count'] == 6 and math.isfinite(stream['mean'])
    comp, err = alg.compress_adaptive([1,1,1,2,2])
    assert len(comp['decoded']) == 5
    cache, err = alg.cache_simulate(['a','b','a','c','a'], capacity=2)
    assert cache['hits'] >= 1
    lb, err = alg.load_balance([1,2,3], workers=2)
    assert len(lb['loads']) == 2
    idx, err = alg.detect_anomalies([0.0] * 10 + [10.0] + [0.0] * 5)
    assert err == 0 and 10 in idx

def test_explore_map_is_reproducible_and_complete():
    first, err1 = alg.explore_map(0.25, iterations=96)
    second, err2 = alg.explore_map(0.25, iterations=96)
    assert err1 == err2 == 0 and first == second
    required = {'orbit','count','mean','variance','energy','terminal','transient_length','estimated_period','lyapunov_like'}
    assert required <= set(first)
    assert first['count'] == 96 and len(first['orbit']) == 96
    assert all(math.isfinite(v) and abs(v) <= 1.0 for v in first['orbit'])
    assert all(math.isfinite(first[k]) for k in ('mean','variance','energy','terminal','lyapunov_like'))

if __name__ == '__main__':
    test_finite_kernel(); test_gas_limit(); test_algorithm_smoke(); test_explore_map_is_reproducible_and_complete(); print('ok')
