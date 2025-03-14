#include "FlatMesh.h"

bool FlatMesh::initFromMesh(const ref<Device>& pDevice, const ref<TriangleMesh> pMesh)
{
    if (!pMesh) return false;

    const bool CCW = !pMesh->getFrontFaceCW();
    const auto mix = CCW ? std::array<size_t, 3>({ 0, 1, 2 }) : std::array<size_t, 3>({ 0, 2, 1 });
    const auto& indices = pMesh->getIndices();
    if (indices.size() < 3) return false;
    const auto& vertices = pMesh->getVertices();

    minCorner = maxCorner = vertices[indices[0]].position;

    std::vector<float3> tempBuf;
    tempBuf.reserve(indices.size());
    for (size_t i = 0; i < indices.size() - 2; i+=3) {
        for (auto j : mix) {
            tempBuf.push_back(vertices[indices[i + j]].position);
            minCorner = min(tempBuf.back(), minCorner);
            maxCorner = max(tempBuf.back(), maxCorner);
        }
    }
    numTriangles = uint(tempBuf.size() / 3);
    name = pMesh->getName();

    buffer = pDevice->createStructuredBuffer(sizeof(float) * 9, numTriangles, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess, MemoryType::DeviceLocal, tempBuf.data(), false);

    return !!buffer;
}
