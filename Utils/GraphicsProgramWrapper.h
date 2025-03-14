#pragma once
#include "Falcor.h"

using namespace Falcor;

// see ComputeProgramWrapper
class  GraphicsProgramWrapper : public Object
{
public:
    static ref<GraphicsProgramWrapper> create(const ref<Device>& pDevice);

    void createProgram(const ProgramDesc& desc, 
        const DefineList& programDefines = DefineList(),
        bool createShaderVars = true);

    void createProgram(const std::filesystem::path& vsPath,
        const std::filesystem::path& psPath,
        const std::string& vsEntry = "main",
        const std::string& psEntry = "main",
        const DefineList& programDefines = DefineList(),
        bool createShaderVars = true);

    void createVars();
    
    ProgramVars& vars()
    {
        assert(mpVars);
        return *mpVars;
    }

    ProgramVars* getVars() const { return mpVars.get(); }
    ShaderVar getRootVar() const { return mpVars->getRootVar(); }

    ShaderVar operator[](const std::string& name)
    {
        return vars().getRootVar()[name];
    }

    void allocateStructuredBuffer(const std::string& name, uint32_t nElements, const void* pInitData = nullptr, size_t initDataSize = 0);

    ref<Vao> getVao() const { return mpState->getVao(); }
    void setVao(const ref<Vao> pVao) { mpState->setVao(pVao); }

    ref<Fbo> getFbo() const { return mpState->getFbo(); }
    void setFbo(const ref<Fbo>& pFbo, bool setVp0Sc0 = true) { mpState->setFbo(pFbo, setVp0Sc0); }

    void draw(RenderContext* pContext, const ref<Fbo>& pFbo, uint32_t vertexCount, uint32_t startVertexLocation = 0);

    void drawIndexed(RenderContext* pContext, const ref<Fbo>& pFbo, uint32_t indexCount, uint32_t startIndexLocation = 0, int32_t baseVertexLocation = 0);

    template <typename T> T* mapBuffer(const char* bufferName,
        typename std::enable_if<std::is_const<T>::value>::type* = 0)
    {
        return reinterpret_cast<T*>(mapRawRead(bufferName));
    }

    void unmapBuffer(const char* bufferName);
    
    Program* getProgram() const { return mpProgram.get(); }

    GraphicsState* getState() const { return mpState.get(); }

private:
    GraphicsProgramWrapper(const ref<Device>& pDevice) : mpDevice(pDevice) {}

    const void* mapRawRead(const char* bufferName);
    void setBuffers();

    // Internal state
    ref<Device> mpDevice;
    ref<Program> mpProgram = nullptr;
    ref<ProgramVars> mpVars = nullptr;
    ref<GraphicsState> mpState = nullptr;

    struct ParameterBuffer
    {
        ref<Buffer> pBuffer;
        bool mapped = false;
    };
    std::map<std::string, ParameterBuffer> mStructuredBuffers;
};
