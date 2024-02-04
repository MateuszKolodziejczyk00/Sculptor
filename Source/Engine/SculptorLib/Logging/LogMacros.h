#pragma once

#include "SculptorAliases.h"


#if WITH_LOGGER
#define SPT_DEFINE_LOG_CATEGORY(Category, Enabled)							\
class LogCategory_##Category##												\
{																			\
public:																		\
																			\
	static LogCategory_##Category##& Get();									\
																			\
	spt::Bool IsEnabled() const { return m_enabled; }						\
																			\
	std::shared_ptr<spdlog::logger> m_logger;								\
	spt::Bool m_enabled;													\
																			\
private:																	\
																			\
	LogCategory_##Category##()												\
		: m_logger(spdlog::stdout_color_mt(#Category))						\
	{																		\
		m_logger->set_level(spdlog::level::trace);							\
		m_enabled = Enabled;												\
	}																		\
};																			\
inline LogCategory_##Category##& LogCategory_##Category##::Get()			\
{																			\
	static LogCategory_##Category instance;									\
	return instance;														\
}

#define SPT_GET_LOGGER(Category)			LogCategory_##Category##::Get()

#define SPT_IS_LOG_CATEGORY_ENABLED(Category) (SPT_GET_LOGGER(Category).IsEnabled())

#define SPT_LOG_IMPL(Category, Type, ...)	if(SPT_IS_LOG_CATEGORY_ENABLED(Category)) { SPT_GET_LOGGER(Category).m_logger->Type(__VA_ARGS__); }

#define SPT_LOG_TRACE(Category, ...)		SPT_LOG_IMPL(Category, trace, __VA_ARGS__)
#define SPT_LOG_INFO(Category, ...)			SPT_LOG_IMPL(Category, info, __VA_ARGS__)
#define SPT_LOG_WARN(Category, ...)			SPT_LOG_IMPL(Category, warn, __VA_ARGS__)
#define SPT_LOG_ERROR(Category, ...)		SPT_LOG_IMPL(Category, error, __VA_ARGS__)
#define SPT_LOG_FATAL(Category, ...)		SPT_LOG_IMPL(Category, critical, __VA_ARGS__)

#else // WITH_LOGGER

#define SPT_DEFINE_LOG_CATEGORY(Category, Enabled)

#define SPT_IS_LOG_CATEGORY_ENABLED(Category) false

#define SPT_LOG_TRACE(Category, ...)
#define SPT_LOG_INFO(Category, ...)
#define SPT_LOG_WARN(Category, ...)
#define SPT_LOG_ERROR(Category, ...)
#define SPT_LOG_FATAL(Category, ...)

#endif // WITH_LOGGER