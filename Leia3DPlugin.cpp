#include "Leia3DPlugin.h"

#include "sr/management/srcontext.h"
#include "sr/sense/display/switchablehint.h"
#include "sr/utility/exception.h"
#include "sr/weaver/glweaver.h"

#define SRDISPLAY_LAZYBINDING
#include "sr/world/display/display.h"

#include <delayimp.h>

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

// Failed delay load is a structured exception, C++ catch does not see it, so it would crash application.
// It happens when LeiaSR Platform does not have DLL or function that plugin uses, for example when Platform is older than SDK.
static bool IsDelayLoadException(const DWORD ExceptionCode)
{
	return ExceptionCode == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND) ||
		   ExceptionCode == VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND);
}

namespace
{
	void SetOpenGLCapability(const GLenum Capability, const GLboolean bEnabled)
	{
		if (bEnabled)
		{
			FE_GL_ERROR(glEnable(Capability));
		}
		else
		{
			FE_GL_ERROR(glDisable(Capability));
		}
	}

	// ImGui OpenGL backend saves and restores OpenGL state only around its own rendering.
	// Weaving outside of it would leave state changed by weaver to the engine, so it is saved and restored here, similar to what backend does.
	struct OpenGLStateBackup
	{
		GLint DrawFramebuffer = 0;
		GLint ReadFramebuffer = 0;
		GLint Viewport[4] = { 0, 0, 0, 0 };
		GLint ScissorBox[4] = { 0, 0, 0, 0 };
		GLint Program = 0;
		GLint VertexArray = 0;
		GLint ArrayBuffer = 0;
		GLint ActiveTexture = 0;
		GLint FirstUnitTexture = 0;
		GLint BlendSourceRGB = 0;
		GLint BlendDestinationRGB = 0;
		GLint BlendSourceAlpha = 0;
		GLint BlendDestinationAlpha = 0;
		GLint BlendEquationRGB = 0;
		GLint BlendEquationAlpha = 0;
		GLboolean bBlend = GL_FALSE;
		GLboolean bCullFace = GL_FALSE;
		GLboolean bDepthTest = GL_FALSE;
		GLboolean bStencilTest = GL_FALSE;
		GLboolean bScissorTest = GL_FALSE;

