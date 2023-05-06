#include "GraphicsProgramWrapper.h"

void GraphicsProgramWrapper::createProgram(const Program::Desc& desc,
    const Program::DefineList& programDefines,
    bool createShaderVars)
{
    // Create program.
    mpProgram = GraphicsProgram::create(desc, programDefines);
    mpState = GraphicsState::create();
    mpState->setProgram(mpProgram);
    assert(mpState);

    // Create vars unless it should be deferred.
    if (createShaderVars) createVars();
}

void GraphicsProgramWrapper::createProgram(const std::filesystem::path& vsPath,
    const std::filesystem::path& psPath,
    const std::string& vsEntry,
    const std::string& psEntry,
    const Program::DefineList& programDefines,
    bool createShaderVars)
{
    Program::Desc d;
    d.addShaderLibrary(vsPath).vsEntry(vsEntry);
    d.addShaderLibrary(psPath).psEntry(psEntry);
    createProgram(d, programDefines, createShaderVars);
}

void GraphicsProgramWrapper::createVars()
{
    mpVars = GraphicsVars::create(mpProgram.get());
    assert(mpVars);
}

void GraphicsProgramWrapper::allocateStructuredBuffer(const std::string& name, uint32_t nElements, const void* pInitData, size_t initDataSize)
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

void GraphicsProgramWrapper::draw(RenderContext* pContext, const Fbo::SharedPtr& pFbo, uint32_t vertexCount, uint32_t startVertexLocation)
{
    setBuffers();
    if (pFbo) setFbo(pFbo, true);
    pContext->draw(mpState.get(), mpVars.get(), vertexCount, startVertexLocation);
}

void GraphicsProgramWrapper::drawIndexed(RenderContext* pContext, const Fbo::SharedPtr& pFbo, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation)
{
    setBuffers();
    if (pFbo) setFbo(pFbo, true);
    pContext->drawIndexed(mpState.get(), mpVars.get(), indexCount, startIndexLocation, baseVertexLocation);
}

void GraphicsProgramWrapper::unmapBuffer(const char* bufferName)
{
    assert(mStructuredBuffers.find(bufferName) != mStructuredBuffers.end());
    if (!mStructuredBuffers[bufferName].mapped) throw std::runtime_error(std::string(bufferName) + ": buffer not mapped");
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
    if (mStructuredBuffers[bufferName].mapped) throw std::runtime_error(std::string(bufferName) + ": buffer already mapped");
    mStructuredBuffers[bufferName].mapped = true;
    return mStructuredBuffers[bufferName].pBuffer->map(Buffer::MapType::Read);
}

void GraphicsProgramWrapper::setBuffers()
{
    assert(mpVars);
    for (const auto& buffer : mStructuredBuffers)
    {
        mpVars->setBuffer(buffer.first, buffer.second.pBuffer);
    }
}
