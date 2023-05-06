#include "SDF.h"

namespace ImGui {
bool HoverTooltip(const char* fmt, ...) {
    if (ImGui::IsItemHovered()) {
        // copied from ImGui::SetTooltip
        va_list args;
        va_start(args, fmt);
        SetTooltipV(fmt, args);
        va_end(args);
        return true;
    }
    return false;
}
std::array<bool, 20> DisableStack{};
int DisableStackTop = 0;
void BeginDisable(bool disable) {
    DisableStack[DisableStackTop++] = disable;
    if (disable)
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
}
void EndDisable() {
    if (DisableStack[--DisableStackTop])
        ImGui::PopStyleColor();
}
}

void SDF_Type_Desc::renderGui(Gui::Widgets& w)
{
    Dropdown(w, "SDF type", sdfType);
}

void BBox::renderGui(Gui::Widgets& w)
{
    w.var("BB size", size, 0.1f);
    w.var("BB corner", corner);
}

BBox BBox::calcInnerBox(float3 res_r) const
{
    BBox box;
    box.corner = corner + 0.5f * res_r * size;
    box.size = size * (1.0f - res_r);
    return box;
}

void SDF_Data_Desc::renderGui(Gui::Widgets& w)
{
    type.renderGui(w);
    ImGui::BeginDisable(type.sdfType == SDF_Type::Procedural);
    w.checkbox("Half precision", halfPrecision);
    ImGui::HoverTooltip("16bit float if true, 32bit otherwise");
    if (w.var("Resolution", resolution, 2))
    {
        static uint cachedResolutionX = 32;
        if (cachedResolutionX != resolution.x)
        {
            resolution.y = resolution.x;
            resolution.z = resolution.x;
            cachedResolutionX = resolution.x;
        }
    }
    ImGui::EndDisable();
    box.renderGui(w);
}

void SDF_TraceProgram_Desc::renderGui(Gui::Widgets& w)
{
    type.renderGuiConst(w);
    if (type.sdfType == SDF_Type::Procedural) {
        proceduralSDFDesc.renderGui(w);
    }
    ImGui::Separator();
    w.checkbox("Hard shadow", CALC_HARD_SHADOW);
    w.checkbox("Mirror back facing normals", MIRROR_BACK_NORMAL);
    w.checkbox("Discard fragments", DISCARD_MISS);
    ImGui::BeginDisable(type.sdfType != SDF_Type::SDF0 && type.sdfType != SDF_Type::Procedural);
    if (w.button("1 SPHERE TRACE##SDF_FUN")) SDF_TRACE_FUN_NUM = 1;
    ImGui::HoverTooltip("Sphere trace");
    if (w.button("2 RELAXED##SDF_FUN", true)) SDF_TRACE_FUN_NUM = 2;
    ImGui::HoverTooltip("Relaxed sphere trace");
    if (w.button("3 ENHANCED##SDF_FUN", true)) SDF_TRACE_FUN_NUM = 3;
    ImGui::HoverTooltip("Enhanced sphere trace");
    if (w.button("4 AUTO##SDF_FUN", true)) SDF_TRACE_FUN_NUM = 4;
    ImGui::HoverTooltip("Auto-relaxed sphere trace");
    w.var("SDF_TRACE_FUN_NUM", SDF_TRACE_FUN_NUM, 1, 4, 1.f, false);
    ImGui::EndDisable();
    w.checkbox("screen space normal", screenspaceNormal);
    ImGui::BeginDisable(screenspaceNormal);
    w.checkbox("FORWARD_DIFF_NORMAL", FORWARD_DIFF_NORMAL);
    ImGui::EndDisable();
    w.checkbox("ENABLE_DEBUG_UTILS", ENABLE_DEBUG_UTILS);
    ImGui::BeginDisable(!ENABLE_DEBUG_UTILS);
    if (w.button("0 OFF##DEB_COL")) DEBUG_COLORING = 0;
    ImGui::HoverTooltip("Debug coloring off");
    if (w.button("1 STEP C##DEB_COL", true)) DEBUG_COLORING = 1;
    ImGui::HoverTooltip("Step count");
    if (w.button("2 BACK C##DEB_COL", true)) DEBUG_COLORING = 2;
    ImGui::HoverTooltip("Backstep count");
    if (w.button("3 STEP S##DEB_COL", true)) DEBUG_COLORING = 3;
    ImGui::HoverTooltip("Average step size");
    w.var("DEBUG_COLORING", DEBUG_COLORING, 0, 10, 1.f, false);
    ImGui::HoverTooltip("Only works if ENABLE_DEBUG_UTILS is on");
    ImGui::EndDisable();
}

