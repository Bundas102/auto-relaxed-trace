/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
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
#include "SDFRenderer.h"

#include <chrono>
#include <fstream>

using namespace std::literals::string_literals;

namespace {
const std::filesystem::path kSDir = "Samples/SDFRenderer/Shaders";
std::filesystem::path kProceduralSDFListFile = "";
std::filesystem::path kCameraPositionsFile = "";
const Gui::RadioButtonGroup kCameraRadioButtons = { {0,"Orbiter", false}, {1,"FPS",true} };

// the size of the SDF input voxels we iterate over in one call (32^3)
const uint3 kInputVoxelSize{32, 32, 32};
const uint kInputMeshChunk = 8192;
}

template<typename F>
void GuiGroup(Gui::Widgets& w, const std::string& label, bool beginExpanded, F&& f) {
    auto g = Gui::Group(w, label, beginExpanded);
    if (g.open())
    {
        ImGui::PushID(label.c_str());
        f(g);
        ImGui::PopID();
    }
    w.separator();
}

uint3 index1dTo3d(uint64_t index, uint3 dim) {
    return {
        index % dim.x,
        (index / dim.x) % dim.y,
        index / dim.x / dim.y
    };
}
uint64_t index3dTo1d(uint3 index, uint3 dim) {
    return (uint64_t)index.x +
        (uint64_t)index.y * dim.x +
        (uint64_t)index.z * dim.x * dim.y;
}

void SDFRenderer::SDF_Generation_State::renderGui(Gui::Widgets& w, float* state) const
{
    if (outputDispatchCount != 0) {
        ImGui::Text("Output voxel size: %u x %u x %u", outputVoxelSize.x, outputVoxelSize.y, outputVoxelSize.z);
        ImGui::Text("Input voxel size: %u x %u x %u", inputVoxelSize.x, inputVoxelSize.y, inputVoxelSize.z);
        ImGui::Text("Current output voxel: %u / %u", outputDispatchIndex, outputDispatchCount);
        ImGui::Text("Current input voxel: %u / %u", inputDispatchIndex, inputDispatchCount);
        const uint64_t current = inputDispatchIndex + outputDispatchIndex * inputDispatchCount;
        const uint64_t total = outputDispatchCount * inputDispatchCount;
        const float statePercent = float(current) / total;
        ImGui::Text("Total dispatch: %u / %u,       %.3f%%", current, total, 100.f * statePercent);
        if (state) *state = statePercent;
    }
}

void SDFRenderer::DebugUtils::renderGui(Gui::Widgets& w)
{
    static bool showMsg = false;
    if (doSaveDepthToTexture || doCountConvergence) {
        // the user requested the calculation but it was not done
        showMsg = true;
        doSaveDepthToTexture = false;
        doCountConvergence = false;
    }
    if(showMsg){
        w.text("Set ENABLE_DEBUG_UTILS to use the debug features");
    }
    if (w.button("Count convergence")) {
        doCountConvergence = true;
        showMsg = false;
    }
    ImGui::Text("Non-converged: %u\nConv.     hit: %u\nConv.    miss: %u", nonConvergedCount, convergedHitCount, convergedMissCount);
    if (w.button("Save depth to texture")) {
        doSaveDepthToTexture = true;
        showMsg = false;
    }
    if (w.button("Save depth texture to file...", true) && debugTexture) {
        FileDialogFilterVec filters;
        filters.push_back({ "exr", "EXR Files" });
        std::filesystem::path path = "depth.exr";
        if (saveFileDialog(filters, path)) {
            debugTexture->captureToFile(0, 0, path, Bitmap::FileFormat::ExrFile);
        }
    }
}
SDFRenderer::SDF_Generation_State SDFRenderer::SDF_Generation_State::create(uint3 outputVoxelSize, uint3 outputRes, uint inputVoxelSize, uint inputRes)
{
    return create(outputVoxelSize, outputRes, uint3(inputVoxelSize, 1, 1), uint3(inputRes, 1, 1));
}
SDFRenderer::SDF_Generation_State SDFRenderer::SDF_Generation_State::create(uint3 outputVoxelSize, uint3 outputRes, uint2 inputVoxelSize, uint2 inputRes)
{
    return create(outputVoxelSize, outputRes, uint3(inputVoxelSize, 1), uint3(inputRes, 1));
}
SDFRenderer::SDF_Generation_State SDFRenderer::SDF_Generation_State::create(uint3 outputVoxelSize, uint3 outputRes, uint3 inputVoxelSize, uint3 inputRes)
{
    auto s = SDF_Generation_State();

    s.inputResolution = inputRes;
    s.outputResolution = outputRes;

    s.inputVoxelSize = inputVoxelSize;
    s.outputVoxelSize = outputVoxelSize;

    s.inputDispatchIndex = 0u;
    s.outputDispatchIndex = 0u;

    s.inputDispatchSize = div_round_up(inputRes, s.inputVoxelSize);
    s.outputDispatchSize = div_round_up(outputRes, s.outputVoxelSize);

    s.inputDispatchCount =
        (uint64_t)s.inputDispatchSize.x *
        s.inputDispatchSize.y *
        s.inputDispatchSize.z;
    s.outputDispatchCount =
        (uint64_t)s.outputDispatchSize.x *
        s.outputDispatchSize.y *
        s.outputDispatchSize.z;
    return s;
}