		void Save()
		{
			FE_GL_ERROR(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &DrawFramebuffer));
			FE_GL_ERROR(glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &ReadFramebuffer));
			FE_GL_ERROR(glGetIntegerv(GL_VIEWPORT, Viewport));
			FE_GL_ERROR(glGetIntegerv(GL_SCISSOR_BOX, ScissorBox));
			FE_GL_ERROR(glGetIntegerv(GL_CURRENT_PROGRAM, &Program));
			FE_GL_ERROR(glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &VertexArray));
			FE_GL_ERROR(glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &ArrayBuffer));
			FE_GL_ERROR(glGetIntegerv(GL_ACTIVE_TEXTURE, &ActiveTexture));
			// Like in ImGui OpenGL backend, only texture of the first unit is saved.
			FE_GL_ERROR(glActiveTexture(GL_TEXTURE0));
			FE_GL_ERROR(glGetIntegerv(GL_TEXTURE_BINDING_2D, &FirstUnitTexture));
			FE_GL_ERROR(glGetIntegerv(GL_BLEND_SRC_RGB, &BlendSourceRGB));
			FE_GL_ERROR(glGetIntegerv(GL_BLEND_DST_RGB, &BlendDestinationRGB));
			FE_GL_ERROR(glGetIntegerv(GL_BLEND_SRC_ALPHA, &BlendSourceAlpha));
			FE_GL_ERROR(glGetIntegerv(GL_BLEND_DST_ALPHA, &BlendDestinationAlpha));
			FE_GL_ERROR(glGetIntegerv(GL_BLEND_EQUATION_RGB, &BlendEquationRGB));
			FE_GL_ERROR(glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &BlendEquationAlpha));
			bBlend = glIsEnabled(GL_BLEND);
			bCullFace = glIsEnabled(GL_CULL_FACE);
			bDepthTest = glIsEnabled(GL_DEPTH_TEST);
			bStencilTest = glIsEnabled(GL_STENCIL_TEST);
			bScissorTest = glIsEnabled(GL_SCISSOR_TEST);
		}

		void Restore() const
		{
			FE_GL_ERROR(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, DrawFramebuffer));
			FE_GL_ERROR(glBindFramebuffer(GL_READ_FRAMEBUFFER, ReadFramebuffer));
			FE_GL_ERROR(glViewport(Viewport[0], Viewport[1], Viewport[2], Viewport[3]));
			FE_GL_ERROR(glScissor(ScissorBox[0], ScissorBox[1], ScissorBox[2], ScissorBox[3]));
			FE_GL_ERROR(glUseProgram(static_cast<GLuint>(Program)));
			FE_GL_ERROR(glBindVertexArray(static_cast<GLuint>(VertexArray)));
			FE_GL_ERROR(glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(ArrayBuffer)));
			FE_GL_ERROR(glActiveTexture(GL_TEXTURE0));
			FE_GL_ERROR(glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(FirstUnitTexture)));
			FE_GL_ERROR(glActiveTexture(static_cast<GLenum>(ActiveTexture)));
			FE_GL_ERROR(glBlendEquationSeparate(static_cast<GLenum>(BlendEquationRGB), static_cast<GLenum>(BlendEquationAlpha)));
			FE_GL_ERROR(glBlendFuncSeparate(static_cast<GLenum>(BlendSourceRGB), static_cast<GLenum>(BlendDestinationRGB),
											static_cast<GLenum>(BlendSourceAlpha), static_cast<GLenum>(BlendDestinationAlpha)));
			SetOpenGLCapability(GL_BLEND, bBlend);
			SetOpenGLCapability(GL_CULL_FACE, bCullFace);
			SetOpenGLCapability(GL_DEPTH_TEST, bDepthTest);
			SetOpenGLCapability(GL_STENCIL_TEST, bStencilTest);
			SetOpenGLCapability(GL_SCISSOR_TEST, bScissorTest);
		}
	};
}

bool Leia3DPlugin::OnLoad()
{
	// LeiaSR DLLs are delay loaded.
	if (LoadLibraryA("SimulatedRealityCore.dll") == nullptr)
	{
		LOG.Add("Leia3DPlugin::OnLoad: LeiaSR Platform is not installed, plugin will stay inactive", "LEIA_3D_PLUGIN", FE_LOG_ERROR);
		return true;
	}

	bLeiaPlatformAvailable = true;

	// 3D mode stays off until application enables it.
	TryInitializeContext(false);
	return true;
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
		LensHint = SR::SwitchableLensHint::create(*Context);
		CreateWeaver();
		Context->initialize();
	}
	catch (const SR::ServerNotAvailableException&)
	{
		LOG.Add("Leia3DPlugin::InitializeContext: Could not connect to the LeiaSR service, it might not be running", "LEIA_3D_PLUGIN", FE_LOG_ERROR);
		Shutdown();
		return false;
	}
	catch (const std::exception& Exception)
	{
		LOG.Add("Leia3DPlugin::InitializeContext: LeiaSR SDK reported error: " + std::string(Exception.what()), "LEIA_3D_PLUGIN", FE_LOG_ERROR);
		Shutdown();
		return false;
	}

	ReportAndClearLeiaOpenGLErrors("Leia3DPlugin::InitializeContext");

	return true;
}