void ProceduralSDF::renderGui(Gui::Widgets& w) const
{
    ImGui::PushID("Procedural SDF");
    ImGui::Text("Name: %s", name.c_str());
    ImGui::Text("File: %s", file.c_str());
    ImGui::Text("Default bounding box:");
    boundingBox.renderGuiConst(w);
    ImGui::PopID();
}


void CameraPosition::setCamera(Falcor::Camera::SharedPtr& pCam) const
{
    if (!pCam) return;
    pCam->setPosition(camPos);
    pCam->setTarget(camAt);
    pCam->setUpVector(camUp);
}

const CameraPosition* CameraPositionList::getActive()
{
    if (activeIndex >= (int)positions.size()) activeIndex = (int)positions.size() - 1;
    return activeIndex < 0 ? nullptr : &positions[activeIndex];
}

const CameraPosition* CameraPositionList::getByName(const std::string& name) const
{
    auto camIt = std::find_if(positions.begin(), positions.end(), [&](const auto& cam) { return cam.name == name; });
    if (camIt != positions.end()) {
        return &(*camIt);
    }
    return nullptr;
}

CameraPositionList CameraPositionList::fromFile(const std::filesystem::path& path)
{
    CameraPositionList list;
    std::ifstream fin(path);
    if (!fin) {
        msgBox("[CameraPositionList::fromFile] couldn't open file", MsgBoxType::Ok, MsgBoxIcon::Error);
        return list;
    }
    std::string line;
    int lineNo = 0;
    while (std::getline(fin, line)) {
        lineNo++;
        std::stringstream ss(line);
        std::string name;
        ss >> name;
        if (!ss || name.length() < 1 || name[0] == '/')
            continue;
        CameraPosition pos;
        pos.name = name;
        auto& p = pos.camPos;
        auto& a = pos.camAt;
        auto& u = pos.camUp;
        ss >> p.x >> p.y >> p.z >> a.x >> a.y >> a.z >> u.x >> u.y >> u.z;
        if (!ss) {
            const std::string errorMsg = "[CameraPositionList::fromFile] couldn't parse line " + std::to_string(lineNo) + ": \"" + line + "\"";
            msgBox(errorMsg, MsgBoxType::Ok, MsgBoxIcon::Error);
        }
        else {
            list.positions.emplace_back(std::move(pos));
        }
    }
    return list;
}

bool CameraPositionList::renderGui(Gui::Widgets& w)
{
    const int activeBefore = activeIndex;
    auto activeSDF = getActive();
    const char* activeName = activeSDF ? activeSDF->name.c_str() : nullptr;
    if (ImGui::BeginCombo("##camera list combo", activeName, ImGuiComboFlags_None))
    {
        for (int n = 0; n < (int)positions.size(); n++)
        {
            const bool is_selected = (activeIndex == n);
            if (ImGui::Selectable(positions[n].name.c_str(), is_selected))
                activeIndex = n;
            if (is_selected)
                ImGui::SetItemDefaultFocus();
/*
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                positions[n].renderGui(w);
                ImGui::EndTooltip();
            }
*/
        }
        ImGui::EndCombo();
    }
    return activeBefore != activeIndex;
}

const ProceduralSDF* ProceduralSDFList::getActive()
{
    if (activeIndex >= (int)sdfs.size()) activeIndex = (int)sdfs.size() - 1;
    return activeIndex < 0 ? nullptr : &sdfs[activeIndex];
}

ProceduralSDFList ProceduralSDFList::fromFile(const std::filesystem::path& path)
{
    ProceduralSDFList list;
    std::ifstream fin(path);
    if (!fin) {
        msgBox("[ProceduralSDFList::fromFile] couldn't open file", MsgBoxType::Ok, MsgBoxIcon::Error);
        return list;
    }
    std::string line;
    int lineNo = 0;
    while (std::getline(fin, line)) {
        lineNo++;
        std::stringstream ss(line);
        std::string name;
        ss >> name;
        if (!ss || name.length() < 1 || name[0] == '/')
            continue;
        ProceduralSDF desc;
        desc.name = name;
        auto& c = desc.boundingBox.corner;
        auto& s = desc.boundingBox.size;
        ss >> desc.file >> c.x >> c.y >> c.z >> s.x >> s.y >> s.z;
        if (!ss) {
            const std::string errorMsg = "[ProceduralSDFList::fromFile] couldn't parse line " + std::to_string(lineNo) + ": \"" + line + "\"";
            msgBox(errorMsg, MsgBoxType::Ok, MsgBoxIcon::Error);
        }
        else {
            list.sdfs.emplace_back(std::move(desc));
        }
    }
    return list;
}

