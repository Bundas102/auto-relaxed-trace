add_falcor_executable(SDFRenderer)

target_sources(SDFRenderer PRIVATE
	FlatMesh.cpp
	FlatMesh.h
	Main.cpp
	SDF.cpp
	SDF.h
	SDF_enum_operations.cpp
	SDFRenderer.cpp
	SDFRenderer.h
	
	Utils/ComputeProgramWrapper.cpp
	Utils/ComputeProgramWrapper.h
	Utils/GraphicsProgramWrapper.cpp
	Utils/GraphicsProgramWrapper.h
	Utils/hash_tuple.hpp
	Utils/magic_enum.hpp
)

# add all shaders automatically
file(GLOB_RECURSE SHADER_FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.slang"
)
target_sources(SDFRenderer PRIVATE ${SHADER_FILES})

target_copy_shaders(SDFRenderer Samples/SDFRenderer)

# this used to be `target_copy_data_folder(SDFRenderer)` in Falcor 5.2
add_custom_command(TARGET SDFRenderer PRE_BUILD 
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data ${FALCOR_OUTPUT_DIRECTORY}/data )

target_source_group(SDFRenderer "Samples")
