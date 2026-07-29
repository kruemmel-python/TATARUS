#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using cl_int = std::int32_t;
using cl_uint = std::uint32_t;
using cl_ulong = std::uint64_t;
using cl_bitfield = cl_ulong;
using cl_device_type = cl_bitfield;
using cl_mem_flags = cl_bitfield;
using cl_bool = cl_uint;
using cl_platform_id = struct _cl_platform_id*;
using cl_device_id = struct _cl_device_id*;
using cl_context = struct _cl_context*;
using cl_command_queue = struct _cl_command_queue*;
using cl_mem = struct _cl_mem*;
using cl_program = struct _cl_program*;
using cl_kernel = struct _cl_kernel*;

constexpr cl_int CL_SUCCESS = 0;
constexpr cl_device_type CL_DEVICE_TYPE_ALL = 0xFFFFFFFFULL;
constexpr cl_mem_flags CL_MEM_READ_WRITE = 1ULL;
constexpr cl_mem_flags CL_MEM_READ_ONLY = 1ULL << 2U;
constexpr cl_mem_flags CL_MEM_COPY_HOST_PTR = 1ULL << 5U;
constexpr cl_bool CL_TRUE = 1;
constexpr cl_uint CL_DEVICE_NAME = 0x102B;
constexpr cl_uint CL_PROGRAM_BUILD_LOG = 0x1183;

struct Api {
    HMODULE module = nullptr;
    decltype(&::GetProcAddress) getProc = &::GetProcAddress;
    cl_int (WINAPI* getPlatformIds)(cl_uint, cl_platform_id*, cl_uint*) = nullptr;
    cl_int (WINAPI* getDeviceIds)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*) = nullptr;
    cl_int (WINAPI* getDeviceInfo)(cl_device_id, cl_uint, std::size_t, void*, std::size_t*) = nullptr;
    cl_context (WINAPI* createContext)(const void*, cl_uint, const cl_device_id*,
        void (WINAPI*)(const char*, const void*, std::size_t, void*), void*, cl_int*) = nullptr;
    cl_command_queue (WINAPI* createCommandQueue)(cl_context, cl_device_id, cl_bitfield, cl_int*) = nullptr;
    cl_program (WINAPI* createProgramWithSource)(cl_context, cl_uint, const char**, const std::size_t*, cl_int*) = nullptr;
    cl_int (WINAPI* buildProgram)(cl_program, cl_uint, const cl_device_id*, const char*,
        void (WINAPI*)(cl_program, void*), void*) = nullptr;
    cl_int (WINAPI* getProgramBuildInfo)(cl_program, cl_device_id, cl_uint, std::size_t, void*, std::size_t*) = nullptr;
    cl_kernel (WINAPI* createKernel)(cl_program, const char*, cl_int*) = nullptr;
    cl_mem (WINAPI* createBuffer)(cl_context, cl_mem_flags, std::size_t, void*, cl_int*) = nullptr;
    cl_int (WINAPI* setKernelArg)(cl_kernel, cl_uint, std::size_t, const void*) = nullptr;
    cl_int (WINAPI* enqueueKernel)(cl_command_queue, cl_kernel, cl_uint, const std::size_t*,
        const std::size_t*, const std::size_t*, cl_uint, const void*, void*) = nullptr;
    cl_int (WINAPI* enqueueRead)(cl_command_queue, cl_mem, cl_bool, std::size_t,
        std::size_t, void*, cl_uint, const void*, void*) = nullptr;
    cl_int (WINAPI* finish)(cl_command_queue) = nullptr;
    cl_int (WINAPI* releaseMem)(cl_mem) = nullptr;
    cl_int (WINAPI* releaseKernel)(cl_kernel) = nullptr;
    cl_int (WINAPI* releaseProgram)(cl_program) = nullptr;
    cl_int (WINAPI* releaseQueue)(cl_command_queue) = nullptr;
    cl_int (WINAPI* releaseContext)(cl_context) = nullptr;

    template<typename T>
    void load(T& target, const char* name) {
        target = reinterpret_cast<T>(getProc(module, name));
        if (!target) throw std::runtime_error(std::string("OpenCL symbol missing: ") + name);
    }

    Api() {
        module = LoadLibraryW(L"OpenCL.dll");
        if (!module) throw std::runtime_error("OpenCL.dll not installed");
        load(getPlatformIds, "clGetPlatformIDs");
        load(getDeviceIds, "clGetDeviceIDs");
        load(getDeviceInfo, "clGetDeviceInfo");
        load(createContext, "clCreateContext");
        load(createCommandQueue, "clCreateCommandQueue");
        load(createProgramWithSource, "clCreateProgramWithSource");
        load(buildProgram, "clBuildProgram");
        load(getProgramBuildInfo, "clGetProgramBuildInfo");
        load(createKernel, "clCreateKernel");
        load(createBuffer, "clCreateBuffer");
        load(setKernelArg, "clSetKernelArg");
        load(enqueueKernel, "clEnqueueNDRangeKernel");
        load(enqueueRead, "clEnqueueReadBuffer");
        load(finish, "clFinish");
        load(releaseMem, "clReleaseMemObject");
        load(releaseKernel, "clReleaseKernel");
        load(releaseProgram, "clReleaseProgram");
        load(releaseQueue, "clReleaseCommandQueue");
        load(releaseContext, "clReleaseContext");
    }
    ~Api() { if (module) FreeLibrary(module); }
};