// __try can not be used in a function that has objects with destructors, so InitializeContext is wrapped.
bool Leia3DPlugin::TryInitializeContext(const bool bInitialLensState)
{
	__try
	{
		return InitializeContext(bInitialLensState);
	}
	__except (IsDelayLoadException(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		OnDelayLoadFailed();
		return false;
	}
}

void Leia3DPlugin::OnDelayLoadFailed()
{
	LOG.Add("Leia3DPlugin::OnDelayLoadFailed: LeiaSR Platform does not have everything that plugin uses, it might be older than LeiaSR SDK that plugin was built with, plugin will stay inactive", "LEIA_3D_PLUGIN", FE_LOG_ERROR);

	// SDK functions could be missing, so what was already created is not destroyed through them.
	Weaver = nullptr;
	LensHint = nullptr;
	Context = nullptr;
	bLeiaPlatformAvailable = false;
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

	if (StereoViewsFramebuffer != nullptr)
	{
		delete StereoViewsFramebuffer;
		StereoViewsFramebuffer = nullptr;
	}

	if (EyeResultFramebuffer != 0)
	{
		FE_GL_ERROR(glDeleteFramebuffers(1, &EyeResultFramebuffer));
		EyeResultFramebuffer = 0;
	}

	if (FinalResultFramebuffer != nullptr)
	{
		delete FinalResultFramebuffer;
		FinalResultFramebuffer = nullptr;
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

void Leia3DPlugin::OnBeforeRender()
{
	// Camera and transform systems already updated this frame, so eye cameras are updated right before rendering.
	UpdateEyeCameras();
}

void Leia3DPlugin::OnAfterRender()
{
	// Eye cameras were placed in OnBeforeRender, no system changes them in between.
	Render();
}

FETexture* Leia3DPlugin::GetFinalResult() const
{
	if (FinalResultFramebuffer == nullptr)
		return nullptr;

	return FinalResultFramebuffer->GetColorAttachment();
}

float Leia3DPlugin::GetWorldUnitsPerMillimeter() const
{
	return WorldUnitsPerMillimeter;
}

void Leia3DPlugin::SetWorldUnitsPerMillimeter(const float NewValue)
{
	if (NewValue <= 0.0f)
	{
		LOG.Add("Leia3DPlugin::SetWorldUnitsPerMillimeter: Value should be positive", "LEIA_3D_PLUGIN", FE_LOG_WARNING);
		return;
	}

	WorldUnitsPerMillimeter = NewValue;
}

bool Leia3DPlugin::AreEyeFrustumsVisible() const
{
	return bEyeFrustumsVisible;
}

void Leia3DPlugin::SetEyeFrustumsVisible(const bool bNewValue)
{
	bEyeFrustumsVisible = bNewValue;
}

bool Leia3DPlugin::IsMonitorVisible() const
{
	return bMonitorVisible;
}

void Leia3DPlugin::SetMonitorVisible(const bool bNewValue)
{
	bMonitorVisible = bNewValue;
}

FEEntity* Leia3DPlugin::GetMonitorEntity() const
{
	FEScene* Scene = GetBoundScene();
	if (Scene == nullptr)
		return nullptr;

	return Scene->GetEntity(MonitorEntityID);
}

bool Leia3DPlugin::Is3DMonitorAvailable() const
{
	if (Context == nullptr || Weaver == nullptr)
		return false;

	SR::IDisplayManager* DisplayManager = SR::TryGetDisplayManagerInstance(*Context);
	if (DisplayManager == nullptr)
		return false;

	SR::IDisplay* Display = DisplayManager->getPrimaryActiveSRDisplay();
	if (Display == nullptr || !Display->isValid())
		return false;

	const SR_recti DisplayLocation = Display->getLocation();
	return DisplayLocation.right > DisplayLocation.left && DisplayLocation.bottom > DisplayLocation.top;
}

FEEntity* Leia3DPlugin::GetLeftEyeEntity() const
{
	FEScene* Scene = GetBoundScene();
	if (Scene == nullptr)
		return nullptr;

	return Scene->GetEntity(LeftEyeID);
}

FEEntity* Leia3DPlugin::GetRightEyeEntity() const
{
	FEScene* Scene = GetBoundScene();
	if (Scene == nullptr)
		return nullptr;

	return Scene->GetEntity(RightEyeID);
}

void Leia3DPlugin::Render()
{
	if (Weaver == nullptr)
		return;

	FEScene* Scene = GetBoundScene();
	if (Scene == nullptr)
		return;

	FEEntity* LeftEye = Scene->GetEntity(LeftEyeID);
	FEEntity* RightEye = Scene->GetEntity(RightEyeID);
	if (LeftEye == nullptr || RightEye == nullptr)
		return;

	FEEntity* PreviousMainCamera = CAMERA_SYSTEM.GetMainCamera(Scene);

	CAMERA_SYSTEM.SetMainCamera(LeftEye);
	RENDERER.Render(Scene);

	CAMERA_SYSTEM.SetMainCamera(RightEye);
	RENDERER.Render(Scene);

	if (PreviousMainCamera != nullptr)
		CAMERA_SYSTEM.SetMainCamera(PreviousMainCamera);

	FETexture* LeftEyeResult = RENDERER.GetCameraResult(LeftEye);
	FETexture* RightEyeResult = RENDERER.GetCameraResult(RightEye);
	if (LeftEyeResult == nullptr || RightEyeResult == nullptr)
		return;

	if (LeftEyeResult->GetWidth() <= 0 || LeftEyeResult->GetHeight() <= 0)
		return;

	OpenGLStateBackup StateBackup;
	StateBackup.Save();

	// Creating textures and framebuffers also changes OpenGL state, so it is done after state is saved.
	UpdateStereoViewsTexture(LeftEyeResult->GetWidth(), LeftEyeResult->GetHeight());
	if (StereoViewsFramebuffer == nullptr)
	{
		StateBackup.Restore();
		return;
	}

	// Copy each eye result into its half of the stereo views texture.
	// Blit is affected by scissor test.
	FE_GL_ERROR(glDisable(GL_SCISSOR_TEST));

	// Bind sets both draw and read framebuffers, so read framebuffer is replaced right after.
	StereoViewsFramebuffer->Bind();
	FE_GL_ERROR(glBindFramebuffer(GL_READ_FRAMEBUFFER, EyeResultFramebuffer));

	FETexture* EyeResults[2] = { LeftEyeResult, RightEyeResult };
	for (int i = 0; i < 2; i++)
	{
		FE_GL_ERROR(glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, EyeResults[i]->GetTextureID(), 0));
		// Linear filter, in case for one frame right eye result has different size than left one.
		FE_GL_ERROR(glBlitFramebuffer(0, 0, EyeResults[i]->GetWidth(), EyeResults[i]->GetHeight(),
									  i * StereoViewsViewWidth, 0, (i + 1) * StereoViewsViewWidth, StereoViewsViewHeight,
									  GL_COLOR_BUFFER_BIT, GL_LINEAR));
	}

	// Eye result textures are owned by renderer, so they should not stay attached here.
	FE_GL_ERROR(glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0));

	StereoViewsFramebuffer->UnBind();
	StateBackup.Restore();

	RenderFinalResult();

	FEViewport* DefaultViewport = ENGINE.GetDefaultViewport();
	if (DefaultViewport != nullptr && DefaultViewport->GetID() == BoundViewportID)
		CopyFinalResultToMainWindow();
}

void Leia3DPlugin::CopyFinalResultToMainWindow()
{
	if (FinalResultFramebuffer == nullptr)
		return;

	OpenGLStateBackup StateBackup;
	StateBackup.Save();

	// Final result has the size of the main window framebuffer, so it is copied texel per pixel and every woven pixel lands on the pixel it was woven for.
	// Copy is not affected by depth test, so screen quads that renderer draws into main window for eye cameras with default viewport do not matter.
	// Blit is affected by scissor test.
	FE_GL_ERROR(glDisable(GL_SCISSOR_TEST));

	// Bind sets both draw and read framebuffers, so draw framebuffer is replaced right after.
	FinalResultFramebuffer->Bind();
	FE_GL_ERROR(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));

	const int Width = FinalResultFramebuffer->GetWidth();
	const int Height = FinalResultFramebuffer->GetHeight();
	FE_GL_ERROR(glBlitFramebuffer(0, 0, Width, Height, 0, 0, Width, Height, GL_COLOR_BUFFER_BIT, GL_NEAREST));

	FinalResultFramebuffer->UnBind();
	StateBackup.Restore();
}

