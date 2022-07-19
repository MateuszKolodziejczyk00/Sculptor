#pragma once

#if WITH_LOGGER
/**
 * Implements new log category. This must be added in .cpp file, and this category will be usable only in this .cpp file
 */
#define SPT_IMPLEMENT_LOG_CATEGORY(Category, Enabled)							\
class LogCategory_##Category##												\
{																			\
public:																		\
	static LogCategory_##Category##& Get()									\
	{																		\
		if (!s_instance)													\
		{																	\
			s_instance = new LogCategory_##Category##();					\
			s_instance->m_logger = spdlog::stdout_color_mt(#Category);		\
			s_instance->m_logger->set_level(spdlog::level::trace);			\
		}																	\
		return *s_instance;													\
	}																		\
	std::shared_ptr<spdlog::logger> m_logger;								\
	static LogCategory_##Category##* s_instance;							\
																			\
	static const Bool s_enabled = Enabled;											\
};																			\
LogCategory_##Category##* LogCategory_##Category##::s_instance = nullptr;



/**
 * Declares new log category that can be used in multiple source files
 * This category must be also implemented using IMPLEMENT_LOG_CATEGORY in.cpp file
 */
#define SPT_DECLARE_LOG_CATEGORY(Category)			\
class spdlog::m_logger;									\
class LogCategory_##Category##							\
{														\
public:													\
	static LogCategory_##Category##& Get();				\
	std::shared_ptr<spdlog::logger> m_logger;			\
	static LogCategory_##Category##* s_instance;		\
														\
	static const Bool s_enabled;						\
};


/**
 * Defines previously declared log category
 */
#define SPT_DEFINE_LOG_CATEGORY(Category, Enabled)								\
LogCategory_##Category##* LogCategory_##Category##::s_instance = nullptr;	\
LogCategory_##Category##* LogCategory_##Category##::s_enabled = Enabled;	\
LogCategory_##Category##& LogCategory_##Category##::Get()					\
{																			\
	if (!s_instance)														\
	{																		\
		s_instance = new LogCategory_##Category##();						\
		s_instance->m_logger = spdlog::stdout_color_mt(#Category);			\
		s_instance->m_logger->set_level(spdlog::level::trace);				\
	}																		\
	return *s_instance;														\
}


#define SPT_GET_LOGGER(Category)			LogCategory_##Category##::Get()

#define SPT_IS_LOG_CATEGORY_ENABLED(Category) (SPT_GET_LOGGER(Category).s_enabled)

#define SPT_LOG_IMPL(Category, Type, ...)	if(SPT_IS_LOG_CATEGORY_ENABLED(Category)) SPT_GET_LOGGER(Category).m_logger->Type(__VA_ARGS__)

#define SPT_LOG_TRACE(Category, ...)		SPT_LOG_IMPL(Category, trace, __VA_ARGS__)
#define SPT_LOG_INFO(Category, ...)			SPT_LOG_IMPL(Category, info, __VA_ARGS__)
#define SPT_LOG_WARN(Category, ...)			SPT_LOG_IMPL(Category, warn, __VA_ARGS__)
#define SPT_LOG_ERROR(Category, ...)		SPT_LOG_IMPL(Category, error, __VA_ARGS__)
#define SPT_LOG_FATAL(Category, ...)		SPT_LOG_IMPL(Category, fatal, __VA_ARGS__)

#else // WITH_LOGGER

#define SPT_IMPLEMENT_LOG_CATEGORY(Category, Enabled)
#define SPT_DECLARE_LOG_CATEGORY(Category)
#define SPT_DEFINE_LOG_CATEGORY(Category, Enabled)

#define SPT_IS_LOG_CATEGORY_ENABLED(Category) false

#define SPT_LOG_TRACE(Category, ...)
#define SPT_LOG_INFO(Category, ...)
#define SPT_LOG_WARN(Category, ...)
#define SPT_LOG_ERROR(Category, ...)
#define SPT_LOG_FATAL(Category, ...)

#endif // WITH_LOGGER