void check(cl_int status, const char* operation) {
    if (status != CL_SUCCESS) {
        throw std::runtime_error(std::string(operation) + " failed with " + std::to_string(status));
    }
}

std::string deviceName(Api& api, cl_device_id device) {
    std::size_t size = 0;
    check(api.getDeviceInfo(device, CL_DEVICE_NAME, 0, nullptr, &size), "clGetDeviceInfo");
    std::string name(size, '\0');
    check(api.getDeviceInfo(device, CL_DEVICE_NAME, size, name.data(), nullptr), "clGetDeviceInfo");
    while (!name.empty() && name.back() == '\0') name.pop_back();
    return name;
}

struct Result {
    bool ran = false;
    int platforms = 0;
    int devices = 0;
    std::string device;
    int samples = 0;
    double maximumError = 0.0;
    std::string message;
};

Result runProbe() {
    Result result;
    Api api;
    cl_uint platformCount = 0;
    check(api.getPlatformIds(0, nullptr, &platformCount), "clGetPlatformIDs");
    result.platforms = static_cast<int>(platformCount);
    if (platformCount == 0) throw std::runtime_error("no OpenCL platforms");
    std::vector<cl_platform_id> platforms(platformCount);
    check(api.getPlatformIds(platformCount, platforms.data(), nullptr), "clGetPlatformIDs");
    cl_device_id device = nullptr;
    for (const auto platform : platforms) {
        cl_uint count = 0;
        if (api.getDeviceIds(platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &count) != CL_SUCCESS) continue;
        result.devices += static_cast<int>(count);
        if (!device && count) {
            std::vector<cl_device_id> devices(count);
            check(api.getDeviceIds(platform, CL_DEVICE_TYPE_ALL, count, devices.data(), nullptr), "clGetDeviceIDs");
            device = devices.front();
        }
    }
    if (!device) throw std::runtime_error("no OpenCL devices");
    result.device = deviceName(api, device);

    cl_int status = 0;
    cl_context context = api.createContext(nullptr, 1, &device, nullptr, nullptr, &status);
    check(status, "clCreateContext");
    cl_command_queue queue = api.createCommandQueue(context, device, 0, &status);
    check(status, "clCreateCommandQueue");
    const char* source =
        "__kernel void integrate(__global const float* v,__global const float* current,"
        "__global float* out,float dt,float rest,float tau){"
        "size_t i=get_global_id(0);out[i]=v[i]+dt*((rest-v[i]+current[i])/tau);}";
    cl_program program = api.createProgramWithSource(context, 1, &source, nullptr, &status);
    check(status, "clCreateProgramWithSource");
    status = api.buildProgram(program, 1, &device, nullptr, nullptr, nullptr);
    if (status != CL_SUCCESS) {
        std::size_t logSize = 0;
        api.getProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
        std::string log(logSize, '\0');
        api.getProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, log.data(), nullptr);
        throw std::runtime_error("OpenCL build failed: " + log);
    }
    cl_kernel kernel = api.createKernel(program, "integrate", &status);
    check(status, "clCreateKernel");
    constexpr std::size_t count = 512;
    std::vector<float> voltage(count), current(count), gpu(count), cpu(count);
    for (std::size_t i = 0; i < count; ++i) {
        voltage[i] = -72.0F + 25.0F * static_cast<float>(i) / static_cast<float>(count);
        current[i] = 5.0F * std::sin(static_cast<float>(i) * 0.071F);
        cpu[i] = voltage[i] + 1.0F * ((-65.0F - voltage[i] + current[i]) / 20.0F);
    }
    const auto bytes = count * sizeof(float);
    cl_mem vBuffer = api.createBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        bytes, voltage.data(), &status); check(status, "clCreateBuffer voltage");
    cl_mem iBuffer = api.createBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        bytes, current.data(), &status); check(status, "clCreateBuffer current");
    cl_mem oBuffer = api.createBuffer(context, CL_MEM_READ_WRITE, bytes, nullptr, &status);
    check(status, "clCreateBuffer output");
    const float dt = 1.0F, rest = -65.0F, tau = 20.0F;
    check(api.setKernelArg(kernel, 0, sizeof(vBuffer), &vBuffer), "clSetKernelArg");
    check(api.setKernelArg(kernel, 1, sizeof(iBuffer), &iBuffer), "clSetKernelArg");
    check(api.setKernelArg(kernel, 2, sizeof(oBuffer), &oBuffer), "clSetKernelArg");
    check(api.setKernelArg(kernel, 3, sizeof(dt), &dt), "clSetKernelArg");
    check(api.setKernelArg(kernel, 4, sizeof(rest), &rest), "clSetKernelArg");
    check(api.setKernelArg(kernel, 5, sizeof(tau), &tau), "clSetKernelArg");
    check(api.enqueueKernel(queue, kernel, 1, nullptr, &count, nullptr, 0, nullptr, nullptr),
        "clEnqueueNDRangeKernel");
    check(api.enqueueRead(queue, oBuffer, CL_TRUE, 0, bytes, gpu.data(), 0, nullptr, nullptr),
        "clEnqueueReadBuffer");
    check(api.finish(queue), "clFinish");
    for (std::size_t i = 0; i < count; ++i) {
        result.maximumError = std::max(result.maximumError,
            std::abs(static_cast<double>(gpu[i] - cpu[i])));
    }
    api.releaseMem(oBuffer); api.releaseMem(iBuffer); api.releaseMem(vBuffer);
    api.releaseKernel(kernel); api.releaseProgram(program); api.releaseQueue(queue);
    api.releaseContext(context);
    result.ran = true;
    result.samples = static_cast<int>(count);
    result.message = result.maximumError <= 2.0e-5 ? "CPU/OpenCL differential PASS" : "CPU/OpenCL differential FAIL";
    return result;
}

void writeJson(const std::filesystem::path& path, const Result& r) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << "{\n  \"status\": \"" << (r.ran ? "executed" : "not_run") << "\",\n"
        << "  \"platforms\": " << r.platforms << ",\n"
        << "  \"devices\": " << r.devices << ",\n"
        << "  \"device\": \"" << r.device << "\",\n"
        << "  \"samples\": " << r.samples << ",\n"
        << "  \"maximum_absolute_error\": " << r.maximumError << ",\n"
        << "  \"message\": \"" << r.message << "\"\n}\n";
}

} // namespace

int main(int argc, char** argv) {
    Result result;
    try {
        result = runProbe();
    } catch (const std::exception& error) {
        result.message = error.what();
    }
    const std::filesystem::path output = argc > 1 ? argv[1] : "opencl_probe.json";
    writeJson(output, result);
    std::cout << result.message << '\n';
    return result.ran && result.maximumError <= 2.0e-5 ? 0 : 2;
}