void Leia3DPlugin::RenderFinalResult()
{
	FEViewport* Viewport = ENGINE.GetViewport(BoundViewportID);
	if (Viewport == nullptr || Viewport->GetWidth() <= 0 || Viewport->GetHeight() <= 0)
		return;

	FEWindow* MainWindow = APPLICATION.GetMainWindow();
	if (MainWindow == nullptr)
		return;

	HWND WindowHandle = static_cast<HWND>(MainWindow->GetPlatformWindowHandle());
	if (WindowHandle == nullptr)
		return;

	// Client area of the window is the default framebuffer, its size is in pixels.
	RECT ClientRectangle = { 0, 0, 0, 0 };
	if (!GetClientRect(WindowHandle, &ClientRectangle))
		return;

	const int FramebufferWidth = ClientRectangle.right - ClientRectangle.left;
	const int FramebufferHeight = ClientRectangle.bottom - ClientRectangle.top;
	if (FramebufferWidth <= 0 || FramebufferHeight <= 0)
		return;

	// Rectangle that eye cameras were rendered for, in framebuffer pixels, with top-left origin like in ImGui.
	// Viewport is in window client coordinates, like in UpdateEyeCameras.
	const ImVec2 Scale = ImGui::GetIO().DisplayFramebufferScale;
	const int PixelX = static_cast<int>(Viewport->GetX() * Scale.x);
	const int PixelY = static_cast<int>(Viewport->GetY() * Scale.y);
	const int PixelWidth = static_cast<int>(Viewport->GetWidth() * Scale.x);
	const int PixelHeight = static_cast<int>(Viewport->GetHeight() * Scale.y);

	OpenGLStateBackup StateBackup;
	StateBackup.Save();

	// Creating textures and framebuffers also changes OpenGL state, so it is done after state is saved.
	UpdateFinalResult(FramebufferWidth, FramebufferHeight);
	if (FinalResultFramebuffer == nullptr)
	{
		StateBackup.Restore();
		return;
	}

	FinalResultFramebuffer->Bind();

	// LeiaSR OpenGL weaver clips its output to scissor rectangle, but reads its Y from the top of the window.
	// Scissor rectangle of the whole texture is the same in both conventions.
	FE_GL_ERROR(glDisable(GL_SCISSOR_TEST));
	FE_GL_ERROR(glScissor(0, 0, FinalResultFramebuffer->GetWidth(), FinalResultFramebuffer->GetHeight()));

	// Weaver draws nothing while window is occluded, and window could move since the previous frame, so old "3D" pixels should not stay.
	const GLfloat Black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	FE_GL_ERROR(glClearBufferfv(GL_COLOR, 0, Black));

	// ImGui uses top-left origin, OpenGL uses bottom-left origin.
	FE_GL_ERROR(glViewport(PixelX, FinalResultFramebuffer->GetHeight() - PixelY - PixelHeight, PixelWidth, PixelHeight));

	Weaver->weave();
	ReportAndClearLeiaOpenGLErrors("Leia3DPlugin::RenderFinalResult");

	FinalResultFramebuffer->UnBind();
	StateBackup.Restore();
}

