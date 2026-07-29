// Validated reference kernel used by AGNervousSystemOpenClProbe.
// The full simulator remains CPU-authoritative until every transferred
// mechanism has a differential test against the deterministic reference.
__kernel void integrate(
    __global const float* voltage,
    __global const float* current,
    __global float* output,
    const float dt,
    const float resting,
    const float tau) {
    const size_t index = get_global_id(0);
    output[index] = voltage[index]
        + dt * ((resting - voltage[index] + current[index]) / tau);
}