void SDFRenderer::onGuiRender(Gui* pGui)
{
    Gui::Window w(pGui, "SDF Renderer", { 450, 750 }, { 25, 25 });
    for (uint i = 0; i < mStates.size(); ++i) {
        ImGui::PushID(i);
        ImGui::Text("State #%i", i + 1);
        if (w.button("Copy", true)) {
            mStates.push_back(state());
            auto& b = mStates.back();
            b.mpSDF = nullptr;
        }
        if (w.button("Activate", true)) {
            mCurrStateIdx = i;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(mStates[i].getModelAndSettingsString().c_str());
        if (i == mCurrStateIdx) {
            w.text("Active state", true);
        }
        else {
            if (w.button("Delete", true)) {
                mStates.erase(std::next(mStates.begin(), i));
                if (i < mCurrStateIdx)
                    mCurrStateIdx--;
            }
        }
        ImGui::PopID();
    }

    auto& s = state();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
    w.text("=== Screen capture (press C) ===");
    ImGui::PopStyleColor();
    mScreenCapture.renderGui(w);
    w.separator();

    GuiGroup(w, "Camera Controls", false, [&](auto&& g) {
        if (g.button("Reset camera")) {
            mpCameraController = createCameraController(0, mpCamera, s.mGenSettings.dataDesc.box);
            if (mCameraIndex != 0) {
                mpCameraController->update();
                mpCameraController = createCameraController(mCameraIndex, mpCamera, s.mGenSettings.dataDesc.box);
            }
        }
        if (g.radioButtons(kCameraRadioButtons, mCameraIndex)) {
            mpCameraController = createCameraController(mCameraIndex, mpCamera, s.mGenSettings.dataDesc.box);
        }
        g.separator();
        mCameraPositionsList.renderGui(g);
        if (g.button("Set camera", true)) {
            if (auto pCamPos = mCameraPositionsList.getActive()) {
                pCamPos->setCamera(mpCamera);
            }
        }
        g.checkbox("Set camera automatically after generation if listed", mSetCameraOnGeneration);
        if (g.button("Set camera from model name") && s.mpSDF) {
            if (auto pCam = mCameraPositionsList.getByName(s.mpSDF->modelName))
                pCam->setCamera(mpCamera);
        }
        g.separator();
        mpCamera->renderUI(g);
        });

    s.RenderGUI(mpDevice, *this, w);
}

void SDFRenderer::ProgramState::RenderGUI(const ref<Device>& pDevice, SDFRenderer& app, Gui::Window& w)
{
    GuiGroup(w, "Render settings", false, [&](auto&& g) {
        w.rgbColor("BG color", app.mBackgroundColor);
        mRendSettings.renderGui(g, mpSDF.get());
        });

    GuiGroup(w, "Generate SDF", true, [&](auto&& g) {
        mGenSettings.renderGui(pDevice, g, &app.mProceduralSDFList, mpSDF.get());
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.9f, .3f, 0.f, 1.f));
        if (g.button("Generate SDF")) {
            mDoGenerateSDF = true;
        }
        ImGui::PopStyleColor();
        static auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsedGenTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - mGenStartTime);
        auto elapsedSeconds = elapsedGenTime.count() / 1000.0f;

        if (mGenState.outputDispatchCount != 0) {
            // Popup during long generation
            constexpr uint2 winSize{ 350, 250 };
            const uint2 winPos = (app.mScreenSize - winSize) / 2u;
            Gui::Window w2(g, "Generation state", winSize, winPos, Gui::WindowFlags::NoResize);
            w2.windowPos(winPos.x, winPos.y);
            endTime = std::chrono::high_resolution_clock::now();
            float progress = 1.f;
            mGenState.renderGui(g, &progress);
            ImGui::ProgressBar(progress);
            ImGui::Text("Elapsed time: %.2f s", elapsedSeconds);
            auto estimateTotal = elapsedSeconds / progress;
            if (progress != 1.f) {
                ImGui::Text("Total estimate: %.2f s", estimateTotal);
                ImGui::Text("Remaining est.: %.2f s", estimateTotal - elapsedSeconds);
            }
            if (w2.button("Stop generation")) {
                mGenState.outputDispatchCount = 0;
                mGenState.inputDispatchIndex = 0;
                mGenState.outputDispatchIndex = 0;
                mDoMakeTraceProgram = false;
                mpSDF.reset();
            }
        }
        ImGui::Text("Generation time: %.2f s", elapsedSeconds);
        });

    GuiGroup(w, "Change Trace Program", false, [&](auto&& g) {
        mTraceProgramSettings.renderGui(g);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.9f, .3f, 0.f, 1.f));
        if (g.button("Set trace program")) {
            mDoMakeTraceProgram = true;
        }
        ImGui::PopStyleColor();
        });

    GuiGroup(w, "Active SDF", false, [&](auto&& g) {
        if (!mpSDF) {
            g.text("No sdf is loaded");
        }
        else {
            ImGui::PushID(mpSDF.get());
            mpSDF->renderGui(g);
            ImGui::PopID();
        }
        });

    GuiGroup(w, "Debug utils", false, [&](auto&& g) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
        g.text("=== Convergence test, Depth to texture ===");
        ImGui::PopStyleColor();
        mDebug.renderGui(g);
        g.separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
        g.text("=== Automatic convergence test ===");
        ImGui::PopStyleColor();
        app.mConvTester.renderGui(g);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
        g.text("=== Automatic performance test ===");
        ImGui::PopStyleColor();
        app.mPerfTester.renderGui(g);
        });
    w.separator();
}

ref<ComputeProgramWrapper> SDFRenderer::createGenProgram(const ref<Device>& pDevice, const SDF_Generation_Desc& genDesc)
{
    // create SDF gen. program
    DefineList defList = {};
    switch (genDesc.sourceDesc.sourceType)
    {
    case Source_Type::ProceduralFunction:
        defList.emplace("SDF_SOURCE", "0");
        if (!genDesc.sourceDesc.proceduralFunction) {
            msgBox("Error", "[SDFRenderer::createGenProgram] Source_Type is ProceduralFunction but there is no function selected", MsgBoxType::Ok, MsgBoxIcon::Error);
            return nullptr;
        }
        defList.emplace("PROCEDURAL_FUNCTION_FILE", "\"" + genDesc.sourceDesc.proceduralFunction->file + "\"");
        break;
    case Source_Type::ResampleSDF:
        if (!genDesc.sourceDesc.sdfToResample) {
            msgBox("Error", "[SDFRenderer::createGenProgram] No source SDF is loaded", MsgBoxType::Ok, MsgBoxIcon::Error);
            return nullptr;
        }
        else if (genDesc.sourceDesc.sdfToResample->desc.type.sdfType == SDF_Type::Procedural) {
            defList.emplace("SDF_SOURCE", "0");
            if (!genDesc.sourceDesc.proceduralFunction) {
                msgBox("Error", "[SDFRenderer::createGenProgram] No source SDF is present", MsgBoxType::Ok, MsgBoxIcon::Error);
                return nullptr;
            }
            defList.emplace("PROCEDURAL_FUNCTION_FILE", "\"" + genDesc.sourceDesc.proceduralFunction->file + "\"");
        }
        else {
            defList.emplace("SDF_SOURCE", "1");
        }
        break;
    case Source_Type::MeshCalc:
        break;
    default:
        msgBox("Error", "[SDFRenderer::createGenProgram] Unsupported Source_Type", MsgBoxType::Ok, MsgBoxIcon::Error);
        return nullptr;
    }
    defList.emplace("MESH_CHUNK_SIZE", std::to_string(kInputMeshChunk));
    const char* entry = genDesc.sourceDesc.sourceType == Source_Type::MeshCalc ? "calcMesh_main" : "main";
    const char* mainFile = genDesc.sourceDesc.sourceType == Source_Type::MeshCalc ? "computeFromMesh.cs.slang" : "computeSDF.cs.slang";

    auto genProg = ComputeProgramWrapper::create(pDevice);
    genProg->createProgram(kSDir / mainFile, entry, defList);

    return genProg;
}