void Leia3DPlugin::UpdateStereoViewsTexture(const int ViewWidth, const int ViewHeight)
{
	if (StereoViewsFramebuffer != nullptr && ViewWidth == StereoViewsViewWidth && ViewHeight == StereoViewsViewHeight)
		return;

	if (EyeResultFramebuffer == 0)
		FE_GL_ERROR(glGenFramebuffers(1, &EyeResultFramebuffer));

	delete StereoViewsFramebuffer;
	StereoViewsFramebuffer = RESOURCE_MANAGER.CreateFramebuffer(0, ViewWidth * 2, ViewHeight);
	if (StereoViewsFramebuffer == nullptr)
		return;

	FETexture* StereoViewsTexture = RESOURCE_MANAGER.CreateTexture(GL_RGBA8, GL_RGBA, ViewWidth * 2, ViewHeight, true, "Leia3DStereoViews");
	StereoViewsFramebuffer->SetColorAttachment(StereoViewsTexture);

	StereoViewsViewWidth = ViewWidth;
	StereoViewsViewHeight = ViewHeight;
	Weaver->setInputViewTexture(StereoViewsTexture->GetTextureID(), StereoViewsViewWidth, StereoViewsViewHeight, GL_RGBA8);
}

void Leia3DPlugin::UpdateFinalResult(const int Width, const int Height)
{
	if (FinalResultFramebuffer != nullptr && FinalResultFramebuffer->GetWidth() == Width && FinalResultFramebuffer->GetHeight() == Height)
		return;

	delete FinalResultFramebuffer;
	FinalResultFramebuffer = RESOURCE_MANAGER.CreateFramebuffer(0, Width, Height);
	if (FinalResultFramebuffer == nullptr)
		return;

	FETexture* FinalResult = RESOURCE_MANAGER.CreateTexture(GL_RGB8, GL_RGB, Width, Height, true, "Leia3DFinalResult");
	// Each "3D" pixel is meant for one exact physical pixel, so it should not be mixed with its neighbors.
	FinalResult->SetFilterType(FE_TEXTURE_MINMAG_FILTER_TYPE::NEAREST);
	FinalResultFramebuffer->SetColorAttachment(FinalResult);

	FinalResultFramebuffer->Bind();
	const GLenum FramebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (FramebufferStatus != GL_FRAMEBUFFER_COMPLETE)
		LOG.Add("Leia3DPlugin::UpdateFinalResult: Framebuffer is not complete, status: " + std::to_string(FramebufferStatus), "LEIA_3D_PLUGIN", FE_LOG_ERROR);

	FinalResultFramebuffer->UnBind();
}

