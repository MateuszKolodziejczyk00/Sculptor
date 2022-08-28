#pragma once

#define SPT_CLIENT_APP(ClientApp) using ClientApplication = ClientApp;

#define SPT_APP_ENTRY() \
spt::lib::Application* CreateApplication() \
{ \
	return new ClientApplication{}; \
} \


#define SPT_CREATE_ENTRY_POINT(ClientApp) \
SPT_CLIENT_APP(ClientApp) \
SPT_APP_ENTRY()


extern spt::lib::Application* CreateApplication();


int main(int argc, char** argv)
{
	spt::lib::Application* app = CreateApplication();
	app->Init(argc, argv);
	app->Run();
	app->Shutdown();
	delete app;
}