ref<GraphicsProgramWrapper> SDFRenderer::createTraceProgram(const ref<Device>& pDevice, const SDF_TraceProgram_Desc& traceDesc)
{
    const auto& sdfType = traceDesc.type;
    DefineList defList = {};
    std::string psEntry = "main";
    switch (sdfType.sdfType)
    {
    case SDF_Type::Procedural:
        defList.emplace("SDF_SOURCE", "0");
        defList.emplace("PROCEDURAL_FUNCTION_FILE", "\"" + traceDesc.proceduralSDFDesc.file + "\"");
        break;
    case SDF_Type::SDF0:
        defList.emplace("SDF_SOURCE", "1");
        break;
    default:
        msgBox("Error", "[SDFRenderer::createTraceProgram] Unsupported SDF_Type", MsgBoxType::Ok, MsgBoxIcon::Error);
        return nullptr;
    }
    if (traceDesc.screenspaceNormal) {
        defList.emplace("SCREENSPACE_NORMAL", "1");
    }
    if (traceDesc.FORWARD_DIFF_NORMAL && !traceDesc.screenspaceNormal) {
        defList.emplace("FORWARD_DIFF_NORMAL", "1");
    }
    if (traceDesc.CALC_HARD_SHADOW) {
        defList.emplace("CALC_HARD_SHADOW", "1");
    }
    if (traceDesc.MIRROR_BACK_NORMAL) {
        defList.emplace("MIRROR_BACK_NORMAL", "1");
    }
    if (traceDesc.DISCARD_MISS) {
        defList.emplace("DISCARD_MISS", "1");
    }
    if (traceDesc.ENABLE_DEBUG_UTILS) {
        defList.emplace("ENABLE_DEBUG_UTILS", "1");
        defList.emplace("DEBUG_COLORING", std::to_string(traceDesc.DEBUG_COLORING));
    }
    defList.emplace("SDF_TRACE_FUN_NUM", std::to_string(traceDesc.SDF_TRACE_FUN_NUM));

    auto prog = GraphicsProgramWrapper::create(pDevice);
    prog->createProgram(kSDir / "cube_surface.vs.slang", kSDir / "cube_main.ps.slang", "main", psEntry, defList);
    prog->setVao(Vao::create(Vao::Topology::TriangleStrip));

    return prog;
}

void SDFRenderer::setActiveTraceProgram(const SDF_TraceProgram_Desc& traceDesc)
{
    state().mpActiveTraceProg = createTraceProgram(mpDevice, traceDesc);
    if(state().mpSDF)
        state().mpSDF->programDesc = traceDesc;
}

std::shared_ptr<SDF> SDFRenderer::ProgramState::generateSDF(
    const ref<Device>& pDevice,
    SDFRenderer& app,
    RenderContext* pContext,
    const SDF_Generation_Desc& genDesc
)
{
    const auto& dest = genDesc.dataDesc; // description of the new SDF
    const auto& source = genDesc.sourceDesc; // description of the source SDF
    const auto& res = dest.resolution;

    mTraceProgramSettings.type = dest.type;
    if (dest.type.sdfType == SDF_Type::Procedural) {
        mDoMakeTraceProgram = true;
        auto pProcSDF = app.mProceduralSDFList.getActive();
        if (!pProcSDF || source.sourceType != Source_Type::ProceduralFunction) {
            msgBox("Error", "[SDFRenderer::generateSDF] SDF_Type is Procedural but no function is set", MsgBoxType::Ok, MsgBoxIcon::Error);
            return {};
        }
        mTraceProgramSettings.proceduralSDFDesc = *pProcSDF;
        auto sdf = std::make_shared<SDF>(dest, pProcSDF->name, nullptr, mTraceProgramSettings);
        sdf->sdfState = SDF_State::Postprocessing;
        sdf->genDesc = genDesc;
        sdf->genDesc.sourceDesc.mesh.reset();
        sdf->genDesc.sourceDesc.sdfToResample.reset();
        return sdf;
    }
    if (source.sourceType == Source_Type::ResampleSDF) {
        if (!source.sdfToResample || (!source.sdfToResample->texture && source.sdfToResample->desc.type.sdfType != SDF_Type::Procedural)) {
            msgBox("Error", "[SDFRenderer::generateSDF] Empty source SDF for generation", MsgBoxType::Ok, MsgBoxIcon::Error);
            return {};
        }
    }
    else if (source.sourceType == Source_Type::MeshCalc) {
        auto t = dest.type.sdfType;
        if (t != SDF_Type::SDF0) {
            msgBox("Error", "[SDFRenderer::generateSDF] Mesh input is unsupported for this SDF type", MsgBoxType::Ok, MsgBoxIcon::Error);
            return {};
        }
        if (source.mesh.numTriangles == 0) {
            msgBox("Error", "[SDFRenderer::generateSDF] Empty source mesh", MsgBoxType::Ok, MsgBoxIcon::Error);
            return {};
        }
    }
    
    switch (dest.type.sdfType)
    {
    case SDF_Type::SDF0:
        break;
    default:
        msgBox("Error", "[SDFRenderer::generateSDF] Unsupported SDF_Type", MsgBoxType::Ok, MsgBoxIcon::Error);
        return {};
    }

    auto sdf = std::make_shared<SDF>(dest, "", nullptr, mTraceProgramSettings);
    sdf->modelName = [&] {
        switch (source.sourceType)
        {
        case Source_Type::ResampleSDF:
            return source.sdfToResample ? source.sdfToResample->modelName : ""s;
        case Source_Type::ProceduralFunction:
            return source.proceduralFunction ? source.proceduralFunction->name : ""s;
        case Source_Type::MeshCalc:
            return source.mesh.name;
        default:
            return ""s;
        }
    }();
    if (sdf->modelName.empty()) {
        msgBox("Error", "[SDFRenderer::generateSDF] Couldn't set the model name", MsgBoxType::Ok, MsgBoxIcon::Warning);
    }
    const ResourceFormat texFormat = dest.halfPrecision ? ResourceFormat::R16Float : ResourceFormat::R32Float;
    const uint32_t mipLevels = 1u;
    sdf->texture = pDevice->createTexture3D(res.x, res.y, res.z, texFormat, mipLevels, nullptr, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);    

    if (source.sourceType == Source_Type::MeshCalc)
    {
        // initialize helper texture
        ResourceFormat helperFormat = dest.halfPrecision ? ResourceFormat::RGBA16Float : ResourceFormat::RGBA32Float;
        sdf->texture2 = pDevice->createTexture3D(
            res.x, res.y, res.z, helperFormat, 1, nullptr,
            ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
        );
        auto initProg = ComputeProgramWrapper::create(pDevice);
        auto& prog = *initProg;
        prog.createProgram(kSDir / "computeFromMesh.cs.slang", "initMeshCalc_main", {});

        prog["outSDF"].setUav(sdf->texture2->getUAV(0));
        prog["CScb"]["maxSize"] = res;
        prog.runProgram(res);
    }

    mpLastGenProg = createGenProgram(pDevice, genDesc);
    if (!mpLastGenProg) {
        msgBox("Error", "[SDFRenderer::generateSDF] Couldn't create gen. program", MsgBoxType::Ok, MsgBoxIcon::Error);
        return {};
    }

    if (source.sourceType != Source_Type::MeshCalc) {
        mDoMakeTraceProgram = app.runGenProgram(
            pContext, *mpLastGenProg, sdf->texture->getUAV(0), nullptr, res, genDesc);
        sdf->sdfState = SDF_State::Postprocessing;
    } else {
        // Setup the logistics for the frame-distributed generation
        mIteratedDispatchParams.genDesc = genDesc;
        mIteratedDispatchParams.mipLevel = 0;
        mIteratedDispatchParams.outputRes = res;
        mGenState = SDFRenderer::SDF_Generation_State::create(mGenSettings.outputVoxelSize, mIteratedDispatchParams.outputRes, kInputMeshChunk, source.mesh.numTriangles);
        mGenStartTime = std::chrono::high_resolution_clock::now();
        sdf->sdfState = SDF_State::Generating;
    }

    sdf->genDesc = genDesc;
    sdf->genDesc.sourceDesc.mesh.reset();
    sdf->genDesc.sourceDesc.sdfToResample.reset();
    return sdf;
}

