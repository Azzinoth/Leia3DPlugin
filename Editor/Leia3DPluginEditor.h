#pragma once
#include "../SubSystems/FocalEngine/SubSystems/PluginSystem/FEPluginConnector.h"
#include "../SubSystems/EditorAPI/FEEditorAPI.h"
#include "../Public/Leia3DPluginInterface.h"
using namespace FocalEngine;

class Leia3DPluginEditor : public FEEditorPluginCore
{
	using EngineModuleInterface = Leia3DPluginInterface;

	// Editor module is loaded after engine module and unloaded before it, so engine module is always available.
	EngineModuleInterface* GetEngineModule() { return PLUGIN_MANAGER.GetPlugin<EngineModuleInterface>(UNIQUE_ID.FromString(EngineModuleInterface::PluginID)); }

	FEEditorAPI* Editor = nullptr;

	// Viewport of "Leia 3D monitor rendering" window, 3D image is produced for its area and shown in it.
	FEUUID ViewportID;

	void DrawSettingsWindow();
	void DrawRenderingWindow();
public:
	bool OnEditorLoad(FEEditorAPI* EditorAPI) override;
	void OnEditorUnload() override;
	void OnEditorUpdate(double DeltaTime) override;
};