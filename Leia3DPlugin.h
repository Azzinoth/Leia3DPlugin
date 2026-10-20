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

	float WorldUnitsPerMillimeter = 0.01f;
	bool bEyeFrustumsVisible = false;
	bool bMonitorVisible = false;
	// Places eye cameras according to eye tracker and sets their off-axis projections.
	void UpdateEyeCameras();
	// EyePosition is in millimeters, relative to the center of the 3D area.
	void UpdateEyeCamera(FEEntity* MonitorEntity, FEEntity* Eye, glm::vec3 EyePosition, float AreaWidth, float AreaHeight);
	// Rectangle in the plane of the 3D monitor area.
	void DebugDrawRectangleOnMonitor(FEEntity* MonitorEntity, glm::vec2 Center, glm::vec2 Size, glm::vec3 Color);

	// Renders bound scene from both eye cameras and weaves the result.
	void Render();

	// Left eye view in the left half, right eye view in the right half.
	FEFramebuffer* StereoViewsFramebuffer = nullptr;
	GLuint EyeResultFramebuffer = 0;
	int StereoViewsViewWidth = 0;
	int StereoViewsViewHeight = 0;
	void UpdateStereoViewsTexture(int ViewWidth, int ViewHeight);

	// Same size as window framebuffer, so "3D" pixels have the same coordinates as they would have in window framebuffer.
	// Its color attachment is the final result, framebuffer owns it and deletes it together with itself.
	FEFramebuffer* FinalResultFramebuffer = nullptr;
	void UpdateFinalResult(int Width, int Height);
	// Weaves stereo views into the final result, at the viewport rectangle that eye cameras were rendered for.
	void RenderFinalResult();

	bool InitializeContext(bool bInitialLensState);
	void CreateWeaver();
	void Shutdown();
public:
	void OnLoad() override;
	void OnUnload() override;
	void OnUpdate(double DeltaTime) override;
	void OnBeforeRender() override;
	void OnAfterRender() override;
	void OnSceneDeleting(FEScene* Scene) override;

	bool Set3DModeEnabled(bool bNewValue) override;
	bool Is3DModeEnabled() const override;

	bool BindToScene(FEScene* Scene, FEViewport* Viewport) override;
	void Unbind() override;
	FEScene* GetBoundScene() const override;

	FETexture* GetFinalResult() const override;
};