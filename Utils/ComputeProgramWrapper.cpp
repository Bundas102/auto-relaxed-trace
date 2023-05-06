#include "ComputeProgramWrapper.h"


void ComputeProgramWrapper::createProgram(const std::filesystem::path& path,
    const std::string& entry,
    const Program::DefineList& programDefines,
    Shader::CompilerFlags flags,
    const std::string& shaderModel,
    bool createShaderVars)
{
    // Create program.
    mpProgram = ComputeProgram::createFromFile(path, entry, programDefines, flags, shaderModel);
    mpState = ComputeState::create();
    mpState->setProgram(mpProgram);

    // Create vars unless it should be deferred.
    if (createShaderVars) createVars();
}

void ComputeProgramWrapper::createVars()
{
    // Create shader variables.
    ProgramReflection::SharedConstPtr pReflection = mpProgram->getReflector();
    mpVars = ComputeVars::create(pReflection);
    assert(mpVars);

    // Try to use shader reflection to query thread group size.
    // ((1,1,1) is assumed if it's not specified.)
    mThreadGroupSize = pReflection->getThreadGroupSize();
    assert(mThreadGroupSize.x >= 1 && mThreadGroupSize.y >= 1 && mThreadGroupSize.z >= 1);
}

void ComputeProgramWrapper::allocateStructuredBuffer(const std::string& name, uint32_t nElements, const void* pInitData, size_t initDataSize)
{
    assert(mpVars);
    mStructuredBuffers[name].pBuffer = Buffer::createStructured(mpProgram.get(), name, nElements);
    assert(mStructuredBuffers[name].pBuffer);
    if (pInitData)
    {
        size_t expectedDataSize = mStructuredBuffers[name].pBuffer->getStructSize() * mStructuredBuffers[name].pBuffer->getElementCount();
        if (initDataSize == 0) initDataSize = expectedDataSize;
        else if (initDataSize != expectedDataSize) std::runtime_error("StructuredBuffer '" + name + "' initial data size mismatch");
        mStructuredBuffers[name].pBuffer->setBlob(pInitData, 0, initDataSize);
    }
}

void ComputeProgramWrapper::runProgram(RenderContext* pContext, const uint3& dimensions)
{
    assert(mpVars);
    for (const auto& buffer : mStructuredBuffers)
    {
        mpVars->setBuffer(buffer.first, buffer.second.pBuffer);
    }

    uint3 groups = div_round_up(dimensions, mThreadGroupSize);

#ifdef FALCOR_D3D12
    // Check dispatch dimensions. TODO: Should be moved into Falcor.
    if (groups.x > D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION ||
        groups.y > D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION ||
        groups.z > D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION)
    {
        throw std::runtime_error("ComputeProgramWrapper::runProgram() - Dispatch dimension exceeds maximum.");
    }
#endif  // FALCOR_D3D12

    pContext->dispatch(mpState.get(), mpVars.get(), groups);
}

void ComputeProgramWrapper::unmapBuffer(const char* bufferName)
{
    assert(mStructuredBuffers.find(bufferName) != mStructuredBuffers.end());
    if (!mStructuredBuffers[bufferName].mapped) throw std::runtime_error(std::string(bufferName) + ": buffer not mapped");
    mStructuredBuffers[bufferName].pBuffer->unmap();
    mStructuredBuffers[bufferName].mapped = false;
}

const void* ComputeProgramWrapper::mapRawRead(const char* bufferName)
{
    assert(mStructuredBuffers.find(bufferName) != mStructuredBuffers.end());
    if (mStructuredBuffers.find(bufferName) == mStructuredBuffers.end())
    {
        throw std::runtime_error(std::string(bufferName) + ": couldn't find buffer to map");
    }
    if (mStructuredBuffers[bufferName].mapped) throw std::runtime_error(std::string(bufferName) + ": buffer already mapped");
    mStructuredBuffers[bufferName].mapped = true;
    return mStructuredBuffers[bufferName].pBuffer->map(Buffer::MapType::Read);
}