std::shared_ptr<CameraController> SDFRenderer::createCameraController(uint32_t camIndex, const ref<Camera>& pCam, const BBox& box)
{
    switch(camIndex){
    case 0:
    {
        auto pOrbiter = std::make_shared<OrbiterCameraController>(pCam);
        pOrbiter->setModelParams(box.corner + 0.5f * box.size, length(box.size) * 0.5f, 2.f);
        return pOrbiter;
    }
    case 1:
    {
        pCam->setUpVector(float3(0, 1, 0));
        return std::make_shared<FirstPersonCameraController>(pCam);
    }
    default:
        msgBox("Error", "[SDFRenderer::createCameraController] Unknown camera controller type requested", MsgBoxType::Ok, MsgBoxIcon::Error);
        return createCameraController(0, pCam, box);
    }
}

std::string SDFRenderer::ProgramState::getModelAndSettingsString()
{
    if (!mpSDF) return "NoSDF";
    const auto& sdf = *mpSDF;
    std::stringstream ss;
    // model name
    ss << sdf.modelName << '_';
    // epsilon
    const auto& eps = mRendSettings.traceEpsilon;
    if (eps == 1e-3f) {
        ss << "e-3_";
    }
    else if (eps == 1e-4f) {
        ss << "e-4_";
    }
    else {
        ss << "e-x_";
    }
    // SDF type
    const auto& type = sdf.desc.type.sdfType;
    ss << type << '_';
    // trace
    ss << "trace" << sdf.programDesc.SDF_TRACE_FUN_NUM << '_';
    if (sdf.programDesc.SDF_TRACE_FUN_NUM != 0) {
        float val = [&]() {
            switch (sdf.programDesc.SDF_TRACE_FUN_NUM) {
            case 2:
                return mRendSettings.relaxedParam;
            case 3:
                return mRendSettings.enhancedParam;
            case 4:
                return mRendSettings.autoParam;
            default:
                return -1.f;
            }
        }();
        ss << val << '_';
    }
    // resolution (assuming cube)
    ss << "dim" << (sdf.desc.type.sdfType != SDF_Type::Procedural ? sdf.desc.resolution.x : 0) << '_';
    // max trace step
    ss << "step" << mRendSettings.primaryTraceStepNum;
    if (sdf.programDesc.CALC_HARD_SHADOW)
        ss << "_shadow";
    return ss.str();
}

bool SDFRenderer::ProgramState::GenerateFieldChunk(SDFRenderer& app, RenderContext* pContext)
{
    static bool previousWasOn = false;

    bool meshInput = mIteratedDispatchParams.genDesc.sourceDesc.sourceType == Source_Type::MeshCalc;

    // If no output voxels are left, we are done
    if (mGenState.outputDispatchIndex >= mGenState.outputDispatchCount)
    {
        // Let's postprocess the SDF
        if (previousWasOn)
        {
            previousWasOn = false;
            if(mpSDF)
                mpSDF->sdfState = SDF_State::Finished_Iteration;
        }
        return false;
    }

    // Mark that the previous request resulted in generation
    previousWasOn = ( mGenState.outputDispatchIndex < mGenState.outputDispatchCount );

    // Dispatch the current input voxel to the generation
    auto mip = mIteratedDispatchParams.mipLevel;
    app.runGenProgram(pContext, *mpLastGenProg,
        (meshInput ? mpSDF->texture2 : mpSDF->texture)->getUAV(mip),
        (meshInput ? nullptr : !mpSDF->texture2 ? nullptr : mpSDF->texture2->getUAV(mip) ),
        mIteratedDispatchParams.outputRes, mIteratedDispatchParams.genDesc);

    // increse the input dispatch index; if we iterated over the input, let's increase the output index
    ++mGenState.inputDispatchIndex;
    if (mGenState.inputDispatchIndex >= mGenState.inputDispatchCount) {
        mGenState.inputDispatchIndex = 0;
        ++mGenState.outputDispatchIndex;
        if (mGenState.outputDispatchIndex >= mGenState.outputDispatchCount) {
            mGenState = SDFRenderer::SDF_Generation_State();
            mDoMakeTraceProgram = true;
        }
    }

    return true;
}

void SDFRenderer::ProgramState::PostProcess(const ref<Device>& pDevice, SDFRenderer& app, RenderContext* pContext)
{
    if (!mpSDF) return;
    if (mpSDF->sdfState != SDF_State::Postprocessing && mpSDF->sdfState != SDF_State::Finished_Iteration) return;

    // set normal epsilon to cell size
    if (mpSDF->desc.type.sdfType != SDF_Type::Procedural) {
        mRendSettings.shadeNormalEps = mpSDF->desc.box.size / (float3)mpSDF->desc.resolution;
        mRendSettings.shadowNormalEps = mRendSettings.shadeNormalEps.x;
    }
    else {
        mRendSettings.shadeNormalEps = float3(1e-4f);
        mRendSettings.shadowNormalEps = mRendSettings.shadeNormalEps.x;
    }
    // set camera
    if (app.mSetCameraOnGeneration) {
        if (auto pCam = app.mCameraPositionsList.getByName(mpSDF->modelName))
            pCam->setCamera(app.mpCamera);
    }
    // turn off bounding box rendering
    mRendSettings.renderSDFBBox = false;

    // Do the final step when the input is a mesh
    if (mpSDF->sdfState == SDF_State::Finished_Iteration && mIteratedDispatchParams.genDesc.sourceDesc.sourceType == Source_Type::MeshCalc && mpSDF->texture && mpSDF->texture2) {
        auto pInitProg = ComputeProgramWrapper::create(pDevice);
        auto& initProg = *pInitProg;
        const char* sdf_type = [&]() {
            switch (mIteratedDispatchParams.genDesc.dataDesc.type.sdfType) {
            case SDF_Type::SDF0:
                return "SDF_TYPE_SDF0";
            default:
                msgBox("Error", "Couldn't finish generation from mesh, the type is unsupported", MsgBoxType::Ok, MsgBoxIcon::Error);
                return "SDF_TYPE_SDF0";
            }
        }();
        initProg.createProgram(kSDir / "computeFromMesh.cs.slang", "finishMeshCalc_main", { { "SDF_TYPE", sdf_type} });

        initProg["tex2"].setUav(mpSDF->texture2->getUAV(0));
        initProg["outSDF"].setUav(mpSDF->texture->getUAV(0));
        initProg["CScb"]["maxSize"] = mpSDF->desc.resolution;
        initProg.runProgram(mpSDF->desc.resolution);
    }

    // the generation is done, we don't need the aux texture anymore
    if (mpSDF->texture2)
        mpSDF->texture2.reset();
    mpSDF->sdfState = SDF_State::Complete;
    return;
}

