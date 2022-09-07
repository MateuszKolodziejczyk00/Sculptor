#include "SPIRVOptimizer.h"
#include "ShaderCompilationEnvironment.h"
#include "spirv-tools/optimizer.hpp"
#include "Logging/Log.h"

namespace spt::sc
{

SPT_IMPLEMENT_LOG_CATEGORY(SPIRVOptimizerLog, true)

namespace impl
{

static spv_target_env GetTargetEnv()
{
	const ETargetEnvironment env = ShaderCompilationEnvironment::GetTargetEnvironment();

	switch (env)
	{
	case ETargetEnvironment::Vulkan_1_3: return SPV_ENV_VULKAN_1_3;

	default:

		SPT_CHECK_NO_ENTRY();
		return SPV_ENV_MAX;
	}
}

void LogOptimizerMessage(spv_message_level_t level, const char* source, const spv_position_t& position, const char* message)
{
	switch (level)
	{
	case SPV_MSG_FATAL:
		SPT_LOG_FATAL(SPIRVOptimizerLog, "[Fatal]: {0}", message);
		break;

	case SPV_MSG_INTERNAL_ERROR:
		SPT_LOG_FATAL(SPIRVOptimizerLog, "[Internal Error]: {0}", message);
		break;

	case SPV_MSG_ERROR:
		SPT_LOG_ERROR(SPIRVOptimizerLog, "[Error]: {0}", message);
		break;

	case SPV_MSG_WARNING:
		SPT_LOG_WARN(SPIRVOptimizerLog, "[Warning]: {0}", message);
		break;

	case SPV_MSG_INFO:
		SPT_LOG_INFO(SPIRVOptimizerLog, "[Info]: {0}", message);
		break;

	case SPV_MSG_DEBUG:
		SPT_LOG_TRACE(SPIRVOptimizerLog, "[Debug]: {0}", message);
		break;
	}
}


class OptimizerImpl
{
public:

	OptimizerImpl();

	lib::DynamicArray<Uint32> OptimizeBinary(const lib::DynamicArray<Uint32>& binary) const;

private:

	spvtools::Optimizer optimizer;

};


OptimizerImpl::OptimizerImpl()
	: optimizer(GetTargetEnv())
{
	optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass())
			 .RegisterPass(spvtools::CreateEliminateDeadFunctionsPass())
			 .RegisterPass(spvtools::CreateEliminateDeadConstantPass())
			 .RegisterPass(spvtools::CreateLoopUnrollPass(true))
			 .RegisterPass(spvtools::CreateReduceLoadSizePass())
			 .RegisterPass(spvtools::CreateCombineAccessChainsPass())
			 .RegisterPass(spvtools::CreateInlineExhaustivePass())
			 .RegisterPass(spvtools::CreateDeadBranchElimPass())
			 .RegisterPass(spvtools::CreateLocalMultiStoreElimPass())
			 .RegisterPass(spvtools::CreateLocalAccessChainConvertPass())
			 .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
			 .RegisterPass(spvtools::CreateCFGCleanupPass());

	optimizer.SetMessageConsumer(&LogOptimizerMessage);
}

lib::DynamicArray<Uint32> OptimizerImpl::OptimizeBinary(const lib::DynamicArray<Uint32>& binary) const
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<Uint32> optimizedBinary;
	const Bool result = optimizer.Run(binary.data(), binary.size(), &optimizedBinary);
	SPT_CHECK(result);

	return optimizedBinary;
}

} // impl

lib::DynamicArray<Uint32> SPIRVOptimizer::OptimizeBinary(const lib::DynamicArray<Uint32>& binary)
{
	static impl::OptimizerImpl instance;

	return instance.OptimizeBinary(binary);
}

} // spt::sc
