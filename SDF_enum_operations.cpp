#include "SDF.h"

#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 32
#include "Utils/magic_enum.hpp"


template<typename ENUM>
Gui::DropdownList makeDropdownList() {
    Gui::DropdownList list(magic_enum::enum_count<ENUM>());
    constexpr auto entries = magic_enum::enum_entries<ENUM>();
    std::transform(entries.begin(), entries.end(), list.begin(),
        [](const auto& e)-> Gui::DropdownValue { return { (uint32_t)e.first, (std::string)e.second }; });
    return list;
}

const Gui::DropdownList SDF_Type_list = makeDropdownList<SDF_Type>();
const Gui::DropdownList Source_Type_list = makeDropdownList<Source_Type>();

template<typename ENUM>
bool Dropdown_template(Gui::Widgets& w, const char label[], ENUM& var, bool sameLine, const Gui::DropdownList& list)
{
    uint32_t uVar = (uint32_t)var;
    if (w.dropdown(label, list, uVar, sameLine)) {
        var = (ENUM)uVar;
        return true;
    }
    return false;
 }

bool Dropdown(Gui::Widgets& w, const char label[], SDF_Type& var, bool sameLine)
{
    return Dropdown_template(w, label, var, sameLine, SDF_Type_list);
}
bool Dropdown(Gui::Widgets& w, const char label[], Source_Type& var, bool sameLine)
{
    return Dropdown_template(w, label, var, sameLine, Source_Type_list);
}

std::ostream& operator<<(std::ostream& os, SDF_Type val)
{
    return magic_enum::ostream_operators::operator<<(os, val);
}
std::ostream& operator<<(std::ostream& os, Source_Type val)
{
    return magic_enum::ostream_operators::operator<<(os, val);
}