void Leia3DPlugin::UpdateEyeCameras()
{
	if (Context == nullptr || Weaver == nullptr)
		return;

	FEScene* Scene = GetBoundScene();
	if (Scene == nullptr)
		return;

	FEEntity* MonitorEntity = Scene->GetEntity(MonitorEntityID);
	FEEntity* LeftEye = Scene->GetEntity(LeftEyeID);
	FEEntity* RightEye = Scene->GetEntity(RightEyeID);
	if (MonitorEntity == nullptr || LeftEye == nullptr || RightEye == nullptr)
		return;

	FEViewport* Viewport = ENGINE.GetViewport(BoundViewportID);
	if (Viewport == nullptr || Viewport->GetWidth() <= 0 || Viewport->GetHeight() <= 0)
		return;

	FEWindow* MainWindow = APPLICATION.GetMainWindow();
	if (MainWindow == nullptr)
		return;

	HWND WindowHandle = static_cast<HWND>(MainWindow->GetPlatformWindowHandle());
	if (WindowHandle == nullptr)
		return;

	SR::IDisplayManager* DisplayManager = SR::TryGetDisplayManagerInstance(*Context);
	if (DisplayManager == nullptr)
	{
		LOG.Add("Leia3DPlugin::UpdateEyeCameras: Display manager is not available", "LEIA_3D_PLUGIN", FE_LOG_WARNING);
		return;
	}

	SR::IDisplay* Display = DisplayManager->getPrimaryActiveSRDisplay();
	if (Display == nullptr || !Display->isValid())
		return;

	// Eye tracker uses display coordinates: millimeters, origin in the center of the display, X to the right, Y up, Z towards the viewer.
	// Location of the display is in desktop pixels, physical size is in centimeters.
	const SR_recti DisplayLocation = Display->getLocation();
	const double DisplayWidthInPixels = static_cast<double>(DisplayLocation.right - DisplayLocation.left);
	const double DisplayHeightInPixels = static_cast<double>(DisplayLocation.bottom - DisplayLocation.top);
	if (DisplayWidthInPixels <= 0.0 || DisplayHeightInPixels <= 0.0)
		return;

	const double MillimetersPerPixelX = Display->getPhysicalSizeWidth() * 10.0 / DisplayWidthInPixels;
	const double MillimetersPerPixelY = Display->getPhysicalSizeHeight() * 10.0 / DisplayHeightInPixels;

	// 3D area is the viewport, not the whole display. Its rectangle is in window client pixels, so it is moved to desktop pixels first.
	POINT ClientOrigin = { 0, 0 };
	ClientToScreen(WindowHandle, &ClientOrigin);
	const double AreaCenterXInPixels = ClientOrigin.x + Viewport->GetX() + Viewport->GetWidth() / 2.0;
	const double AreaCenterYInPixels = ClientOrigin.y + Viewport->GetY() + Viewport->GetHeight() / 2.0;

	// Desktop Y goes down, display Y goes up.
	const glm::vec3 AreaCenter = glm::vec3(static_cast<float>((AreaCenterXInPixels - (DisplayLocation.left + DisplayLocation.right) / 2.0) * MillimetersPerPixelX),
										   static_cast<float>(-(AreaCenterYInPixels - (DisplayLocation.top + DisplayLocation.bottom) / 2.0) * MillimetersPerPixelY),
										   0.0f);
	const float AreaWidth = static_cast<float>(Viewport->GetWidth() * MillimetersPerPixelX);
	const float AreaHeight = static_cast<float>(Viewport->GetHeight() * MillimetersPerPixelY);

	glm::vec3 LeftEyePosition = glm::vec3(0.0f);
	glm::vec3 RightEyePosition = glm::vec3(0.0f);
	Weaver->getPredictedEyePositions(&LeftEyePosition.x, &RightEyePosition.x);

	// When nobody is tracked, eyes could be at zero, then default viewing position of the display is used.
	if (LeftEyePosition.z < 1.0f || RightEyePosition.z < 1.0f)
	{
		glm::vec3 DefaultViewingPosition = glm::vec3(0.0f);
		Display->getDefaultViewingPosition(DefaultViewingPosition.x, DefaultViewingPosition.y, DefaultViewingPosition.z);
		// Average distance between eyes is about 64 millimeters.
		const glm::vec3 HalfEyeDistance = glm::vec3(32.0f, 0.0f, 0.0f);
		LeftEyePosition = DefaultViewingPosition - HalfEyeDistance;
		RightEyePosition = DefaultViewingPosition + HalfEyeDistance;
	}

	UpdateEyeCamera(MonitorEntity, LeftEye, LeftEyePosition - AreaCenter, AreaWidth, AreaHeight);
	UpdateEyeCamera(MonitorEntity, RightEye, RightEyePosition - AreaCenter, AreaWidth, AreaHeight);

	// Camera system resets eye projections at the beginning of each frame, so frustums are drawn right after off-axis projections are set.
	// Edges of both frustums go through the corners of the 3D monitor area, so that is where they cross.
	if (bEyeFrustumsVisible)
	{
		RENDERER.DebugDrawFrustum(LeftEye, glm::vec3(1.0f, 0.2f, 0.2f));
		RENDERER.DebugDrawFrustum(RightEye, glm::vec3(0.2f, 0.8f, 1.0f));
	}

	if (bMonitorVisible)
	{
		// AreaCenter is the center of the 3D monitor area relative to the center of the display, so the display center relative to the 3D area is the opposite.
		const glm::vec2 DisplaySize = glm::vec2(Display->getPhysicalSizeWidth(), Display->getPhysicalSizeHeight()) * 10.0f;
		DebugDrawRectangleOnMonitor(MonitorEntity, -glm::vec2(AreaCenter), DisplaySize, glm::vec3(0.8f));
		DebugDrawRectangleOnMonitor(MonitorEntity, glm::vec2(0.0f), glm::vec2(AreaWidth, AreaHeight), glm::vec3(1.0f, 0.8f, 0.2f));
	}
}

