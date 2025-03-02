/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */ /*!
 * \file
 * \brief Subgroups Tests
 */ /*--------------------------------------------------------------------*/

#include "vktSubgroupsBallotMasksTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{

enum MaskType
{
	MASKTYPE_EQ = 0,
	MASKTYPE_GE,
	MASKTYPE_GT,
	MASKTYPE_LE,
	MASKTYPE_LT,
	MASKTYPE_LAST
};

struct CaseDefinition
{
	MaskType			maskType;
	VkShaderStageFlags	shaderStage;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				requiredSubgroupSize;
};

static bool checkVertexPipelineStages (const void*			internalData,
									   vector<const void*>	datas,
									   deUint32				width,
									   deUint32)
{
	DE_UNREF(internalData);

	return subgroups::check(datas, width, 0xf);
}

static bool checkComputeOrMesh (const void*			internalData,
								vector<const void*>	datas,
								const deUint32		numWorkgroups[3],
								const deUint32		localSize[3],
								deUint32)
{
	DE_UNREF(internalData);

	return subgroups::checkComputeOrMesh(datas, numWorkgroups, localSize, 0xf);
}

string getMaskTypeName (const MaskType maskType)
{
	switch (maskType)
	{
		case MASKTYPE_EQ:	return "gl_SubGroupEqMaskARB";
		case MASKTYPE_GE:	return "gl_SubGroupGeMaskARB";
		case MASKTYPE_GT:	return "gl_SubGroupGtMaskARB";
		case MASKTYPE_LE:	return "gl_SubGroupLeMaskARB";
		case MASKTYPE_LT:	return "gl_SubGroupLtMaskARB";
		default:			TCU_THROW(InternalError, "Unsupported mask type");
	}
}

string getBodySource (const CaseDefinition& caseDef)
{
	string	body	=
		"  uint64_t value = " + getMaskTypeName(caseDef.maskType) + ";\n"
		"  bool temp = true;\n";

	switch(caseDef.maskType)
	{
		case MASKTYPE_EQ:
			body += "  uint64_t mask = uint64_t(1) << gl_SubGroupInvocationARB;\n"
					"  temp = (value & mask) != 0;\n";
			break;
		case MASKTYPE_GE:
			body += "  for (uint i = 0; i < gl_SubGroupSizeARB; i++) {\n"
					"    uint64_t mask = uint64_t(1) << i;\n"
					"    if (i >= gl_SubGroupInvocationARB && (value & mask) == 0)\n"
					"       temp = false;\n"
					"    if (i < gl_SubGroupInvocationARB && (value & mask) != 0)\n"
					"       temp = false;\n"
					"  };\n";
			break;
		case MASKTYPE_GT:
			body += "  for (uint i = 0; i < gl_SubGroupSizeARB; i++) {\n"
					"    uint64_t mask = uint64_t(1) << i;\n"
					"    if (i > gl_SubGroupInvocationARB && (value & mask) == 0)\n"
					"       temp = false;\n"
					"    if (i <= gl_SubGroupInvocationARB && (value & mask) != 0)\n"
					"       temp = false;\n"
					"  };\n";
			break;
		case MASKTYPE_LE:
			body += "  for (uint i = 0; i < gl_SubGroupSizeARB; i++) {\n"
					"    uint64_t mask = uint64_t(1) << i;\n"
					"    if (i <= gl_SubGroupInvocationARB && (value & mask) == 0)\n"
					"       temp = false;\n"
					"    if (i > gl_SubGroupInvocationARB && (value & mask) != 0)\n"
					"       temp = false;\n"
					"  };\n";
			break;
		case MASKTYPE_LT:
			body += "  for (uint i = 0; i < gl_SubGroupSizeARB; i++) {\n"
					"    uint64_t mask = uint64_t(1) << i;\n"
					"    if (i < gl_SubGroupInvocationARB && (value & mask) == 0)\n"
					"       temp = false;\n"
					"    if (i >= gl_SubGroupInvocationARB && (value & mask) != 0)\n"
					"       temp = false;\n"
					"  };\n";
			break;
		default:
			TCU_THROW(InternalError, "Unknown mask type");
	}

	body += "  uint tempResult = temp ? 0xf : 0x2;\n";
	body += "  tempRes = tempResult;\n";

	return body;
}