bool SDFRenderer::runGenProgram(RenderContext* pContext, ComputeProgramWrapper& comp, const ref<UnorderedAccessView> destTexture, const ref<UnorderedAccessView> auxTexture, uint3 res, const SDF_Generation_Desc& genDesc) {
    const auto& dest = genDesc.dataDesc; // description of the new SDF
    const auto& source = genDesc.sourceDesc; // description of the source SDF

    comp["outSDF"].setUav(destTexture);
    {
        // `comp["outAuxData"].setUav(aurTexture);` + check whether the var exists:
        auto auxTexVar = comp.getRootVar().findMember("outAuxData");
        if (auxTexVar.isValid()) {
            auxTexVar.setUav(auxTexture);
        }
    }
    comp["CScb"]["maxSize"] = res;
    comp["CScb"]["oneOverMaxSize"] = 1.0f / float3(res);
    comp["CScb"]["BBcorner"] = dest.box.corner;
    comp["CScb"]["BBsize"] = dest.box.size;

    uint3 dispatchRes = res;
    uint3 inputOffset = uint3(0);
    uint3 ouputOffset = uint3(0);
    if (genDesc.sourceDesc.sourceType == Source_Type::MeshCalc) {
        const auto& gs = state().mGenState;
        bool linearInput = genDesc.sourceDesc.sourceType == Source_Type::MeshCalc;
        if (linearInput) {
            inputOffset = uint3(
                (uint)gs.inputDispatchIndex * gs.inputVoxelSize.x, // start index
                gs.inputResolution.x, // max size
                0);
        }
        else {
            inputOffset = index1dTo3d(gs.inputDispatchIndex, gs.inputDispatchSize) * gs.inputVoxelSize;
        }
        ouputOffset = index1dTo3d(gs.outputDispatchIndex, gs.outputDispatchSize) * gs.outputVoxelSize;
        dispatchRes = gs.outputVoxelSize;
    }

    comp[ "CScb" ][ "currentInputOffset" ] = inputOffset;
    comp[ "CScb" ][ "currentOutputOffset" ] = ouputOffset;

    switch (source.sourceType) {
    case Source_Type::ProceduralFunction:
        comp["MODELcb"]["innerBoxCorner"] = dest.box.corner;
        comp["MODELcb"]["innerBoxSize"] = dest.box.size;
        comp["MODELcb"]["outerBoxCorner"] = dest.box.corner;
        comp["MODELcb"]["oneOverOuterBoxSize"] = 1.0f / dest.box.size;
        comp["MODELcb"]["outerBoxSize"] = dest.box.size;
        comp["MODELcb"]["resolution"] = dest.resolution;
        comp["MODELcb"]["resolution_r"] = 1.0f / float3(dest.resolution);
        break;
    case Source_Type::ResampleSDF:
        source.sdfToResample->setModelParameters(comp.getRootVar());
        break;
    case Source_Type::MeshCalc:
        comp["triangleBuffer"] = genDesc.sourceDesc.mesh.buffer;
        break;
    default:
        msgBox("Error", "[SDFRenderer::runGenProgram] Unsupported Source_Type", MsgBoxType::Ok, MsgBoxIcon::Error);
        return false;
    }

    comp.runProgram(dispatchRes);

    return true;
}

void SDFRenderer::onLoad(RenderContext* pRenderContext)
{
    mpDevice = pRenderContext->getDevice();
    // turn off v-sync by default (can be toggled with V)
    toggleVsync(false);

    // load procedural sdf list
    auto findFileInDataDirectories = [](const std::filesystem::path& path, std::filesystem::path& fullPath)
    {
        static std::vector<std::filesystem::path> dirs = {Falcor::getRuntimeDirectory() / "Data"};
        fullPath = Falcor::findFileInDirectories(path, dirs);
        return fullPath != std::filesystem::path{};
    };
    if (findFileInDataDirectories("proceduralSDFList.txt", kProceduralSDFListFile))
    {
        mProceduralSDFList = ProceduralSDFList::fromFile(kProceduralSDFListFile);
    }
    else {
        msgBox("Error", "[SDFRenderer::onLoad] Couldn't find proceduralSDFList.txt", MsgBoxType::Ok, MsgBoxIcon::Error);
    }
    // load camera positions list
    if (findFileInDataDirectories("cameraPositions.txt", kCameraPositionsFile))
    {
        mCameraPositionsList = CameraPositionList::fromFile(kCameraPositionsFile);
    }
    else {
        msgBox("Error", "[SDFRenderer::onLoad] Couldn't find cameraPositions.txt", MsgBoxType::Ok, MsgBoxIcon::Error);
    }

    // create cube wire program
    mpCubeWireProg = GraphicsProgramWrapper::create(mpDevice);
    mpCubeWireProg->createProgram(kSDir / "cube_frame.vs.slang", kSDir / "color.ps.slang");
    mpCubeWireProg->setVao(Vao::create(Vao::Topology::LineList));

    mStates.reserve(5);
    mStates.resize(1);
    mCurrStateIdx = 0;
    auto& s = state();
    s.mGenSettings.sourceDesc.proceduralFunction = mProceduralSDFList.getActive();

    // camera
    mpCamera = Camera::create("Main Camera");
    // create orbiter camera first (it sets the default position)
    mpCameraController = createCameraController(0, mpCamera, s.mGenSettings.dataDesc.box);
    mpCameraController->update();
    // then change it if needed
    if (mCameraIndex != 0) {
        mpCameraController = createCameraController(mCameraIndex, mpCamera, s.mGenSettings.dataDesc.box);
        mpCameraController->update();
    }
    mpCamera->setNearPlane(0.001f);
    mpCamera->beginFrame();

    // sampler
    Sampler::Desc desc;
    desc.setFilterMode(TextureFilteringMode::Point, TextureFilteringMode::Point, TextureFilteringMode::Point);
    desc.setAddressingMode(TextureAddressingMode::Clamp, TextureAddressingMode::Clamp, TextureAddressingMode::Clamp);
    mpPointSampler = mpDevice->createSampler(desc);
    desc.setFilterMode(TextureFilteringMode::Linear, TextureFilteringMode::Linear, TextureFilteringMode::Point);
    desc.setAddressingMode(TextureAddressingMode::Clamp, TextureAddressingMode::Clamp, TextureAddressingMode::Clamp);
    mpLinearSampler = mpDevice->createSampler(desc);
}

