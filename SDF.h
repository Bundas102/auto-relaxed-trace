#pragma once

#include "Falcor.h"

#include "dear_imgui/imgui.h"

#include "FlatMesh.h"
#include "Utils/hash_tuple.hpp"

using namespace Falcor;

class SDF;

enum class SDF_Type {
    SDF0,                /* traditional order 0 Signed Distance Field         */
    Procedural,          /* Procedural function inside a bounding box         */
};

enum class Source_Type {
    ResampleSDF,          /* resample a loaded DSDF               */
    ProceduralFunction,   /* use a function written in the shader */
    MeshCalc,             /* distance from a loaded mesh          */
};


bool Dropdown(Gui::Widgets& w, const char label[], SDF_Type& var, bool sameLine = false);
bool Dropdown(Gui::Widgets& w, const char label[], Source_Type& var, bool sameLine = false);

std::ostream& operator<<(std::ostream& os, SDF_Type val);
std::ostream& operator<<(std::ostream& os, Source_Type val);

// CRTP
template<typename Renderable>
struct ConstRender {
    void renderGuiConst(Gui::Widgets& w) const {
        const auto& col = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        Renderable copy = static_cast<const Renderable&>(*this);
        copy.renderGui(w);
        ImGui::PopStyleColor();
    }
    friend bool operator==(const Renderable& a, const Renderable& b) {
        return a.asTuple() == b.asTuple();
    }
};

struct SDF_Type_Desc : ConstRender<SDF_Type_Desc>, hash_tuple::TupleHash<SDF_Type_Desc> {
    SDF_Type sdfType = SDF_Type::SDF0;

    auto asTuple() const { return std::make_tuple(sdfType); }

    void renderGui(Gui::Widgets& w);
};

// a bounding box
struct BBox : ConstRender<BBox> {
    float3 corner{ 0.f };       // bounding box width, height, depth
    float3 size{ 1.f };         // bounding box min. corner coordinates

    auto asTuple() const { return std::tie(corner, size); }

    void renderGui(Gui::Widgets& w);
    // calculate inner box of SDF with resolution RES
    // res_r : 1 / RES
    BBox calcInnerBox(float3 res_r) const;
};

struct SDF_Data_Desc : ConstRender<SDF_Data_Desc> {
    // SDF type
    SDF_Type_Desc type{};
    bool halfPrecision{ true }; // 16bit floats if true (32bit otherwise)
    // Sample spacing
    uint3 resolution{ 64 };      // number of samples in the texture: w, h, d
    BBox box{};
//  float3 border;              // == 0.5f * size / res
//  float3 res_r;               // == 1.0f / res

    auto asTuple() const { return std::tie(type, halfPrecision, resolution, box); }

    void renderGui(Gui::Widgets& w);

    BBox calcInnerBox() const { return box.calcInnerBox(1.0f / float3(resolution)); }
};

struct ProceduralSDF {
    std::string name;
    std::string file;
    BBox boundingBox;

    void renderGui(Gui::Widgets& w) const;
};
struct ProceduralSDFList {
    std::vector<ProceduralSDF> sdfs;
    int activeIndex = 0;

    const ProceduralSDF* getActive();
    static ProceduralSDFList fromFile(const std::filesystem::path& path);

    bool renderGui(Gui::Widgets& w);
};
struct SDF_TraceProgram_Desc : ConstRender<SDF_TraceProgram_Desc> {
    // SDF type
    SDF_Type_Desc type{};

    // procedural sdf
    ProceduralSDF proceduralSDFDesc{};

    // trace program parameters
    int SDF_TRACE_FUN_NUM = 1;
    bool CALC_HARD_SHADOW{ false };
    bool MIRROR_BACK_NORMAL{ true };
    bool DISCARD_MISS{ true };
    bool screenspaceNormal{ false };
    bool ENABLE_DEBUG_UTILS{ false };
    bool FORWARD_DIFF_NORMAL{ false };
    int DEBUG_COLORING = 0;

    auto asTuple() const { return std::tie(type, SDF_TRACE_FUN_NUM, CALC_HARD_SHADOW, MIRROR_BACK_NORMAL, DISCARD_MISS, screenspaceNormal, ENABLE_DEBUG_UTILS, FORWARD_DIFF_NORMAL, DEBUG_COLORING); }