string getExtHeader (const CaseDefinition&)
{
	return
		"#extension GL_ARB_shader_ballot: enable\n"
		"#extension GL_ARB_gpu_shader_int64: enable\n";
}

vector<string> getPerStageHeadDeclarations (const CaseDefinition& caseDef)
{
	const deUint32	stageCount	= subgroups::getStagesCount(caseDef.shaderStage);
	const bool		fragment	= (caseDef.shaderStage & VK_SHADER_STAGE_FRAGMENT_BIT) != 0;
	vector<string>	result		(stageCount, string());

	if (fragment)
		result.reserve(result.size() + 1);

	for (size_t i = 0; i < result.size(); ++i)
	{
		result[i] =
			"layout(set = 0, binding = " + de::toString(i) + ", std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n";
	}

	if (fragment)
	{
		const string	fragPart	=
			"layout(location = 0) out uint result;\n";

		result.push_back(fragPart);
	}

	return result;
}

vector<string> getFramebufferPerStageHeadDeclarations (const CaseDefinition& caseDef)
{
	vector<string>	result;

	DE_UNREF(caseDef);

	result.push_back("layout(location = 0) out float result;\n");
	result.push_back("layout(location = 0) out float out_color;\n");
	result.push_back("layout(location = 0) out float out_color[];\n");
	result.push_back("layout(location = 0) out float out_color;\n");

	return result;
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);
	const string				extHeader			= getExtHeader(caseDef);
	const string				testSrc				= getBodySource(caseDef);
	const vector<string>		headDeclarations	= getFramebufferPerStageHeadDeclarations(caseDef);
	const bool					pointSizeSupported	= *caseDef.geometryPointSizeSupported;

	subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, VK_FORMAT_R32_UINT, pointSizeSupported, extHeader, testSrc, "", headDeclarations);
}

void initPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
#ifndef CTS_USES_VULKANSC
	const bool					spirv14required		= (isAllRayTracingStages(caseDef.shaderStage) || isAllMeshShadingStages(caseDef.shaderStage));
#else
	const bool					spirv14required		= false;
#endif // CTS_USES_VULKANSC
	const SpirvVersion			spirvVersion		= (spirv14required ? SPIRV_VERSION_1_4 : SPIRV_VERSION_1_3);
	const ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, spirvVersion, 0u, spirv14required);
	const string				extHeader			= getExtHeader(caseDef);
	const string				testSrc				= getBodySource(caseDef);
	const vector<string>		headDeclarations	= getPerStageHeadDeclarations(caseDef);
	const bool					pointSizeSupport	= *caseDef.geometryPointSizeSupported;

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, VK_FORMAT_R32_UINT, pointSizeSupport, extHeader, testSrc, "", headDeclarations);
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!context.requireDeviceFunctionality("VK_EXT_shader_subgroup_ballot"))
	{
		TCU_THROW(NotSupportedError, "Device does not support VK_EXT_shader_subgroup_ballot extension");
	}

	if (!subgroups::isInt64SupportedForDevice(context))
		TCU_THROW(NotSupportedError, "Int64 is not supported");

	if (caseDef.requiredSubgroupSize)
	{
		context.requireDeviceFunctionality("VK_EXT_subgroup_size_control");

#ifndef CTS_USES_VULKANSC
		const VkPhysicalDeviceSubgroupSizeControlFeatures&		subgroupSizeControlFeatures		= context.getSubgroupSizeControlFeatures();
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
#else
		const VkPhysicalDeviceSubgroupSizeControlFeaturesEXT&		subgroupSizeControlFeatures	= context.getSubgroupSizeControlFeaturesEXT();
		const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context.getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC

		if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes nor required subgroup size");

		if (subgroupSizeControlFeatures.computeFullSubgroups == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support full subgroups in compute shaders");

		if ((subgroupSizeControlProperties.requiredSubgroupSizeStages & caseDef.shaderStage) != caseDef.shaderStage)
			TCU_THROW(NotSupportedError, "Required subgroup size is not supported for shader stage");
	}

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);

