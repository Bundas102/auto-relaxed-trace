#include "ComputeProgramWrapper.h"

ref<ComputeProgramWrapper> ComputeProgramWrapper::create(const ref<Device>& pDevice)
{
    return ref<ComputeProgramWrapper>(new ComputeProgramWrapper(pDevice));
}

void ComputeProgramWrapper::createProgram(
    const std::filesystem::path& path,
    const std::string& entry,
    const DefineList& programDefines,
    SlangCompilerFlags flags,
    ShaderModel shaderModel,
    bool createShaderVars
)
{
    // Create program.
    mpProgram = Program::createCompute(mpDevice, path, entry, programDefines, flags, shaderModel);
    mpState = ComputeState::create(mpDevice);
    mpState->setProgram(mpProgram);

    // Create vars unless it should be deferred.
    if (createShaderVars)
        createVars();
}

void ComputeProgramWrapper::createProgram(const ProgramDesc& desc, const DefineList& programDefines, bool createShaderVars)
{
    // Create program.
    mpProgram = Program::create(mpDevice, desc, programDefines);
    mpState = ComputeState::create(mpDevice);
    mpState->setProgram(mpProgram);

    // Create vars unless it should be deferred.
    if (createShaderVars)
        createVars();
}

void ComputeProgramWrapper::createVars()
{
    // Create shader variables.
    const ref<const ProgramReflection>& pReflection = mpProgram->getReflector();
    mpVars = ProgramVars::create(mpDevice, pReflection);
    assert(mpVars);

    // Try to use shader reflection to query thread group size.
    // ((1,1,1) is assumed if it's not specified.)
    mThreadGroupSize = pReflection->getThreadGroupSize();
    assert(mThreadGroupSize.x >= 1 && mThreadGroupSize.y >= 1 && mThreadGroupSize.z >= 1);
}

void ComputeProgramWrapper::allocateStructuredBuffer(const std::string& name, uint32_t nElements, const void* pInitData, size_t initDataSize)
{
    FALCOR_CHECK(mpVars != nullptr, "Program vars not created");
    mStructuredBuffers[name] = mpDevice->createStructuredBuffer(mpVars->getRootVar()[name], nElements);
    if (pInitData)
    {
        ref<Buffer> buffer = mStructuredBuffers[name];
        size_t expectedDataSize = buffer->getStructSize() * buffer->getElementCount();
        if (initDataSize == 0)
            initDataSize = expectedDataSize;
        else if (initDataSize != expectedDataSize)
            throw std::runtime_error("StructuredBuffer '" + name + "' initial data size mismatch");
        buffer->setBlob(pInitData, 0, initDataSize);
    }
}

void ComputeProgramWrapper::runProgram(const uint3& dimensions)
{
    FALCOR_CHECK(mpVars != nullptr, "Program vars not created");
    for (const auto& buffer : mStructuredBuffers)
    {
        mpVars->setBuffer(buffer.first, buffer.second);
    }

    uint3 groups = div_round_up(dimensions, mThreadGroupSize);

    // Check dispatch dimensions.
    if (any(groups > mpDevice->getLimits().maxComputeDispatchThreadGroups))
    {
        throw std::runtime_error("ComputeProgramWrapper::runProgram() - Dispatch dimension exceeds maximum.");
    }

    mpDevice->getRenderContext()->dispatch(mpState.get(), mpVars.get(), groups);
}

void ComputeProgramWrapper::unmapBuffer(const char* bufferName)
{
    assert(mStructuredBuffers.find(bufferName) != mStructuredBuffers.end());
    mStructuredBuffers[bufferName]->unmap();
}

const void* ComputeProgramWrapper::mapRawRead(const char* bufferName)
{
    assert(mStructuredBuffers.find(bufferName) != mStructuredBuffers.end());
    if (mStructuredBuffers.find(bufferName) == mStructuredBuffers.end())
    {
        throw std::runtime_error(std::string(bufferName) + ": couldn't find buffer to map");
    }
    return mStructuredBuffers[bufferName]->map();
}