void Leia3DPlugin::UpdateEyeCamera(FEEntity* MonitorEntity, FEEntity* Eye, const glm::vec3 EyePosition, const float AreaWidth, const float AreaHeight)
{
	if (MonitorEntity == nullptr || Eye == nullptr || EyePosition.z <= 0.0f)
		return;

	FETransformComponent& EyeTransform = Eye->GetComponent<FETransformComponent>();
	FECameraComponent& EyeCamera = Eye->GetComponent<FECameraComponent>();

	// Eye camera is a child of MonitorEntity, which is the center of the 3D area, with the area in its local XY plane, facing +Z.
	// Eye keeps default orientation, so it looks along -Z, straight at the plane of the area.
	EyeTransform.SetPosition(EyePosition * WorldUnitsPerMillimeter);

	// Transform and camera systems already updated this frame, so world and view matrices are updated here.
	const glm::mat4 EyeWorldMatrix = MonitorEntity->GetComponent<FETransformComponent>().GetWorldMatrix() * EyeTransform.GetLocalMatrix();
	EyeTransform.ForceSetWorldMatrix(EyeWorldMatrix);
	EyeCamera.SetViewMatrix(glm::inverse(EyeWorldMatrix));

	// Off-axis projection (Kooima, "Generalized Perspective Projection"): frustum goes from the eye through the edges of the 3D area.
	// In eye space area edges are just offsets from the eye, scaled to the near plane. Only ratios are used, so millimeters are fine here.
	const float NearPlane = EyeCamera.GetNearPlane();
	const float ToNearPlane = NearPlane / EyePosition.z;
	const float Left = (-AreaWidth / 2.0f - EyePosition.x) * ToNearPlane;
	const float Right = (AreaWidth / 2.0f - EyePosition.x) * ToNearPlane;
	const float Bottom = (-AreaHeight / 2.0f - EyePosition.y) * ToNearPlane;
	const float Top = (AreaHeight / 2.0f - EyePosition.y) * ToNearPlane;
	EyeCamera.SetProjectionMatrix(glm::frustum(Left, Right, Bottom, Top, NearPlane, EyeCamera.GetFarPlane()));
}

