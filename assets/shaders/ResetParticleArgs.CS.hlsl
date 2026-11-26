#include "GPUParticle.hlsli"

RWStructuredBuffer<uint32_t> gIndirectArgs : register(u5);

[numthreads(1, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID) {
    if (DTid.x == 0u) {
        // Bucket 0: Normal Alpha
        gIndirectArgs[0] = 4u; // VertexCountPerInstance
        gIndirectArgs[1] = 0u; // InstanceCount
        gIndirectArgs[2] = 0u; // StartVertexLocation
        gIndirectArgs[3] = 0u; // StartInstanceLocation

        // Bucket 1: Additive HDR
        gIndirectArgs[4] = 4u; // VertexCountPerInstance
        gIndirectArgs[5] = 0u; // InstanceCount
        gIndirectArgs[6] = 0u; // StartVertexLocation
        gIndirectArgs[7] = 0u; // StartInstanceLocation
    }
}
