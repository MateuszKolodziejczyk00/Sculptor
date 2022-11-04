#pragma once

#include "SculptorLibMacros.h"


namespace spt::lib
{

class SCULPTOR_LIB_API Application
{
public:

	Application();
	virtual ~Application() = default;

	void Init(int argc, char** argv);

	void Run();

	void Shutdown();

protected:

	virtual void OnInit(int argc, char** argv);

	virtual void OnRun();

	virtual void OnShutdown();
};

}
