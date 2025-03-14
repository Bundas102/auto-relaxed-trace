#pragma once
#include "Falcor.h"

using namespace Falcor;

// mostly copied from Falcor::UnitTest
class  ComputeProgramWrapper: public Object
{
public:
    
    static ref<ComputeProgramWrapper> create(const ref<Device>& pDevice);

    /** createProgram creates a compute program from the source code at the
        given path.  The entrypoint is assumed to be |main()| unless
        otherwise specified with the |csEntry| parameter.  Preprocessor
        defines and compiler flags can also be optionally provided.
    */
    void createProgram(
        const std::filesystem::path& path,
        const std::string& csEntry = "main",
        const DefineList& programDefines = DefineList(),
        SlangCompilerFlags flags = SlangCompilerFlags::None,
        ShaderModel shaderModel = ShaderModel::Unknown,
        bool createShaderVars = true
    );

    /**
     * Create compute program based on program desc and defines.
     */
    void createProgram(const ProgramDesc& desc, const DefineList& programDefines = DefineList(), bool createShaderVars = true);

    /** (Re-)create the shader variables. Call this if vars were not
        created in createProgram() (if createVars = false), or after
        the shader variables have changed through specialization.
    */
    void createVars();

    /** vars returns the ComputeVars for the program for use in binding
        textures, etc.
    */
    ProgramVars& vars()
    {
        assert(mpVars);
        return *mpVars;
    }

    ShaderVar getRootVar() { return mpVars->getRootVar(); }

    /** Get a shader variable that points at the field with the given `name`.
        This is an alias for `vars().getRootVar()[name]`.
    */
    ShaderVar operator[](const std::string& name)
    {
        return vars().getRootVar()[name];
    }

    template<typename T>
    T* mapBuffer(const char* bufferName, typename std::enable_if<std::is_const<T>::value>::type* = 0)
    {
        return reinterpret_cast<T*>(mapRawRead(bufferName));
    }

    /**
     * Read the contents of a structured buffer into a vector.
     */
    template<typename T>
    std::vector<T> readBuffer(const char* bufferName)
    {
        FALCOR_ASSERT(mStructuredBuffers.find(bufferName) != mStructuredBuffers.end());
        auto it = mStructuredBuffers.find(bufferName);
        if (it == mStructuredBuffers.end())
            throw std::runtime_error(std::string(bufferName) + ": couldn't find buffer to map");
        ref<Buffer> buffer = it->second;
        std::vector<T> result = buffer->getElements<T>();
        return result;
    }

    void ComputeProgramWrapper::unmapBuffer(const char* bufferName);

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
    void runProgram(const uint3& dimensions);

    /** runProgram runs the compute program that was specified in
        |createProgram|, where the total number of threads that runs is
        given by the product of the three provided dimensions.
    */
    void runProgram(uint32_t width = 1, uint32_t height = 1, uint32_t depth = 1) { runProgram(uint3(width, height, depth)); }

    /**
     * Returns the current Falcor render device.
     */
    const ref<Device>& getDevice() const { return mpDevice; }

    /**
     * Returns the current Falcor render context.
     */
    RenderContext* getRenderContext() const { return mpDevice->getRenderContext(); }

    /** Returns the program.
    */
    Program* getProgram() const { return mpProgram.get(); }

    /**
     * Returns the program vars.
     */
    ProgramVars* getVars() const { return mpVars.get(); }

private:
    const void* ComputeProgramWrapper::mapRawRead(const char* bufferName);
    ComputeProgramWrapper(const ref<Device>& pDevice) : mpDevice(pDevice) {}
    // Internal state
    ref<Device> mpDevice;
    ref<ComputeState> mpState;
    ref<Program> mpProgram;
    ref<ProgramVars> mpVars;
    uint3 mThreadGroupSize = { 0, 0, 0 };

    std::map<std::string, ref<Buffer>> mStructuredBuffers;
};
