#pragma once

#include "MeshBuilder.h"

#include "TinyGLTF.h"


namespace spt::rsc
{

class GLTFMeshBuilder : public MeshBuilder
{
protected:

	using Super = MeshBuilder;

public:

	explicit GLTFMeshBuilder(const MeshBuildParameters& parameters);

	void BuildMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model);

private:

	const tinygltf::Accessor* GetAttributeAccessor(const tinygltf::Primitive& prim, const tinygltf::Model& model, const lib::String& name);

	void SetIndices(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetLocations(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetNormals(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetTangents(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
	void SetUVs(const tinygltf::Accessor& accessor, const tinygltf::Model& model);

	template<typename TDestType>
	Uint32 AppendAccessorData(const tinygltf::Accessor& accessor, const tinygltf::Model& model);
};


template<typename TDestType>
Uint32 GLTFMeshBuilder::AppendAccessorData(const tinygltf::Accessor& accessor, const tinygltf::Model& model)
{
	SPT_PROFILER_FUNCTION();

	const Int32 bufferViewIdx = accessor.bufferView;
	SPT_CHECK(bufferViewIdx != -1);

	const tinygltf::BufferView& bufferView = model.bufferViews[bufferViewIdx];
	const Int32 bufferIdx = bufferView.buffer;
	SPT_CHECK(bufferIdx != -1);

	const tinygltf::Buffer& buffer = model.buffers[bufferIdx];

	const SizeType bufferByteOffset = accessor.byteOffset + bufferView.byteOffset;
	const Int32 strideAsInt = accessor.ByteStride(bufferView);
	SPT_CHECK(strideAsInt != -1);
	const SizeType stride = static_cast<SizeType>(strideAsInt);

	const SizeType elementsNum = accessor.count;
	const SizeType componentsNum = tinygltf::GetNumComponentsInType(accessor.type);
	const Int32 componentType = accessor.componentType;
	
	const unsigned char* data = buffer.data.data() + bufferByteOffset;

	switch (componentType)
	{
	case TINYGLTF_COMPONENT_TYPE_BYTE:
		AppendData<TDestType, char>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		AppendData<TDestType, unsigned char>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_SHORT:
		AppendData<TDestType, short>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		AppendData<TDestType, unsigned short>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_INT:
		AppendData<TDestType, Int32>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		AppendData<TDestType, Uint32>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_FLOAT:
		AppendData<TDestType, Real32>(data, componentsNum, stride, elementsNum);
		break;
	case TINYGLTF_COMPONENT_TYPE_DOUBLE:
		AppendData<TDestType, Real64>(data, componentsNum, stride, elementsNum);
		break;
	default:
		SPT_CHECK_NO_ENTRY();
	}

	// value must be lower because maxValue is reserved for "invalid"
	SPT_CHECK(elementsNum < maxValue<Uint32>);
	return static_cast<Uint32>(elementsNum);
}

} // spt::rsc
