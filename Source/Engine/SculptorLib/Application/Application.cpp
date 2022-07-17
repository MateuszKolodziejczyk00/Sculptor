#include "Application.h"


namespace spt::lib
{

Application::Application()
{

}

void Application::Init()
{
	OnInit();
}

void Application::Run()
{
	OnRun();
}

void Application::Shutdown()
{
	OnShutdown();
}

void Application::OnInit()
{

}

void Application::OnRun()
{

}

void Application::OnShutdown()
{

}

}