    void renderGui(Gui::Widgets& w);
};

struct CameraPosition {
    std::string name;
    float3 camPos;
    float3 camAt;
    float3 camUp;

    void setCamera(Falcor::Camera::SharedPtr& pCam) const;
};
struct CameraPositionList {
    std::vector<CameraPosition> positions;
    int activeIndex = 0;

    const CameraPosition* getActive();
    const CameraPosition* getByName(const std::string& name) const;
    static CameraPositionList fromFile(const std::filesystem::path& path);

    bool renderGui(Gui::Widgets& w);
};

struct SDF_DistanceSource_Desc : ConstRender<SDF_DistanceSource_Desc> {
    Source_Type sourceType = Source_Type::ProceduralFunction;

    // Source_Type::ResampleSDF
    std::shared_ptr<SDF> sdfToResample;
    // Source_Type::ProceduralFunction
    const ProceduralSDF* proceduralFunction;
    // Source_Type::MeshCalc
    FlatMesh mesh;

    void updatePointers(ProceduralSDFList* sdfList);

    auto asTuple() const { return std::make_tuple(
        sourceType,
        sourceType == Source_Type::ResampleSDF ? sdfToResample : nullptr,
        sourceType == Source_Type::ProceduralFunction ? proceduralFunction : nullptr,
        sourceType == Source_Type::MeshCalc ? mesh.buffer : nullptr
    ); }

    void renderGui(Gui::Widgets& w, BBox* boxToSet, ProceduralSDFList* sdfList = nullptr);
};
struct SDF_Generation_Desc : ConstRender<SDF_Generation_Desc> {
    // description of the new SDF
    SDF_Data_Desc dataDesc{};
    // description of the SDF source and creation of samples
    SDF_DistanceSource_Desc sourceDesc{};

    // run params
    uint3 outputVoxelSize{ 64 };
    bool keepSource = false;

    auto asTuple() const { return std::tie(dataDesc, sourceDesc, outputVoxelSize, keepSource); }

    void renderGui(Gui::Widgets& w, ProceduralSDFList* sdfList = nullptr, SDF* activeSDF = nullptr);
};

struct Render_Settings : ConstRender<Render_Settings> {
    // draw settings
    bool renderSDF = true;
    bool renderSDFBBox = true;
    // trace settings
    uint primaryTraceStepNum{ 100 };
    float traceEpsilon{ 0.0001f };
    float relaxedParam{ 1.6f };
    float enhancedParam{ 0.88f };
    float autoParam{ 0.3f };
    // shade settings
    float3 lightDir{ glm::normalize(float3{ -1, -1, -1}) };
    float3 colorAmbient{ 0.01f };
    float3 colorDiffuse{ 1.0f };
    float3 shadeNormalEps{ 1.0f / 32.0f };
    float shadowNormalEps{ 0.001f };

    auto asTuple() const { return std::tie(renderSDF, renderSDFBBox, primaryTraceStepNum, traceEpsilon, relaxedParam, enhancedParam, autoParam, lightDir, colorAmbient, colorDiffuse, shadeNormalEps, shadowNormalEps); }

    void renderGui(Gui::Widgets& w, const SDF* activeSdf = nullptr);
};

enum class SDF_State {
    Empty,
    Generating,
    Finished_Iteration,
    Postprocessing,
    Complete
};

class SDF {
public:
    SDF(const SDF_Data_Desc& _desc = SDF_Data_Desc{}, const std::string& _name = "", Texture::SharedPtr _texture = nullptr, const SDF_TraceProgram_Desc& _progDesc = SDF_TraceProgram_Desc{})
        : desc(_desc), modelName(_name), texture(_texture), programDesc(_progDesc) {}
    SDF_Data_Desc desc;
    std::string modelName;
    Texture::SharedPtr texture;
    Texture::SharedPtr texture2;
    Buffer::SharedPtr buffer;
    SDF_TraceProgram_Desc programDesc;
    SDF_Generation_Desc genDesc;
    SDF_State sdfState = SDF_State::Empty;


    void renderGui(Gui::Widgets& w) const;

    void setModelParameters(const ShaderVar& rootVar) const;
};
