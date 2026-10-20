#include "Leia3DPlugin.h"

#include "sr/management/srcontext.h"
#include "sr/sense/display/switchablehint.h"
#include "sr/utility/exception.h"
#include "sr/weaver/glweaver.h"

// DO NOT CHANGE THIS LINE.
// The plugin manager uses it to create and destroy the plugin instance.
REGISTER_PLUGIN(Leia3DPlugin)

// LeiaSR SDK can leave OpenGL errors in the error queue. Engine would then report them
// in the next unrelated FE_GL_ERROR call, so they are logged and cleared right after SDK calls.
static void ReportAndClearLeiaOpenGLErrors(const std::string& FunctionName)
{
	// Limited number of iterations, in case glGetError keeps returning errors without a valid context.
	for (int i = 0; i < 32; i++)
	{
		const GLenum Error = glGetError();
		if (Error == GL_NO_ERROR)
			return;

		LOG.Add(FunctionName + ": LeiaSR SDK left OpenGL error: " + std::to_string(Error), "LEIA_3D_PLUGIN", FE_LOG_WARNING);
	}
}

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
	CreateWeaver();
	Context->initialize();
	ReportAndClearLeiaOpenGLErrors("Leia3DPlugin::InitializeContext");

	return true;
}

void Leia3DPlugin::CreateWeaver()
{
	FEWindow* MainWindow = APPLICATION.GetMainWindow();
	if (MainWindow == nullptr)
	{
		LOG.Add("Leia3DPlugin::CreateWeaver: Main window is nullptr", "LEIA_3D_PLUGIN", FE_LOG_ERROR);
		return;
	}

	HWND WindowHandle = static_cast<HWND>(MainWindow->GetPlatformWindowHandle());
	if (WindowHandle == nullptr)
	{
		LOG.Add("Leia3DPlugin::CreateWeaver: Main window has no platform window handle", "LEIA_3D_PLUGIN", FE_LOG_ERROR);
		return;
	}

	if (SR::CreateGLWeaver(*Context, WindowHandle, &Weaver) != WeaverErrorCode::WeaverSuccess)
	{
		LOG.Add("Leia3DPlugin::CreateWeaver: Failed to create OpenGL weaver", "LEIA_3D_PLUGIN", FE_LOG_ERROR);
		Weaver = nullptr;
		return;
	}

	// Stereo image is stored as sRGB values in GL_RGBA8 texture and window framebuffer is not sRGB,
	// so weaver should convert to linear after reading and back to sRGB before writing.
	Weaver->setShaderSRGBConversion(true, true);
}

void Leia3DPlugin::Shutdown()
{
	// Weaver uses both OpenGL context and SR context, so it should be destroyed first.
	if (Weaver != nullptr)
	{
		Weaver->destroy();
		Weaver = nullptr;
	}

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

bool Leia3DPlugin::BindToScene(FEScene* Scene, FEViewport* Viewport)
{
	if (Scene == nullptr || Viewport == nullptr)
	{
		LOG.Add("Leia3DPlugin::BindToScene: Scene or viewport is nullptr", "LEIA_3D_PLUGIN", FE_LOG_WARNING);
		return false;
	}

	Unbind();

	// Monitor entity is the 3D area of the monitor in the scene, eye cameras are its children.
	FEEntity* MonitorEntity = Scene->CreateEntity("Leia3DMonitor");

	FEEntity* LeftEye = Scene->CreateEntity("Leia3DLeftEye");
	LeftEye->AddComponent<FECameraComponent>();
	CAMERA_SYSTEM.SetCameraViewport(LeftEye, Viewport->GetID());
	MonitorEntity->AttachChild(LeftEye);

	FEEntity* RightEye = Scene->CreateEntity("Leia3DRightEye");
	RightEye->AddComponent<FECameraComponent>();
	CAMERA_SYSTEM.SetCameraViewport(RightEye, Viewport->GetID());
	MonitorEntity->AttachChild(RightEye);

	BoundSceneID = Scene->GetID();
	BoundViewportID = Viewport->GetID();
	MonitorEntityID = MonitorEntity->GetID();
	LeftEyeID = LeftEye->GetID();
	RightEyeID = RightEye->GetID();

	return true;
}

void Leia3DPlugin::Unbind()
{
	FEScene* Scene = GetBoundScene();
	if (Scene != nullptr)
	{
		// Entities could be already deleted by application.
		const FEUUID EntityIDs[3] = { LeftEyeID, RightEyeID, MonitorEntityID };
		for (int i = 0; i < 3; i++)
		{
			if (Scene->GetEntity(EntityIDs[i]) != nullptr)
				Scene->DeleteEntity(EntityIDs[i]);
		}
	}

	ClearBinding();
}

FEScene* Leia3DPlugin::GetBoundScene() const
{
	if (UNIQUE_ID.IsNull(BoundSceneID))
		return nullptr;

	return SCENE_MANAGER.GetSceneByID(BoundSceneID);
}

void Leia3DPlugin::ClearBinding()
{
	BoundSceneID = FEUUID();
	BoundViewportID = FEUUID();
	MonitorEntityID = FEUUID();
	LeftEyeID = FEUUID();
	RightEyeID = FEUUID();
}

void Leia3DPlugin::OnSceneDeleting(FEScene* Scene)
{
	// Entities are deleted together with the scene, so only binding is cleared.
	if (Scene != nullptr && Scene->GetID() == BoundSceneID)
		ClearBinding();
}

void Leia3DPlugin::OnUpdate(double DeltaTime)
{

}