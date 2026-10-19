#include "SubSystems/FocalEngine/SubSystems/PluginSystem/FEPluginConnector.h"
#include "Public/Leia3DPluginInterface.h"
using namespace FocalEngine;

// Forward declarations, so that LeiaSR SDK headers are only needed in Leia3DPlugin.cpp.
namespace SR
{
	class SRContext;
	class SwitchableLensHint;
}

class Leia3DPlugin : public FEPluginCore, public Leia3DPluginInterface
{
	bool bLeiaPlatformAvailable = false;

	SR::SRContext* Context = nullptr;
	SR::SwitchableLensHint* LensHint = nullptr;
	bool b3DModeEnabled = false;

	bool InitializeContext(bool bInitialLensState);
	void Shutdown();
public:
	void OnLoad() override;
	void OnUnload() override;
	void OnUpdate(double DeltaTime) override;

	bool Set3DModeEnabled(bool bNewValue) override;
	bool Is3DModeEnabled() const override;
};