bool ProceduralSDFList::renderGui(Gui::Widgets& w)
{
    const int activeBefore = activeIndex;
    auto activeSDF = getActive();
    const char* activeName = activeSDF ? activeSDF->name.c_str() : nullptr;
    if (ImGui::BeginCombo("##procedural sdf combo", activeName, ImGuiComboFlags_None))
    {
        for (int n = 0; n < (int)sdfs.size(); n++)
        {
            const bool is_selected = (activeIndex == n);
            if (ImGui::Selectable(sdfs[n].name.c_str(), is_selected))
                activeIndex = n;
            if (is_selected)
                ImGui::SetItemDefaultFocus();
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                sdfs[n].renderGui(w);
                ImGui::EndTooltip();
            }
        }
        ImGui::EndCombo();
    }
    return activeBefore != activeIndex;
}

void SDF_DistanceSource_Desc::updatePointers(ProceduralSDFList* sdfList)
{
    proceduralFunction = sdfList ? sdfList->getActive() : nullptr;
}

void SDF_DistanceSource_Desc::renderGui(Gui::Widgets& w, BBox* boxToSet, ProceduralSDFList* sdfList)
{
    Dropdown(w, "Source type", sourceType);
    ImGui::Indent();
    ImGui::Separator();
    if (sourceType == Source_Type::ProceduralFunction) {
        if (sdfList && sdfList->renderGui(w)) {
            proceduralFunction = sdfList->getActive();
            if (proceduralFunction) {
                if (boxToSet)
                    *boxToSet = proceduralFunction->boundingBox;
            }
        }
        if (proceduralFunction) {
            proceduralFunction->renderGui(w);
            if ((boxToSet) && w.button("Set bounding box to the procedural SDF's default")) {
                if(boxToSet)
                    *boxToSet = proceduralFunction->boundingBox;
            }
        }
        else {
            w.text("No procedural function is selected");
        }
    }
    else if (sourceType == Source_Type::MeshCalc) {
        TriangleMesh::SharedPtr newMesh;
        static float3 boxSide = float3(0.5f);
        static uint2 paramRes = uint2(10, 10);
        w.var("box size/sphere radius", boxSide, 0.1f);
        w.var("parametric surface tesselation", paramRes, 2, 1000);
        if (w.button("Create cube mesh")) {
            newMesh = TriangleMesh::createCube(boxSide);
            newMesh->setName("Cube");
        }
        if (w.button("Create sphere mesh", true)) {
            newMesh = TriangleMesh::createSphere(boxSide.x, paramRes.x, paramRes.y);
            newMesh->setName("Sphere");
        }
        if (w.button("Load mesh from file...", true)) {
            std::filesystem::path path;
            if (openFileDialog({}, path))
            {
                newMesh = TriangleMesh::createFromFile(path);
                newMesh->setName(path.filename().string());
            }
        }

        if (newMesh) {
            if (mesh.initFromMesh(newMesh) && mesh.numTriangles != 0) {
                float3 innerSize = mesh.maxCorner - mesh.minCorner;
                float3 padding = innerSize / 6.f;
                BBox bb;
                bb.corner = mesh.minCorner - padding;
                bb.size = innerSize + 2.f * padding;
                if (boxToSet)
                    *boxToSet = bb;
            }
            else {
                msgBox("Error during mesh loading", MsgBoxType::Ok, MsgBoxIcon::Error);
            }
        }

        if (mesh.numTriangles == 0) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "No mesh is loaded");
        }
        else {
            ImGui::Text("Mesh name: %s\nNum. triangles: %i\nMin: %.3f, %.3f, %.3f\nMax: %.3f, %.3f, %.3f",
                mesh.name.c_str(), mesh.numTriangles,
                mesh.minCorner.x, mesh.minCorner.y, mesh.minCorner.z,
                mesh.maxCorner.x, mesh.maxCorner.y, mesh.maxCorner.z);
            if (w.button("Delete mesh")) {
                mesh.reset();
            }
        }
    }
    ImGui::Separator();
    ImGui::Unindent();
}

