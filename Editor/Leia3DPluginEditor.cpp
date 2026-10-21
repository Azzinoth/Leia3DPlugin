#include "Leia3DPluginEditor.h"

// DO NOT CHANGE THIS LINE.
// The plugin manager uses it to create and destroy the plugin instance.
REGISTER_PLUGIN(Leia3DPluginEditor)

// Windows, menu items and inspector sections should be added here through Editor, they are removed automatically when module is unloaded.
bool Leia3DPluginEditor::OnEditorLoad(FEEditorAPI* EditorAPI)
{
	Editor = EditorAPI;

	// Both modules are built from one project, so versions differ only if one of the DLLs is outdated.
	EngineModuleInterface* EngineModule = GetEngineModule();
	if (EngineModule == nullptr || EngineModule->GetInterfaceVersion() != EngineModuleInterface::InterfaceVersion)
	{
		LOG.Add("Leia3DPluginEditor::OnEditorLoad: Engine module is not available or was built with different version of Leia3DPluginInterface", "LEIA_3D_PLUGIN_EDITOR", FE_LOG_WARNING);
		return false;
	}

	Editor->AddWindow("Settings", "Leia 3D settings", [this]() { DrawSettingsWindow(); });

	// Viewport that is needed for binding is created when window is drawn, so window is shown from the start.
	const FEUUID RenderingWindowID = Editor->AddWindow("Rendering", "Leia 3D monitor rendering", [this]() { DrawRenderingWindow(); });
	Editor->SetWindowVisible(RenderingWindowID, true);

	return true;
}

void Leia3DPluginEditor::OnEditorUnload()
{
	// Bound viewport belongs to the window of this module, so binding should not outlive the module.
	GetEngineModule()->Unbind();
}

void Leia3DPluginEditor::OnEditorUpdate(double DeltaTime)
{

}

void Leia3DPluginEditor::DrawSettingsWindow()
{
	EngineModuleInterface* EngineModule = GetEngineModule();

	bool b3DModeEnabled = EngineModule->Is3DModeEnabled();
	if (ImGui::Checkbox("3D monitor mode", &b3DModeEnabled))
	{
		if (!EngineModule->Set3DModeEnabled(b3DModeEnabled))
			LOG.Add("Leia3DPluginEditor::DrawSettingsWindow: Failed to switch Leia 3D monitor mode, LeiaSR service might not be running", "LEIA_3D_PLUGIN_EDITOR", FE_LOG_WARNING);
	}

	// 3D image will be produced for the area of "Leia 3D monitor rendering" window.
	FEScene* FocusedScene = Editor->GetFocusedScene();
	FEViewport* Viewport = ENGINE.GetViewport(ViewportID);
	ImGui::BeginDisabled(FocusedScene == nullptr || Viewport == nullptr);
	if (ImGui::Button("Bind to focused scene"))
	{
		if (!EngineModule->BindToScene(FocusedScene, Viewport))
			LOG.Add("Leia3DPluginEditor::DrawSettingsWindow: Failed to bind Leia 3D plugin to focused scene", "LEIA_3D_PLUGIN_EDITOR", FE_LOG_WARNING);
	}
	ImGui::EndDisabled();

	bool bEyeFrustumsVisible = EngineModule->AreEyeFrustumsVisible();
	if (ImGui::Checkbox("Show eye frustums", &bEyeFrustumsVisible))
		EngineModule->SetEyeFrustumsVisible(bEyeFrustumsVisible);

	bool bMonitorVisible = EngineModule->IsMonitorVisible();
	if (ImGui::Checkbox("Show monitor", &bMonitorVisible))
		EngineModule->SetMonitorVisible(bMonitorVisible);

	// Useful values differ by orders of magnitude, so slider is logarithmic.
	float WorldUnitsPerMillimeter = EngineModule->GetWorldUnitsPerMillimeter();
	if (ImGui::SliderFloat("World units per millimeter", &WorldUnitsPerMillimeter, 0.0001f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp))
		EngineModule->SetWorldUnitsPerMillimeter(WorldUnitsPerMillimeter);
}

void Leia3DPluginEditor::DrawRenderingWindow()
{
	// Editor calls Begin before DrawContent, so current ImGui window is the window of this module.
	// FE_IMGUI_WINDOW_MANAGER is part of the editor executable, so current window is taken from ImGui context directly.
	if (ENGINE.GetViewport(ViewportID) == nullptr)
		ViewportID = ENGINE.CreateViewport(ImGui::GetCurrentContext()->CurrentWindow);

	FETexture* FinalResult = GetEngineModule()->GetFinalResult();
	// Whole pixels, because each "3D" pixel should be shown exactly on the pixel it was meant for.
	const ImVec2 Position = ImFloor(ImGui::GetCursorScreenPos());
	const ImVec2 Size = ImFloor(ImGui::GetContentRegionAvail());
	if (FinalResult != nullptr && Size.x > 0.0f && Size.y > 0.0f)
	{
		// Final result has the size of the main window, so each screen pixel shows the texel at the same position.
		const ImVec2 Scale = ImGui::GetIO().DisplayFramebufferScale;
		const float TextureWidth = static_cast<float>(FinalResult->GetWidth());
		const float TextureHeight = static_cast<float>(FinalResult->GetHeight());
		// Texture is upside down for ImGui, so V of the top edge is bigger than V of the bottom edge.
		const ImVec2 TopLeftUV = ImVec2(Position.x * Scale.x / TextureWidth, 1.0f - Position.y * Scale.y / TextureHeight);
		const ImVec2 BottomRightUV = ImVec2((Position.x + Size.x) * Scale.x / TextureWidth, 1.0f - (Position.y + Size.y) * Scale.y / TextureHeight);

		ImGui::SetCursorScreenPos(Position);
		ImGui::Image(FinalResult->GetTextureID(), Size, TopLeftUV, BottomRightUV);
	}
}