void SDFRenderer::onFrameRender(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    // clear background
    const float4 clearColor(mBackgroundColor, 1.f);
    pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);

    // gen new sdf
    if (state().mDoGenerateSDF) {
        state().mDoGenerateSDF = false;
        if (state().mGenSettings.keepSource) {
            mStates.push_back(state()); // copy state
            mCurrStateIdx = uint(mStates.size() - 1);
        }
        auto& s = state();
        s.mGenSettings.sourceDesc.sdfToResample = s.mpSDF ? std::move(s.mpSDF) : nullptr;
        s.mGenSettings.sourceDesc.updatePointers(&mProceduralSDFList);
        s.mpSDF = s.generateSDF(mpDevice, *this, pRenderContext, s.mGenSettings);
        s.mGenSettings.sourceDesc.sdfToResample = nullptr;
    }

    auto& s = state();

    // Generate chunks as long as we have unprocessed input and output and do nothing else
    if (s.GenerateFieldChunk(*this, pRenderContext)) return;

    s.PostProcess(mpDevice, *this, pRenderContext);

    // make new trace program
    if (s.mDoMakeTraceProgram && s.mpSDF) {
        s.mDoMakeTraceProgram = false;
        setActiveTraceProgram(s.mTraceProgramSettings);
    }
    // camera
    mpCameraController->update();
    mpCamera->beginFrame();

    // automatic testing
    mConvTester.startFrame();
    mPerfTester.startFrame();

    // render SDF
    {
        const auto& name = mPerfTester.testState == PerformanceTester::TestState::Running ? mPerfTester.testName : "model";
        ScopedProfilerEvent pe(pRenderContext, name);
        s.RenderSDF(*this, pRenderContext, pTargetFbo);
    }

    // render bounding box
    s.RenderBB(*this, pRenderContext, pTargetFbo);

    // automatic testing
    mConvTester.endFrame();
    mPerfTester.endFrame(pRenderContext);
    mScreenCapture.captureIfRequested(pTargetFbo);
}

bool SDFRenderer::ProgramState::RenderSDF(SDFRenderer& app, RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    if (!(mRendSettings.renderSDF && mpActiveTraceProg && mpSDF)) return false;

    const auto& pDevice = pRenderContext->getDevice();
    const auto& camPos = app.mpCamera->getPosition();
    const auto camDir = normalize(app.mpCamera->getTarget() - camPos);
    const float planeDist = dot(camPos, camDir) + app.mpCamera->getNearPlane();
    const BBox innerBox = mpSDF->desc.calcInnerBox();
    auto& activeTraceProg = *mpActiveTraceProg;

    activeTraceProg["VScb"]["modelScale"] = innerBox.size;
    activeTraceProg["VScb"]["modelTrans"] = innerBox.corner;
    activeTraceProg["VScb"]["viewProj"] = app.mpCamera->getViewProjMatrix();
    activeTraceProg["VScb"]["inverseViewProj"] = app.mpCamera->getInvViewProjMatrix();
    activeTraceProg["VScb"]["cameraPos"] = camPos;
    activeTraceProg["VScb"]["cameraDir"] = camDir;
    activeTraceProg["VScb"]["planeDist"] = planeDist;

    activeTraceProg["PScb"]["camPos"] = camPos;
    activeTraceProg["PScb"]["viewProj"] = app.mpCamera->getViewProjMatrix();
    activeTraceProg["PScb"]["maxStep"] = mRendSettings.primaryTraceStepNum;
    activeTraceProg["PScb"]["traceEpsilon"] = mRendSettings.traceEpsilon;
    activeTraceProg["PScb"]["stepRelaxation"] = [&]() {
        switch (mpSDF->programDesc.SDF_TRACE_FUN_NUM) {
        case 2:
            return mRendSettings.relaxedParam;
        case 3:
            return mRendSettings.enhancedParam;
        case 4:
            return mRendSettings.autoParam;
        default:
            return mRendSettings.relaxedParam;
        }
    }();

    mpSDF->setModelParameters(activeTraceProg.getRootVar());
    activeTraceProg["SHADEcb"]["shadeNormalEps"] = mRendSettings.shadeNormalEps;
    activeTraceProg["SHADEcb"]["shadowNormalEps"] = mRendSettings.shadowNormalEps;
    activeTraceProg["SHADEcb"]["lightDir"] = mRendSettings.lightDir;
    activeTraceProg["SHADEcb"]["colorAmbient"] = mRendSettings.colorAmbient;
    activeTraceProg["SHADEcb"]["colorDiffuse"] = mRendSettings.colorDiffuse;

    activeTraceProg["sdfSampler"] = app.mpLinearSampler;

    // debug calculations
    if (mpSDF->programDesc.ENABLE_DEBUG_UTILS) {
        activeTraceProg["debugCB"]["screenResolution"] = uint2(pTargetFbo->getWidth(), pTargetFbo->getHeight());
        activeTraceProg["debugCB"]["saveDepthToDebugTexture"] = mDebug.doSaveDepthToTexture;
        activeTraceProg["debugCB"]["saveConvergence"] = mDebug.doCountConvergence;
        if (mDebug.doSaveDepthToTexture) {
            mDebug.debugTexture = pDevice->createTexture2D(
                pTargetFbo->getWidth(), pTargetFbo->getHeight(), ResourceFormat::R32Float, 1, 1, nullptr,
                ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
            );
            activeTraceProg["debugTexture"] = mDebug.debugTexture;
        }
        const static uint zeros[3] = { 0,0,0 };
        activeTraceProg.allocateStructuredBuffer("debugBuffer", 3, zeros);
    }

    // rendering
    auto diff = abs(camPos - innerBox.corner - 0.5f * innerBox.size) - 0.5f * innerBox.size;
    if (diff.x < 0.f && diff.y < 0.f && diff.z < 0.f) {
       // fullScreen "quad"
        activeTraceProg["VScb"]["type"] = 0u;
        activeTraceProg.draw(pRenderContext, pTargetFbo, 3);
    }
    else {
        auto frontVertex = float3((camDir.x < 0 ? innerBox.size.x : 0), (camDir.y < 0 ? innerBox.size.y : 0), (camDir.z < 0 ? innerBox.size.z : 0));
        frontVertex += innerBox.corner;
        if (dot(frontVertex, camDir) < planeDist) { // the bounding box' corner is clipped
            // clip
            activeTraceProg["VScb"]["type"] = 1u;
            activeTraceProg.draw(pRenderContext, pTargetFbo, 6);
        }
        // bounding box
        activeTraceProg["VScb"]["type"] = 2u;
        activeTraceProg.draw(pRenderContext, pTargetFbo, 14);
    }
    // retrieving debug calculations
    if (mpSDF->programDesc.ENABLE_DEBUG_UTILS) {
        if (mDebug.doCountConvergence) {
            mDebug.doCountConvergence = false;
            auto pVals = activeTraceProg.mapBuffer<const uint>("debugBuffer");
            mDebug.nonConvergedCount = pVals[0];
            mDebug.convergedHitCount = pVals[1];
            mDebug.convergedMissCount = pVals[2];
            activeTraceProg.unmapBuffer("debugBuffer");
        }
        if (mDebug.doSaveDepthToTexture) {
            mDebug.doSaveDepthToTexture = false;
        }
    }
    return true;
}