void SDF_Generation_Desc::renderGui(Gui::Widgets& w, ProceduralSDFList* sdfList, SDF* activeSDF)
{
    if (w.button("Reset generation settings")) {
        *this = SDF_Generation_Desc();
    }
    if (w.button("Res x2", true)) {
        dataDesc.resolution *= 2u;
    }
    if (w.button("Res /2", true)) {
        dataDesc.resolution /= 2u;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
    w.text("=== Source SDF settings ===");
    ImGui::PopStyleColor();
    sourceDesc.renderGui(w, &dataDesc.box, sdfList);
    if (sourceDesc.sourceType == Source_Type::MeshCalc) {
        auto t = dataDesc.type.sdfType;
        if (t != SDF_Type::SDF0) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Mesh input is unsupported for this SDF type.");
        }
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
    w.text("=== Output SDF settings ===");
    ImGui::PopStyleColor();
    dataDesc.renderGui(w);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
    w.text("=== Generation settings ===");
    ImGui::PopStyleColor();
    w.checkbox("Keep source SDF", keepSource);
    ImGui::HoverTooltip("Keep the source SDF in a program state,\nand create the new SDF in a new state");
    w.separator();
}

void Render_Settings::renderGui(Gui::Widgets& w, const SDF* activeSdf)
{
    if (w.button("Reset render settings")) {
        *this = Render_Settings();
    }
    w.checkbox("Render SDF", renderSDF);
    if (renderSDF && !activeSdf) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "   No SDF is loaded");
    }
    w.checkbox("Render SDF bounding box", renderSDFBBox);
    ImGui::HoverTooltip("The drawn bounding box is the one under 'Generate SDF'");

    w.separator(2);
    ImGui::Text("SDF trace render settings");
    if (w.button("-##maxStep")) primaryTraceStepNum--;
    if (w.button("+##maxStep", true)) primaryTraceStepNum++;
    if (w.button("32##maxStep", true)) primaryTraceStepNum = 32;
    if (w.button("1000##maxStep", true)) primaryTraceStepNum = 1000;
    w.var("maxStep", primaryTraceStepNum, 1u, 1000u, 1.0f, true);

    if (w.button("1e-3##traceEps", false)) { traceEpsilon = 1e-3f; }
    if (w.button("1e-4##traceEps", true)) { traceEpsilon = 1e-4f; }
    w.var("traceEpsilon", traceEpsilon, 0.0001f, 0.1f, 0.001f, true);

    if (w.button("1.2##relax", false)) { relaxedParam = 1.2f; }
    if (w.button("1.6##relax", true)) { relaxedParam = 1.6f; }
    w.var("relax param", relaxedParam, 1.0f, 2.0f, 0.001f, true);
    ImGui::HoverTooltip("Relaxation parameter for relaxed sphere tracing");

    if (w.button("0.88##relax", false)) { enhancedParam = 0.88f; }
    if (w.button("0.95##relax", true)) { enhancedParam = 0.95f; }
    w.var("enhanced param", enhancedParam, 0.01f, 1.0f, 0.001f, true);
    ImGui::HoverTooltip("Relaxation parameter for enhanced sphere tracing");

    if (w.button("0.2##relax", false)) { autoParam = 0.2f; }
    if (w.button("0.3##relax", true)) { autoParam = 0.3f; }
    w.var("auto param", autoParam, 0.01f, 0.99f, 0.001f, true);
    ImGui::HoverTooltip("Exponential averaging coefficient for relaxed sphere tracing");

    w.var("shade normal eps", shadeNormalEps, 0.0001f);
    if (activeSdf && w.button("Set to cell size", true)) {
        shadeNormalEps = activeSdf->desc.box.size / (float3)activeSdf->desc.resolution;
    }
    w.var("shadow normal eps", shadowNormalEps, 0.0001f);

    w.direction("light dir", lightDir);
    w.rgbColor("ambient color", colorAmbient);
    w.rgbColor("diffuse color", colorDiffuse);

}

void SDF::renderGui(Gui::Widgets& w) const
{
    ImGui::Text("Model name: %s", modelName.c_str());
    w.text("=== SDF descpriptor ===");
    desc.renderGuiConst(w);
    w.text("=== Current trace program ===");
    programDesc.renderGuiConst(w);
}

void SDF::setModelParameters(const ShaderVar& rootVar) const
{
    auto innerBox = desc.calcInnerBox();
    auto modelCB = rootVar.findMember("MODELcb");
    if (modelCB.isValid()) {
        modelCB["innerBoxCorner"] = innerBox.corner;
        modelCB["innerBoxSize"] = innerBox.size;
        modelCB["outerBoxCorner"] = desc.box.corner;
        modelCB["outerBoxSize"] = desc.box.size;
        modelCB["oneOverOuterBoxSize"] = 1.0f / desc.box.size;
        modelCB["resolution"] = desc.resolution;
        modelCB["resolution_r"] = 1.0f / float3(desc.resolution);
    }

    if (desc.type.sdfType == SDF_Type::SDF0) {
        auto texVar = rootVar.findMember("modelTex");
        if (texVar.isValid()) {
            texVar = texture;
        }
    }
}
