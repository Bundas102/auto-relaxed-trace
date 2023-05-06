/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#pragma once
#include "Falcor.h"

#include "Utils/ComputeProgramWrapper.h"
#include "Utils/GraphicsProgramWrapper.h"

#include "SDF.h"

#include <unordered_map>

using namespace Falcor;

class SDFRenderer : public IRenderer
{
public:
    void onLoad(RenderContext* pRenderContext) override;
    void onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo) override;
    void onShutdown() override;
    void onResizeSwapChain(uint32_t width, uint32_t height) override;
    bool onKeyEvent(const KeyboardEvent& keyEvent) override;
    bool onMouseEvent(const MouseEvent& mouseEvent) override;
    void onHotReload(HotReloadFlags reloaded) override;
    void onGuiRender(Gui* pGui) override;

    friend struct ConvergenceTester;
    struct ConvergenceTester {
        struct Result {
            uint stepNum = 0;
            uint nonConvergedCount = 0;
            uint convergedHitCount = 0;
            uint convergedMissCount = 0;
            friend std::ostream& operator<<(std::ostream& os, const Result& r);
        };
        enum class TestState { NotTesting, Running, Paused, Ended };
        ConvergenceTester(SDFRenderer& app) : app(app) {}
        SDFRenderer& app;
        TestState testState = TestState::NotTesting;
        uint currentStepNum = 1;
        uint startStepNum = 1;
        uint endStepNum = 200;
        std::vector<Result> results;
        void startTest(uint minNum, uint maxNum);
        void pauseTest();
        void resumeTest();
        void startFrame();
        void endFrame();
        void printResults(std::ostream& os);
        void renderGui(Gui::Widgets& w);
    };
    friend struct PerformanceTester;
    struct PerformanceTester
    {
        struct Result {
            float param;
            Profiler::Stats stats;
            friend std::ostream& operator<<(std::ostream& os, const Result& r);
        };
        enum class TestState { NotTesting, Starting, Running, Ended };
        PerformanceTester(SDFRenderer& app) : app(app) {}
        SDFRenderer& app;
        // state
        TestState testState = TestState::NotTesting;
        uint currFrame = 0u;
        std::vector<float> rawTimes;
        float* param = nullptr;
        std::vector<Result> results;
        std::string testName;
        // settings
        uint numTestFrames = 100u;
        float startParam = 0.0f;
        float endParam = 1.0f;
        float paramStep = 0.1f;

        bool startTest();
        void stopTest(); // stop the test early
        void startFrame();
        void endFrame();
        void printResults(std::ostream& os);
        void renderGui(Gui::Widgets& w);
    };


    // In each frame, we process an input voxel until we iterate over the entire input.
    // Then we proceed to the next output voxel with the first input voxel. 
    struct SDF_Generation_State
    {
        uint3 inputResolution{ 1,1,1 }; // the total input size to iterate over
        uint3 outputResolution{ 1,1,1 }; // the total output size to populate

        uint3 inputVoxelSize{ 32,32,32 }; // the size of the input voxels we iterate over in one call
        uint3 outputVoxelSize{ 32,32,32 }; // the size of the output voxels we populuate in one call

        uint3 inputDispatchSize{ 1, 1, 1 }; // how many input voxels we have to iterate over
        uint64_t inputDispatchCount = 0; // how many input voxels we have to iterate over
        uint3 outputDispatchSize{ 1, 1, 1 }; // how many output voxels we have to populate
        uint64_t outputDispatchCount = 0; // how many output voxels we have to populate

        uint64_t inputDispatchIndex = 0; // linear index of the currently processed input voxel
        uint64_t outputDispatchIndex = 0; // linear index of the currently processed output voxel

        void renderGui(Gui::Widgets& w, float* state) const;
        static SDF_Generation_State create(uint3 outputVoxelSize, uint3 outputRes, uint inputVoxelSize, uint inputRes);
        static SDF_Generation_State create(uint3 outputVoxelSize, uint3 outputRes, uint2 inputVoxelSize, uint2 inputRes);
        static SDF_Generation_State create(uint3 outputVoxelSize, uint3 outputRes, uint3 inputVoxelSize, uint3 inputRes);
    };
    struct SDF_GenerationRunParams
    {
        uint32_t mipLevel{ 0 };
        uint3 outputRes{ 1,1,1 };
        SDF_Generation_Desc genDesc;
    };
    struct DebugUtils {
        bool doSaveDepthToTexture = false;
        bool doCountConvergence = false;
        Texture::SharedPtr debugTexture;

        uint nonConvergedCount = 0;
        uint convergedHitCount = 0;
        uint convergedMissCount = 0;

        void renderGui(Gui::Widgets& w);
    };
    struct ProgramState {
        // trace program
        GraphicsProgramWrapper::SharedPtr mpActiveTraceProg;
        // SDF
        std::shared_ptr<SDF> mpSDF = nullptr;
        // SDF generator compute program
        ComputeProgramWrapper::SharedPtr mpLastGenProg;
        std::chrono::high_resolution_clock::time_point mGenStartTime = std::chrono::high_resolution_clock::now();

        Render_Settings mRendSettings;

        SDF_Generation_Desc mGenSettings;
        SDF_GenerationRunParams mIteratedDispatchParams;
        bool mDoGenerateSDF = false;

        SDF_Generation_State mGenState{};

        SDF_TraceProgram_Desc mTraceProgramSettings;
        bool mDoMakeTraceProgram = false;


        DebugUtils mDebug{};

        // Methods
        void RenderGUI(SDFRenderer& app, Gui::Window& w);

        bool RenderSDF(SDFRenderer& app, RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo);
        bool RenderBB(SDFRenderer& app, RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo);

        bool GenerateFieldChunk(SDFRenderer& app, RenderContext* pContext);
        void PostProcess(SDFRenderer& app, RenderContext* pContext);

        std::shared_ptr<SDF> generateSDF(SDFRenderer& app, RenderContext* pContext, const SDF_Generation_Desc& genDesc);

        std::string getModelAndSettingsString();
    };