bool SDFRenderer::ProgramState::RenderBB(SDFRenderer& app, RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    if (!mRendSettings.renderSDFBBox) {
        if (!mpSDF || any(mpSDF->desc.box.corner != mGenSettings.dataDesc.box.corner) || any(mpSDF->desc.box.size != mGenSettings.dataDesc.box.size))
        {
            mRendSettings.renderSDFBBox = true;
        }
    }
    if (!(mRendSettings.renderSDFBBox && app.mpCubeWireProg)) return false;

    auto& prog = *app.mpCubeWireProg;
    // draw outer BB
    prog["VScb"]["modelScale"] = mGenSettings.dataDesc.box.size;
    prog["VScb"]["modelTrans"] = mGenSettings.dataDesc.box.corner;
    prog["VScb"]["viewProj"] = app.mpCamera->getViewProjMatrix();
    prog["PScb"]["color"] = float3(.5, 0, 1);

    prog.draw(pRenderContext, pTargetFbo, 24);

    // draw inner BB
    auto inner = mGenSettings.dataDesc.calcInnerBox();
    prog["VScb"]["modelScale"] = inner.size;
    prog["VScb"]["modelTrans"] = inner.corner;

    prog.draw(pRenderContext, pTargetFbo, 24);

    return true;
}

void SDFRenderer::onShutdown()
{
    
}

bool SDFRenderer::onKeyEvent(const KeyboardEvent& keyEvent)
{
    // pass evets to the camera
    if (mpCameraController->onKeyEvent(keyEvent)) return true;

    // don't double trigger things
    if (keyEvent.type == KeyboardEvent::Type::KeyReleased) return false;

    switch (keyEvent.key)
    {
    case Input::Key::Escape:
        return true; // stop Falcor from exiting on pressing Escape
    case Input::Key::C:
        if (keyEvent.hasModifier(Input::Modifier::Ctrl)) {
            // copy model string to clipboard
            const auto& model = state().getModelAndSettingsString();
            ImGui::SetClipboardText(model.c_str());
            return true;
        }
        else {
            // create screen capture
            const auto& model = state().getModelAndSettingsString();
            mScreenCapture.captrueNextFrame(model);
            return true;
        }
        break;
    case Input::Key::P:
        if (keyEvent.hasModifier(Input::Modifier::Ctrl)) {
            // start performance test
            if (!mPerfTester.startTest()) {
                msgBox("Error", "Couldn't start test, incorrect parameters?", MsgBoxType::Ok, MsgBoxIcon::Error);
            }
            break;
        }
    case Input::Key::Key1: case Input::Key::Key2: case Input::Key::Key3:
    case Input::Key::Key4: case Input::Key::Key5: case Input::Key::Key6:
    case Input::Key::Key7: case Input::Key::Key8: case Input::Key::Key9:
    {
        const uint32_t index = (uint32_t)keyEvent.key - (uint32_t)Input::Key::Key1;
        if (index < mStates.size()) {
            mCurrStateIdx = (uint)index;
            return true;
        }
        break;
    }
    }
    return false;
}

bool SDFRenderer::onMouseEvent(const MouseEvent& mouseEvent)
{
    mpCameraController->onMouseEvent(mouseEvent);
    if (mouseEvent.wheelDelta.y != 0.f) {
        static float camSpeed = 1.0f;
        camSpeed *= pow(1.3f, mouseEvent.wheelDelta.y);
        mpCameraController->setCameraSpeed(camSpeed);
    }
    return false;
}

bool SDFRenderer::onGamepadState(const GamepadState& gamepadState)
{
    return mpCameraController->onGamepadState(gamepadState);
}

void SDFRenderer::onHotReload(HotReloadFlags reloaded)
{
}

void SDFRenderer::onResize(uint32_t width, uint32_t height)
{
    mScreenSize = { width, height };
    mpCamera->setAspectRatio((float)width / (float)height);
}




void SDFRenderer::ConvergenceTester::startTest(uint minNum, uint maxNum)
{
    startStepNum = minNum;
    endStepNum = maxNum;
    currentStepNum = startStepNum;
    results.clear();
    results.reserve(endStepNum - startStepNum + 1);
    testState = TestState::Running;
}

void SDFRenderer::ConvergenceTester::pauseTest()
{
    if(testState == TestState::Running)
        testState = TestState::Paused;
}

void SDFRenderer::ConvergenceTester::resumeTest()
{
    if (testState == TestState::Paused)
        testState = TestState::Running;
}

void SDFRenderer::ConvergenceTester::startFrame()
{
    if (testState != TestState::Running) return;
    app.state().mRendSettings.primaryTraceStepNum = currentStepNum;
    app.state().mDebug.doCountConvergence = true;
}

void SDFRenderer::ConvergenceTester::endFrame()
{
    if (testState != TestState::Running) return;
    results.emplace_back(Result{
        currentStepNum,
        app.state().mDebug.nonConvergedCount,
        app.state().mDebug.convergedHitCount,
        app.state().mDebug.convergedMissCount
        });
    currentStepNum++;
    if (currentStepNum > endStepNum)
        testState = TestState::Ended;
}
void SDFRenderer::ConvergenceTester::renderGui(Gui::Widgets& w)
{
    switch (testState)
    {
    case SDFRenderer::ConvergenceTester::TestState::NotTesting:
        w.var("Starting step num", startStepNum, 1u, 500u);
        w.var("Last step num", endStepNum, startStepNum, 500u);
        if (w.button("Start test")) {
            startTest(startStepNum, endStepNum);
        }
        break;
    case SDFRenderer::ConvergenceTester::TestState::Running:
        ImGui::Text("Test in progress\nStart: %u\nCurrent: %u\nLast: %u", startStepNum, currentStepNum, endStepNum);
        if (w.button("Pause")) {
            pauseTest();
        }
        break;
    case SDFRenderer::ConvergenceTester::TestState::Paused:
        ImGui::Text("Testing is paused\nStart: %u\nCurrent: %u\nLast: %u", startStepNum, currentStepNum, endStepNum);
        w.var("Last step num", endStepNum, startStepNum, 500u);
        if (w.button("Resume")) {
            resumeTest();
        }
        break;
    case SDFRenderer::ConvergenceTester::TestState::Ended:
        ImGui::Text("Test ended\nStart: %u\nLast: %u", startStepNum, endStepNum);
        if (w.button("Save results to file...")) {
            FileDialogFilterVec filters;
            filters.push_back({ "txt", "Text Files" });
            std::filesystem::path path;
            if (saveFileDialog(filters, path)) {
                std::ofstream of(path);
                printResults(of);
            }
        }
        if (w.button("Copy results to clipboard", true)) {
            std::stringstream ss;
            ss << app.state().getModelAndSettingsString() << "\n";
            printResults(ss);
            ImGui::SetClipboardText(ss.str().c_str()); // note: the temporary string lives until the end of the expression, so this is fine
        }
        if (w.button("New test")) {
            testState = TestState::NotTesting;
        }
        break;
    default:
        break;
    }
}

void SDFRenderer::ConvergenceTester::printResults(std::ostream& os)
{
    os << "stepNum\tnonConvergedCount\tconvergedHitCount\tconvergedMissCount\n";
    for (auto& r : results) {
        os << r << '\n';
    }
}

std::ostream& operator<<(std::ostream& os, const SDFRenderer::ConvergenceTester::Result& r)
{
    return os << r.stepNum << '\t' << r.nonConvergedCount << '\t' << r.convergedHitCount << '\t' << r.convergedMissCount;
    
}




