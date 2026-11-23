#include "GPUParticle.hlsli"

RWStructuredBuffer<uint32_t> gIndirectArgs : register(u4);

[numthreads(1, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID) {
    if (DTid.x == 0u) {
        gIndirectArgs[0] = 4u; // VertexCountPerInstance
        gIndirectArgs[1] = 0u; // InstanceCount
        gIndirectArgs[2] = 0u; // StartVertexLocation
        gIndirectArgs[3] = 0u; // StartInstanceLocation
    }
}
