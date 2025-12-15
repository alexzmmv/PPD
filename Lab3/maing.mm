// Compile:
//   clang++ -std=c++17 -ObjC++ -framework Foundation -framework Metal -o maing maing.cpp
// If the compiler requires an Objective-C++ extension, rename to maing.mm and run:
//   clang++ -std=c++17 -ObjC++ -framework Foundation -framework Metal -o maing maing.mm
// Run:
//   ./maing <n> <m> <p> <threads> gpu
// Example:
//   ./maing 512 512 512 1 gpu

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <chrono>

using namespace std;

static void usage(const char *prog) {
    cerr << "Usage: " << (prog ? prog : "prog") << " n m p threads strategy\n";
    cerr << "  A is n x m, B is m x p, C is n x p\n";
    cerr << "  strategy: gpu  (runs on Metal GPU)\n";
}

static void fill_matrices_float(vector<float> &A, vector<float> &B, int n, int m, int p) {
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < m; ++k)
            A[i * m + k] = 1.0f;
    for (int k = 0; k < m; ++k)
        for (int j = 0; j < p; ++j)
            B[k * p + j] = 1.0f;
}

int main(int argc, char **argv) {
    @autoreleasepool {
        ios::sync_with_stdio(true);
        if (argc < 6) {
            usage(argv[0]);
            return 1;
        }
        int n = atoi(argv[1]);
        int m = atoi(argv[2]);
        int p = atoi(argv[3]);
        int T = atoi(argv[4]); // kept for compatibility, not used by GPU path
        string strat = argv[5] ? argv[5] : "";

        if (n <= 0 || m <= 0 || p <= 0) {
            usage(argv[0]);
            return 1;
        }

        const int total = n * p;
        vector<float> A(static_cast<size_t>(n) * m);
        vector<float> B(static_cast<size_t>(m) * p);
        vector<float> C(static_cast<size_t>(n) * p, 0.0f);

        fill_matrices_float(A, B, n, m, p);

        if (strat == "gpu") {
            // Metal kernel source (simple 1-element-per-thread, float)
            const char *metal_src =
                "using namespace metal;\n"
                "kernel void matmul(\n"
                "    device const float *A [[ buffer(0) ]],\n"
                "    device const float *B [[ buffer(1) ]],\n"
                "    device float *C [[ buffer(2) ]],\n"
                "    constant uint &n [[ buffer(3) ]],\n"
                "    constant uint &m [[ buffer(4) ]],\n"
                "    constant uint &p [[ buffer(5) ]],\n"
                "    uint gid [[ thread_position_in_grid ]]) {\n"
                "    uint idx = gid;\n"
                "    if (idx >= (n * p)) return;\n"
                "    uint i = idx / p;\n"
                "    uint j = idx % p;\n"
                "    float sum = 0.0f;\n"
                "    for (uint k = 0; k < m; ++k) {\n"
                "        sum += A[i * m + k] * B[k * p + j];\n"
                "    }\n"
                "    C[i * p + j] = sum;\n"
                "}\n";

            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            if (!device) {
                cerr << "No Metal device found.\n";
                return 1;
            }

            NSError *err = nil;
            NSString *src = [NSString stringWithUTF8String:metal_src];
            id<MTLLibrary> library = [device newLibraryWithSource:src options:nil error:&err];
            if (!library) {
                cerr << "Failed to compile Metal shader: " << (err ? [[err localizedDescription] UTF8String] : "unknown") << "\n";
                return 1;
            }
            id<MTLFunction> function = [library newFunctionWithName:@"matmul"];
            if (!function) {
                cerr << "Failed to get function 'matmul'\n";
                return 1;
            }
            id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&err];
            if (!pipeline) {
                cerr << "Failed to create pipeline: " << (err ? [[err localizedDescription] UTF8String] : "unknown") << "\n";
                return 1;
            }

            // Create shared buffers
            id<MTLBuffer> bufA = [device newBufferWithBytes:A.data() length:sizeof(float)*A.size() options:MTLResourceStorageModeShared];
            id<MTLBuffer> bufB = [device newBufferWithBytes:B.data() length:sizeof(float)*B.size() options:MTLResourceStorageModeShared];
            id<MTLBuffer> bufC = [device newBufferWithLength:sizeof(float)*C.size() options:MTLResourceStorageModeShared];

            // small constant buffers via setBytes
            uint32_t nu = static_cast<uint32_t>(n);
            uint32_t mu = static_cast<uint32_t>(m);
            uint32_t pu = static_cast<uint32_t>(p);

            id<MTLCommandQueue> queue = [device newCommandQueue];
            if (!queue) {
                cerr << "Failed to create command queue\n";
                return 1;
            }

            auto tstart = chrono::high_resolution_clock::now();
            id<MTLCommandBuffer> cmd = [queue commandBuffer];
            id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
            [enc setComputePipelineState:pipeline];
            [enc setBuffer:bufA offset:0 atIndex:0];
            [enc setBuffer:bufB offset:0 atIndex:1];
            [enc setBuffer:bufC offset:0 atIndex:2];
            [enc setBytes:&nu length:sizeof(nu) atIndex:3];
            [enc setBytes:&mu length:sizeof(mu) atIndex:4];
            [enc setBytes:&pu length:sizeof(pu) atIndex:5];

            // Dispatch one thread per element
            MTLSize grid = MTLSizeMake((size_t)total, 1, 1);
            // choose threadgroup size (query device/pipeline if needed)
            uint32_t threadsPerThreadgroup = 256;
            if (threadsPerThreadgroup > pipeline.maxTotalThreadsPerThreadgroup)
                threadsPerThreadgroup = (uint32_t)pipeline.maxTotalThreadsPerThreadgroup;
            if (threadsPerThreadgroup == 0) threadsPerThreadgroup = 1;
            MTLSize tg = MTLSizeMake((size_t)threadsPerThreadgroup, 1, 1);

            [enc dispatchThreads:grid threadsPerThreadgroup:tg];
            [enc endEncoding];

            [cmd commit];
            [cmd waitUntilCompleted];
            auto tend = chrono::high_resolution_clock::now();

            //current time
            auto tcurrent_memcpy = chrono::high_resolution_clock::now();
            // copy result
            memcpy(C.data(), bufC.contents, sizeof(float)*C.size());
            auto tend_memcpy = chrono::high_resolution_clock::now();
            auto elapsed_memcpy = chrono::duration_cast<chrono::milliseconds>(tend_memcpy - tcurrent_memcpy).count();

            auto elapsed_ms = chrono::duration_cast<chrono::milliseconds>(tend - tstart).count();
            cout << "GPU Time taken: " << elapsed_ms << " ms\n";
            cout << "Memory Copy Time taken: " << elapsed_memcpy << " ms\n";

            // Optional: print first few values
            cout << "Result matrix C (first 10 elements):\n";
            for (int i = 1; i < min(10, n*p); ++i) {
                if(C[i] != C[i-1]) {
                    cout << "Error in computation!\n";
                    break;
                }
            }
            cout << "\n";

            return 0;
        } else {
            cerr << "Strategy not recognized or not 'gpu'. Run with 'gpu' to use Metal.\n";
            return 1;
        }

    }
    return 0;

}
