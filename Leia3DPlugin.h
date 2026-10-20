#include "SubSystems/FocalEngine/SubSystems/PluginSystem/FEPluginConnector.h"
#include "Public/Leia3DPluginInterface.h"
using namespace FocalEngine;

// Forward declarations, so that LeiaSR SDK headers are only needed in Leia3DPlugin.cpp.
namespace SR
{
	class SRContext;
	class SwitchableLensHint;
	class IGLWeaver1;
}

class Leia3DPlugin : public FEPluginCore, public Leia3DPluginInterface
{
	bool bLeiaPlatformAvailable = false;

	SR::SRContext* Context = nullptr;
	SR::SwitchableLensHint* LensHint = nullptr;
	SR::IGLWeaver1* Weaver = nullptr;
	bool b3DModeEnabled = false;

	FEUUID BoundSceneID;
	FEUUID BoundViewportID;
	FEUUID MonitorEntityID;
	FEUUID LeftEyeID;
	FEUUID RightEyeID;
	void ClearBinding();

	bool InitializeContext(bool bInitialLensState);
	void CreateWeaver();
	void Shutdown();
public:
	void OnLoad() override;
	void OnUnload() override;
	void OnUpdate(double DeltaTime) override;
	void OnSceneDeleting(FEScene* Scene) override;

	bool Set3DModeEnabled(bool bNewValue) override;
	bool Is3DModeEnabled() const override;

	bool BindToScene(FEScene* Scene, FEViewport* Viewport) override;
	void Unbind() override;
	FEScene* GetBoundScene() const override;
};