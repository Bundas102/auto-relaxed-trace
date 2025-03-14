#pragma once

#include "Falcor.h"
#include <Scene/TriangleMesh.h>

using namespace Falcor;


// Stores a mesh as a list of triangles, no indexbuffer.
// Only contains positions, no other attributes.
class FlatMesh {
public:
    bool initFromMesh(const ref<Device>& pDevice, const ref<TriangleMesh> pMesh);
    void reset() { *this = FlatMesh(); }

    std::string name;
    uint numTriangles = 0;

    float3 minCorner{ 0 };
    float3 maxCorner{ 0 };

    ref<Buffer> buffer;
};
