#pragma once
#include "Falcor.h"

using namespace Falcor;

// mostly copied from Falcor::GPUUnitTestContext
class  ComputeProgramWrapper
{
public:
    using SharedPtr = ParameterBlockSharedPtr<ComputeProgramWrapper>;

    static SharedPtr create() { return SharedPtr(new ComputeProgramWrapper()); }

    /** createProgram creates a compute program from the source code at the
        given path.  The entrypoint is assumed to be |main()| unless
        otherwise specified with the |csEntry| parameter.  Preprocessor
        defines and compiler flags can also be optionally provided.
    */
    void createProgram(const std::filesystem::path& path,
        const std::string& csEntry = "main",
        const Program::DefineList& programDefines = Program::DefineList(),
        Shader::CompilerFlags flags = Shader::CompilerFlags::None,
        const std::string& shaderModel = "",
        bool createShaderVars = true);

    /** (Re-)create the shader variables. Call this if vars were not
        created in createProgram() (if createVars = false), or after
        the shader variables have changed through specialization.
    */
    void createVars();

    /** vars returns the ComputeVars for the program for use in binding
        textures, etc.
    */
    ComputeVars& vars()
    {
        assert(mpVars);
        return *mpVars;
    }

    ShaderVar getRootVar() const { return mpVars->getRootVar(); }

    /** Get a shader variable that points at the field with the given `name`.
        This is an alias for `vars().getRootVar()[name]`.
    */
    ShaderVar operator[](const std::string& name)
    {
        return vars().getRootVar()[name];
    }

    /** allocateStructuredBuffer is a helper method that allocates a
        structured buffer of the given name with the given number of
        elements.  Note: the given structured buffer must be declared at
        global scope.

        \param[in] name Name of the buffer in the shader.
        \param[in] nElements Number of elements to allocate.
        \param[in] pInitData Optional parameter. Initial buffer data.
        \param[in] initDataSize Optional parameter. Size of the pointed initial data for validation (if 0 the buffer is assumed to be of the right size).
    */
    void allocateStructuredBuffer(const std::string& name, uint32_t nElements, const void* pInitData = nullptr, size_t initDataSize = 0);

    /** runProgram runs the compute program that was specified in
        |createProgram|, where the total number of threads that runs is
        given by the product of the three provided dimensions.
        \param[in] dimensions Number of threads to dispatch in each dimension.
    */
    void runProgram(RenderContext* pContext, const uint3& dimensions);

    /** runProgram runs the compute program that was specified in
        |createProgram|, where the total number of threads that runs is
        given by the product of the three provided dimensions.
    */
    void runProgram(RenderContext* pContext, uint32_t width = 1, uint32_t height = 1, uint32_t depth = 1) { runProgram(pContext, uint3(width, height, depth)); }

    /** mapBuffer returns a pointer to the named structured buffer.
        Returns nullptr if no such buffer exists.  SFINAE is used to
        require that a the requested pointer is const.
    */
    template <typename T> T* mapBuffer(const char* bufferName,
        typename std::enable_if<std::is_const<T>::value>::type* = 0)
    {
        return reinterpret_cast<T*>(mapRawRead(bufferName));
    }

    /** unmapBuffer unmaps a buffer after it's been used after a call to
        |mapBuffer()|.
    */
    void unmapBuffer(const char* bufferName);

    /** Returns the program.
    */
    ComputeProgram* getProgram() const { return mpProgram.get(); }

private:
    const void* mapRawRead(const char* bufferName);

    // Internal state
    ComputeState::SharedPtr mpState;
    ComputeProgram::SharedPtr mpProgram;
    ComputeVars::SharedPtr mpVars;
    uint3 mThreadGroupSize = { 0, 0, 0 };

    struct ParameterBuffer
    {
        Buffer::SharedPtr pBuffer;
        bool mapped = false;
    };
    std::map<std::string, ParameterBuffer> mStructuredBuffers;
};
