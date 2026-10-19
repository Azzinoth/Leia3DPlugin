#include "Leia3DPlugin.h"

#include "sr/management/srcontext.h"
#include "sr/sense/display/switchablehint.h"
#include "sr/utility/exception.h"

// DO NOT CHANGE THIS LINE.
// The plugin manager uses it to create and destroy the plugin instance.
REGISTER_PLUGIN(Leia3DPlugin)

void Leia3DPlugin::OnLoad()
{
	// LeiaSR DLLs are delay loaded.
	if (LoadLibraryA("SimulatedRealityCore.dll") == nullptr)
	{
		LOG.Add("Leia3DPlugin::OnLoad: LeiaSR Platform is not installed, plugin will stay inactive", "LEIA_3D_PLUGIN", FE_LOG_ERROR);
		return;
	}

	bLeiaPlatformAvailable = true;

	// 3D mode stays off until application enables it.
	InitializeContext(false);
}

void Leia3DPlugin::OnUnload()
{
	Shutdown();
}

bool Leia3DPlugin::InitializeContext(const bool bInitialLensState)
{
	if (!bLeiaPlatformAvailable)
		return false;

	if (Context != nullptr)
		return true;

	try
	{
		Context = SR::SRContext::create(bInitialLensState);
	}
	catch (const SR::ServerNotAvailableException&)
	{
		LOG.Add("Leia3DPlugin::InitializeContext: Could not connect to the LeiaSR service, it might not be running", "LEIA_3D_PLUGIN", FE_LOG_ERROR);
		Context = nullptr;
		return false;
	}

	LensHint = SR::SwitchableLensHint::create(*Context);
	Context->initialize();

	return true;
}

void Leia3DPlugin::Shutdown()
{
	// LensHint is owned by Context, so it is deleted together with it.
	if (Context != nullptr)
	{
		SR::SRContext::deleteSRContext(Context);
		Context = nullptr;
		LensHint = nullptr;
	}
}

bool Leia3DPlugin::Set3DModeEnabled(const bool bNewValue)
{
	if (Context == nullptr)
		return false;

	bNewValue ? LensHint->enable() : LensHint->disable();

	b3DModeEnabled = bNewValue;
	return true;
}

bool Leia3DPlugin::Is3DModeEnabled() const
{
	return b3DModeEnabled;
}

void Leia3DPlugin::OnUpdate(double DeltaTime)
{

}