#ifndef CTS_USES_VULKANSC
	if (isAllRayTracingStages(caseDef.shaderStage))
	{
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
	}
	else if (isAllMeshShadingStages(caseDef.shaderStage))
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
		context.requireDeviceFunctionality("VK_EXT_mesh_shader");

		if ((caseDef.shaderStage & VK_SHADER_STAGE_TASK_BIT_EXT) != 0u)
		{
			const auto& features = context.getMeshShaderFeaturesEXT();
			if (!features.taskShader)
				TCU_THROW(NotSupportedError, "Task shaders not supported");
		}
	}
#endif // CTS_USES_VULKANSC

	subgroups::supportedCheckShader(context, caseDef.shaderStage);
}

TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	switch (caseDef.shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages);
		default:											TCU_THROW(InternalError, "Unhandled shader stage");
	}
}

TestStatus test (Context& context, const CaseDefinition caseDef)
{
	const bool isCompute	= isAllComputeStages(caseDef.shaderStage);
#ifndef CTS_USES_VULKANSC
	const bool isMesh		= isAllMeshShadingStages(caseDef.shaderStage);
#else
	const bool isMesh		= false;
#endif // CTS_USES_VULKANSC
	DE_ASSERT(!(isCompute && isMesh));

	if (isCompute || isMesh)
	{
#ifndef CTS_USES_VULKANSC
		const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties	= context.getSubgroupSizeControlProperties();
#else
		const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties	= context.getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC
		TestLog&												log								= context.getTestContext().getLog();

		if (caseDef.requiredSubgroupSize == DE_FALSE)
		{
			if (isCompute)
				return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkComputeOrMesh);
			else
				return subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, nullptr, 0, nullptr, checkComputeOrMesh);
		}

		log << TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			TestStatus result (QP_TEST_RESULT_INTERNAL_ERROR, "Internal Error");

			if (isCompute)
				result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, DE_NULL, 0u, DE_NULL, checkComputeOrMesh, size);
			else
				result = subgroups::makeMeshTest(context, VK_FORMAT_R32_UINT, nullptr, 0u, nullptr, checkComputeOrMesh, size);

			if (result.getCode() != QP_TEST_RESULT_PASS)
			{
				log << TestLog::Message << "subgroupSize " << size << " failed" << TestLog::EndMessage;
				return result;
			}
		}

		return TestStatus::pass("OK");
	}
	else if (isAllGraphicsStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages	= subgroups::getPossibleGraphicsSubgroupStages(context, caseDef.shaderStage);

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages, stages);
	}
#ifndef CTS_USES_VULKANSC
	else if (isAllRayTracingStages(caseDef.shaderStage))
	{
		const VkShaderStageFlags	stages	= subgroups::getPossibleRayTracingSubgroupStages(context, caseDef.shaderStage);

		return subgroups::allRayTracingStages(context, VK_FORMAT_R32_UINT, DE_NULL, 0, DE_NULL, checkVertexPipelineStages, stages);
	}
#endif // CTS_USES_VULKANSC
	else
		TCU_THROW(InternalError, "Unknown stage or invalid stage set");
}
}

namespace vkt
{
namespace subgroups
{
TestCaseGroup* createSubgroupsBallotMasksTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup>	group				(new TestCaseGroup(testCtx, "ballot_mask"));
	de::MovePtr<TestCaseGroup>	groupARB			(new TestCaseGroup(testCtx, "ext_shader_subgroup_ballot"));
	de::MovePtr<TestCaseGroup>	graphicGroup		(new TestCaseGroup(testCtx, "graphics"));
	de::MovePtr<TestCaseGroup>	computeGroup		(new TestCaseGroup(testCtx, "compute"));
	de::MovePtr<TestCaseGroup>	framebufferGroup	(new TestCaseGroup(testCtx, "framebuffer"));
#ifndef CTS_USES_VULKANSC
	de::MovePtr<TestCaseGroup>	raytracingGroup		(new TestCaseGroup(testCtx, "ray_tracing"));
	de::MovePtr<TestCaseGroup>	meshGroup			(new TestCaseGroup(testCtx, "mesh"));
#endif // CTS_USES_VULKANSC
	const VkShaderStageFlags	fbStages[]			=
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};
#ifndef CTS_USES_VULKANSC
	const VkShaderStageFlags	meshStages[]		=
	{
		VK_SHADER_STAGE_MESH_BIT_EXT,
		VK_SHADER_STAGE_TASK_BIT_EXT,
	};
#endif // CTS_USES_VULKANSC
	const deBool				boolValues[]		=
	{
		DE_FALSE,
		DE_TRUE
	};

