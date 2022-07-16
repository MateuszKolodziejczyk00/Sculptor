#pragma once

#include "SculptorLib.h"

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

private:

};

}