void Leia3DPlugin::DebugDrawRectangleOnMonitor(FEEntity* MonitorEntity, const glm::vec2 Center, const glm::vec2 Size, const glm::vec3 Color)
{
	if (MonitorEntity == nullptr)
		return;

	// Plane of the 3D area is the local XY plane of MonitorEntity, like in UpdateEyeCamera.
	const glm::mat4 MonitorWorldMatrix = MonitorEntity->GetComponent<FETransformComponent>().GetWorldMatrix();
	const glm::vec2 HalfSize = Size / 2.0f;
	const glm::vec2 LocalCorners[4] = { Center + glm::vec2(-HalfSize.x, -HalfSize.y),
										Center + glm::vec2(HalfSize.x, -HalfSize.y),
										Center + glm::vec2(HalfSize.x, HalfSize.y),
										Center + glm::vec2(-HalfSize.x, HalfSize.y) };

	glm::vec3 WorldCorners[4];
	for (int i = 0; i < 4; i++)
		WorldCorners[i] = glm::vec3(MonitorWorldMatrix * glm::vec4(glm::vec3(LocalCorners[i], 0.0f) * WorldUnitsPerMillimeter, 1.0f));

	for (int i = 0; i < 4; i++)
		RENDERER.DebugDrawLine(FELine(WorldCorners[i], WorldCorners[(i + 1) % 4], Color, 0.2f));
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