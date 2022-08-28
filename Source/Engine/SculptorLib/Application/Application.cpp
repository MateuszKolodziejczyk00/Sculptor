#include "Application.h"


namespace spt::lib
{

Application::Application()
{

}

void Application::Init(int argc, char** argv)
{
	OnInit(argc, argv);
}

void Application::Run()
{
	OnRun();
}

void Application::Shutdown()
{
	OnShutdown();
}

void Application::OnInit(int argc, char** argv)
{

}

void Application::OnRun()
{

}

void Application::OnShutdown()
{

}

}