private:

    std::vector<ProgramState> mStates;
    uint mCurrStateIdx = 0;
    ProgramState& state() { return mStates[mCurrStateIdx]; }

    GraphicsProgramWrapper::SharedPtr mpCubeWireProg;

    static ComputeProgramWrapper::SharedPtr createGenProgram(const SDF_Generation_Desc& genDesc);
    static GraphicsProgramWrapper::SharedPtr createTraceProgram(const SDF_TraceProgram_Desc& sdfType);
    void setActiveTraceProgram(const SDF_TraceProgram_Desc& sdfType);

    bool runGenProgram( RenderContext* pContext,
                        ComputeProgramWrapper& comp,
                        const UnorderedAccessView::SharedPtr& destTexture,
                        const UnorderedAccessView::SharedPtr& auxTexture,
                        uint3 res,
                        const SDF_Generation_Desc& genDesc );

    // camera
    Camera::SharedPtr mpCamera = nullptr;
    CameraController::SharedPtr mpCameraController = nullptr;
    uint32_t mCameraIndex = 1;
    static CameraController::SharedPtr createCameraController(uint32_t camIndex, const Camera::SharedPtr& pCam, const BBox& box);
    CameraPositionList mCameraPositionsList;
    bool mSetCameraOnGeneration = true;
    uint2 mScreenSize{ 1920u, 1080u };

    ProceduralSDFList mProceduralSDFList;

    Sampler::SharedPtr mpPointSampler;
    Sampler::SharedPtr mpLinearSampler;

    ConvergenceTester mConvTester{ *this };
    PerformanceTester mPerfTester{ *this };

    float3 mBackgroundColor{ 1.f };
};