	for (int maskTypeIndex = 0; maskTypeIndex < MASKTYPE_LAST; ++maskTypeIndex)
	{
		const MaskType	maskType	= static_cast<MaskType>(maskTypeIndex);
		const string	mask		= de::toLower(getMaskTypeName(maskType));

		for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
		{
			const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
			const string			testName				= mask + (requiredSubgroupSize ? "_requiredsubgroupsize" : "");
			const CaseDefinition	caseDef					=
			{
				maskType,						//  MaskType			maskType;
				VK_SHADER_STAGE_COMPUTE_BIT,	//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				requiredSubgroupSize,			//  deBool				requiredSubgroupSize;
			};

			addFunctionCaseWithPrograms(computeGroup.get(), testName,supportedCheck, initPrograms, test, caseDef);
		}

#ifndef CTS_USES_VULKANSC
		for (size_t groupSizeNdx = 0; groupSizeNdx < DE_LENGTH_OF_ARRAY(boolValues); ++groupSizeNdx)
		{
			for (const auto& stage : meshStages)
			{
				const deBool			requiredSubgroupSize	= boolValues[groupSizeNdx];
				const string			testName				= mask + (requiredSubgroupSize ? "_requiredsubgroupsize" : "");
				const CaseDefinition	caseDef					=
				{
					maskType,						//  MaskType			maskType;
					stage,							//  VkShaderStageFlags	shaderStage;
					de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
					requiredSubgroupSize,			//  deBool				requiredSubgroupSize;
				};

				addFunctionCaseWithPrograms(meshGroup.get(), testName + "_" + getShaderStageName(stage),  supportedCheck, initPrograms, test, caseDef);
			}
		}
#endif // CTS_USES_VULKANSC

		{
			const CaseDefinition	caseDef		=
			{
				maskType,						//  MaskType			maskType;
				VK_SHADER_STAGE_ALL_GRAPHICS,	//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				DE_FALSE						//  deBool				requiredSubgroupSize;
			};

			addFunctionCaseWithPrograms(graphicGroup.get(), mask,  supportedCheck, initPrograms, test, caseDef);
		}

#ifndef CTS_USES_VULKANSC
		{
			const CaseDefinition	caseDef		=
			{
				maskType,						//  MaskType			maskType;
				SHADER_STAGE_ALL_RAY_TRACING,	//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				DE_FALSE						//  deBool				requiredSubgroupSize;
			};

			addFunctionCaseWithPrograms(raytracingGroup.get(), mask,  supportedCheck, initPrograms, test, caseDef);
		}
#endif // CTS_USES_VULKANSC

		for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(fbStages); ++stageIndex)
		{
			const CaseDefinition	caseDef		=
			{
				maskType,						//  MaskType			maskType;
				fbStages[stageIndex],			//  VkShaderStageFlags	shaderStage;
				de::SharedPtr<bool>(new bool),	//  de::SharedPtr<bool>	geometryPointSizeSupported;
				DE_FALSE						//  deBool				requiredSubgroupSize;
			};
			const string			testName	= mask + "_" + getShaderStageName(caseDef.shaderStage);

			addFunctionCaseWithPrograms(framebufferGroup.get(), testName,supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
		}
	}

	groupARB->addChild(graphicGroup.release());
	groupARB->addChild(computeGroup.release());
	groupARB->addChild(framebufferGroup.release());
#ifndef CTS_USES_VULKANSC
	groupARB->addChild(raytracingGroup.release());
	groupARB->addChild(meshGroup.release());
#endif // CTS_USES_VULKANSC
	group->addChild(groupARB.release());

	return group.release();
}

} // subgroups
} // vkt