bool SDFRenderer::PerformanceTester::startTest()
{
    if (!param || startParam >= endParam || paramStep <= 0.0f || numTestFrames < 3)
        return false;
    float testNum = (endParam - startParam) / paramStep;
    if (testNum > 2000.f || testNum < 1.f)
        return false;
    currFrame = 0u;
    results.clear();
    results.reserve((uint)testNum + 2);
    rawTimes.clear();
    rawTimes.reserve(numTestFrames + 1);
    testState = TestState::Starting;
    return true;
}

void SDFRenderer::PerformanceTester::stopTest()
{
    testState = TestState::Ended;
}

void SDFRenderer::PerformanceTester::startFrame()
{
    if (testState == TestState::Starting) {
        currFrame = 0u;
        *param = startParam;
        testName = "p_" + std::to_string(*param);
        testState = TestState::Running;
    }
}

void SDFRenderer::PerformanceTester::endFrame(RenderContext* pRenderContext)
{
    if (testState != TestState::Running)
        return;
    if (currFrame != 0) {
        const auto& e = pRenderContext->getProfiler()->getEvent(("/onFrameRender/" + testName).c_str());
        if (!e) {
            // profiler is not active?
            testState = TestState::Ended;
            msgBox("Error", "Couldn't make measurements", MsgBoxType::Ok, MsgBoxIcon::Error);
            param = nullptr;
            return;
        }
        rawTimes.push_back(e->getGpuTime());
    }
    currFrame++;
    if (currFrame > numTestFrames) {
        // finished the testing of one parameter
        Profiler::Stats s = Profiler::Stats::compute(rawTimes.data() + 1, numTestFrames - 1);
        results.emplace_back(Result{ *param, s });
        // setup next test
        *param += paramStep;
        rawTimes.clear();
        if (*param > endParam) {
            // test ended
            const auto& e = std::min_element(results.cbegin(), results.cend(), [](auto& a, auto& b) {return a.stats.min < b.stats.min; });
            *param = e != results.cend() ? e->param : endParam;
            testState = TestState::Ended;
            param = nullptr;
            return;
        }
        currFrame = 0u;
        testName = "p_" + std::to_string(*param);
    }
}

void SDFRenderer::PerformanceTester::renderGui(Gui::Widgets& w)
{
    switch (testState)
    {
    case TestState::NotTesting:
        // Gui for settings
    {
        w.text("The profiling window (P) must be open during performance testing");

        w.var("Number of test frames", numTestFrames, 3u, 200u);
        static std::string paramName = "not set";
        if (!param) paramName = "not set";
        ImGui::Text("Tested parameter: %s", paramName.c_str());
        w.var("Starting parameter value", startParam, 0.0f, 2.0f);
        w.var("End parameter value", endParam, startParam, 2.0f);
        w.var("Parameter value step", paramStep, 0.001f, 0.5f);
        w.text("Hide the GUI by pressing F2, then start the test by pressing Ctrl+P");
        if (w.button("Set tested parameter to `relaxedParam`")) {
            param = &app.state().mRendSettings.relaxedParam;
            paramName = "relaxedParam";
        }
        if (w.button("Set tested parameter to `enhancedParam`")) {
            param = &app.state().mRendSettings.enhancedParam;
            paramName = "enhancedParam";
        }
        if (w.button("Set tested parameter to `autoParam`")) {
            param = &app.state().mRendSettings.autoParam;
            paramName = "autoParam";
        }
        /** /
        if (w.button("Start test##preftest")) {
            startTest();
        }
        /**/
        break;
    }
    case TestState::Starting:
        // nothing
        break;
    case TestState::Running:
        // Gui shouldn't really be visible
        w.text("Close the GUI (F2) for more accurate measurements");
        ImGui::Text("Test in progress\nStart: %.4f\nCurrent: %.4f\nLast: %.4f", startParam, *param, endParam);
        if (w.button("Stop test##perftest")) {
            stopTest();
        }
        break;
    case TestState::Ended:
        // save test results, start new test
        ImGui::Text("Test ended\nStart: %.4f\nLast: %.4f", startParam, endParam);
        if (w.button("Save results to file...##perftest")) {
            FileDialogFilterVec filters;
            filters.push_back({ "txt", "Text Files" });
            std::filesystem::path path;
            if (saveFileDialog(filters, path)) {
                std::ofstream of(path);
                printResults(of);
            }
        }
        if (w.button("Copy results to clipboard##perftest", true)) {
            std::stringstream ss;
            ss << app.state().getModelAndSettingsString() << "\n";
            printResults(ss);
            ImGui::SetClipboardText(ss.str().c_str());
        }
        if (w.button("New test##perftest")) {
            testState = TestState::NotTesting;
        }
        break;
    }
}

void SDFRenderer::PerformanceTester::printResults(std::ostream& os)
{
    os << "param\tmin\tmax\tavg\tstdDev\n";
    for (auto& r : results) {
        os << r << '\n';
    }
}

std::ostream& operator<<(std::ostream& os, const SDFRenderer::PerformanceTester::Result& r)
{
    return os << r.param << '\t' << r.stats.min << '\t' << r.stats.max << '\t' << r.stats.mean << '\t' << r.stats.stdDev;
}



namespace Falcor {

    Profiler::Stats Profiler::Stats::compute(const float* data, size_t len)
    {
        if (len == 0) return {};

        float min = std::numeric_limits<float>::max();
        float max = std::numeric_limits<float>::lowest();
        double sum = 0.0;
        double sum2 = 0.0;

        for (size_t i = 0; i < len; ++i)
        {
            float value = data[i];
            min = std::min(min, value);
            max = std::max(max, value);
            sum += value;
            sum2 += value * value;
        }

        double mean = sum / len;
        double mean2 = sum2 / len;
        double variance = mean2 - mean * mean;
        double stdDev = std::sqrt(variance);

        return { min, max, (float)mean, (float)stdDev };
    }

    Profiler::Stats Profiler::Event::computeGpuTimeStats() const
    {
        return Stats::compute(mGpuTimeHistory.data(), mHistorySize);
    }
}

void SDFRenderer::ScreenCapture::captrueNextFrame(std::string fileName, std::filesystem::path directory)
{
    mDoCapture = true;
    mFileName = fileName;
    mDirectory = directory;
}

void SDFRenderer::ScreenCapture::captureIfRequested(const ref<Fbo>& pTargetFbo)
{
    if (mDoCapture)
    {
        mDoCapture = false;
        const std::string& f = mFileName == "" ? getExecutableName() : mFileName;
        std::filesystem::path d = mDirectory == "" ? mDefaultDirectory : mDirectory;
        std::filesystem::path path = findAvailableFilename(f, d, "png");
        pTargetFbo->getColorTexture(0).get()->captureToFile(0, 0, path);
    }
}

void SDFRenderer::ScreenCapture::renderGui(Gui::Widgets& w)
{
    if (w.button("Choose capture directory"))
    {
        std::filesystem::path tmp = mDefaultDirectory;
        if (Falcor::chooseFolderDialog(tmp))
        {
            mDefaultDirectory = tmp;
        }
    }
    w.text("Current folder: " + mDefaultDirectory.string());
}
