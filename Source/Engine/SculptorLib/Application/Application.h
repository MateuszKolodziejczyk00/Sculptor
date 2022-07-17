#pragma once

#include "SculptorLibMacros.h"


namespace spt::lib
{

class SCULPTORLIB_API Application
{
public:

	Application();
	virtual ~Application() = default;

	void Init();

	void Run();

	void Shutdown();

protected:

	virtual void OnInit();

	virtual void OnRun();

	virtual void OnShutdown();
};

}
