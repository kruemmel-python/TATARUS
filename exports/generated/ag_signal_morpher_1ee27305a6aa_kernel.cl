inline float ag_sanitize(float v){ uint i=as_uint(v); return ((i & 0x7F800000u)==0x7F800000u) ? 0.0f : clamp(v,-1000000.0f,1000000.0f); }
inline float ag_expclamp(float x){ return exp(clamp(ag_sanitize(x),-20.0f,20.0f)); }
inline float ag_logabs(float x){ return log(fabs(ag_sanitize(x))+1e-9f); }
inline float ag_sdiv(float a,float b){ return ag_sanitize(a)/(fabs(ag_sanitize(b))+1e-6f); }
inline float ag_kernel(float x){ x=ag_sanitize(x); return ag_sanitize((float)(((ag_logabs(sin(cos(ag_sdiv(x,x)))) - tanh(ag_sdiv((sin(ag_sdiv(x,x)) * -0.357064),(ag_logabs(sin(cos(ag_sdiv(x,x)))) - ag_logabs(sin(cos(ag_sdiv(x,x)))))))) - sin((ag_logabs(sin(cos(ag_sdiv(x,x)))) * (sin(cos(ag_sdiv(x,x))) + cos(sin(ag_sdiv(x,x))))))))); }
__kernel void transform_scalar(__global const float* xs,__global float* ys,const uint n){ uint i=get_global_id(0); if(i<n) ys[i]=ag_kernel(xs[i]); }
