#pragma once
#include "Falcor.h"

using namespace Falcor;

// see ComputeProgramWrapper
class  GraphicsProgramWrapper
{
public:
    using SharedPtr = ParameterBlockSharedPtr<GraphicsProgramWrapper>;

    static SharedPtr create() { return SharedPtr(new GraphicsProgramWrapper()); }

    void createProgram(const Program::Desc& desc,
        const Program::DefineList& programDefines = Program::DefineList(),
        bool createShaderVars = true);

    void createProgram(const std::filesystem::path& vsPath,
        const std::filesystem::path& psPath,
        const std::string& vsEntry = "main",
        const std::string& psEntry = "main",
        const Program::DefineList& programDefines = Program::DefineList(),
        bool createShaderVars = true);

    void createVars();
    
    ProgramVars& vars()
    {
        assert(mpVars);
        return *mpVars;
    }

    ShaderVar getRootVar() const { return mpVars->getRootVar(); }

    ShaderVar operator[](const std::string& name)
    {
        return vars().getRootVar()[name];
    }

    void allocateStructuredBuffer(const std::string& name, uint32_t nElements, const void* pInitData = nullptr, size_t initDataSize = 0);

    Vao::SharedConstPtr getVao() const { return mpState->getVao(); }
    void setVao(const Vao::SharedConstPtr& pVao) { mpState->setVao(pVao); }

    Fbo::SharedPtr getFbo() const { return mpState->getFbo(); }
    void setFbo(const Fbo::SharedPtr& pFbo, bool setVp0Sc0 = true) { mpState->setFbo(pFbo, setVp0Sc0); }

    void draw(RenderContext* pContext, const Fbo::SharedPtr& pFbo, uint32_t vertexCount, uint32_t startVertexLocation = 0);

    void drawIndexed(RenderContext* pContext, const Fbo::SharedPtr& pFbo, uint32_t indexCount, uint32_t startIndexLocation = 0, int32_t baseVertexLocation = 0);

    template <typename T> T* mapBuffer(const char* bufferName,
        typename std::enable_if<std::is_const<T>::value>::type* = 0)
    {
        return reinterpret_cast<T*>(mapRawRead(bufferName));
    }

    void unmapBuffer(const char* bufferName);
    
    GraphicsProgram* getProgram() const { return mpProgram.get(); }

    GraphicsState* getState() const { return mpState.get(); }

private:
    const void* mapRawRead(const char* bufferName);
    void setBuffers();

    // Internal state
    GraphicsProgram::SharedPtr mpProgram = nullptr;
    GraphicsVars::SharedPtr    mpVars = nullptr;
    GraphicsState::SharedPtr   mpState = nullptr;

    struct ParameterBuffer
    {
        Buffer::SharedPtr pBuffer;
        bool mapped = false;
    };
    std::map<std::string, ParameterBuffer> mStructuredBuffers;
};
