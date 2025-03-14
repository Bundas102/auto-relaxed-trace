#include "GraphicsProgramWrapper.h"

ref<GraphicsProgramWrapper> GraphicsProgramWrapper::create(const ref<Device>& pDevice)
{
    return ref<GraphicsProgramWrapper>(new GraphicsProgramWrapper(pDevice));
}

void GraphicsProgramWrapper::createProgram(
    const ProgramDesc& desc,
    const DefineList& programDefines,
    bool createShaderVars)
{
    // Create program.
    mpProgram = Program::create(mpDevice, desc, programDefines);
    mpState = GraphicsState::create(mpDevice);
    mpState->setProgram(mpProgram);
    assert(mpState);

    // Create vars unless it should be deferred.
    if (createShaderVars) createVars();
}

void GraphicsProgramWrapper::createProgram(
    const std::filesystem::path& vsPath,
    const std::filesystem::path& psPath,
    const std::string& vsEntry,
    const std::string& psEntry,
    const DefineList& programDefines,
    bool createShaderVars)
{
    ProgramDesc d;
    d.addShaderLibrary(vsPath).vsEntry(vsEntry);
    d.addShaderLibrary(psPath).psEntry(psEntry);
    createProgram(d, programDefines, createShaderVars);
}

void GraphicsProgramWrapper::createVars()
{
    mpVars = ProgramVars::create(mpDevice, mpProgram.get());
    assert(mpVars);
}

void GraphicsProgramWrapper::allocateStructuredBuffer(
    const std::string& name,
    uint32_t nElements,
    const void* pInitData,
    size_t initDataSize
)
{
    FALCOR_CHECK(mpVars != nullptr, "Program vars not created");
    mStructuredBuffers[name] = { mpDevice->createStructuredBuffer(mpVars->getRootVar()[name], nElements), false };
    if (pInitData)
    {
        ref<Buffer> buffer = mStructuredBuffers[name].pBuffer;
        size_t expectedDataSize = buffer->getStructSize() * buffer->getElementCount();
        if (initDataSize == 0)
            initDataSize = expectedDataSize;
        else if (initDataSize != expectedDataSize)
            throw std::runtime_error("StructuredBuffer '" + name + "' initial data size mismatch");
        buffer->setBlob(pInitData, 0, initDataSize);
    }
}

void GraphicsProgramWrapper::draw(RenderContext* pContext, const ref<Fbo>& pFbo, uint32_t vertexCount, uint32_t startVertexLocation)
{
    setBuffers();
    if (pFbo) setFbo(pFbo, true);
    pContext->draw(mpState.get(), mpVars.get(), vertexCount, startVertexLocation);
}

void GraphicsProgramWrapper::drawIndexed(RenderContext* pContext, const ref<Fbo>& pFbo, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation)
{
    setBuffers();
    if (pFbo) setFbo(pFbo, true);
    pContext->drawIndexed(mpState.get(), mpVars.get(), indexCount, startIndexLocation, baseVertexLocation);
}

void GraphicsProgramWrapper::unmapBuffer(const char* bufferName)
{
    assert(mStructuredBuffers.find(bufferName) != mStructuredBuffers.end());
    if (!mStructuredBuffers[bufferName].mapped)
        throw std::runtime_error(std::string(bufferName) + ": buffer not mapped");
    mStructuredBuffers[bufferName].pBuffer->unmap();
    mStructuredBuffers[bufferName].mapped = false;
}

const void* GraphicsProgramWrapper::mapRawRead(const char* bufferName)
{
    assert(mStructuredBuffers.find(bufferName) != mStructuredBuffers.end());
    if (mStructuredBuffers.find(bufferName) == mStructuredBuffers.end())
    {
        throw std::runtime_error(std::string(bufferName) + ": couldn't find buffer to map");
    }
    if (mStructuredBuffers[bufferName].mapped)
        throw std::runtime_error(std::string(bufferName) + ": buffer already mapped");
    mStructuredBuffers[bufferName].mapped = true;
    return mStructuredBuffers[bufferName].pBuffer->map();
}

void GraphicsProgramWrapper::setBuffers()
{
    assert(mpVars);
    for (const auto& buffer : mStructuredBuffers)
    {
        mpVars->setBuffer(buffer.first, buffer.second.pBuffer);
    }
}
