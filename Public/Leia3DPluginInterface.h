#pragma once

namespace FocalEngine
{
	class FEScene;
	class FEViewport;
	class FETexture;
	class FEEntity;
}

// Public API of Leia3DPlugin, applications include only this header.
// Usage:
//	using PluginInterface = Leia3DPluginInterface;
//	PluginInterface* Plugin = PLUGIN_MANAGER.GetPlugin<PluginInterface>(UNIQUE_ID.FromString(PluginInterface::PluginID));
//	if (Plugin != nullptr && Plugin->GetInterfaceVersion() == PluginInterface::InterfaceVersion)
//		Plugin->...;
class Leia3DPluginInterface
{
public:
	// Same as "ID" in Leia3DPlugin.json.
	static constexpr const char* PluginID = "510257d6-2cd9-449a-9be7-cd2d6f5b4179";

	// Increase when anything in this file changes.
	static constexpr int InterfaceVersion = 3;

	// Order of virtual functions is part of the interface, new functions should be added only at the end.
	virtual ~Leia3DPluginInterface() = default;
	virtual int GetInterfaceVersion() const { return InterfaceVersion; }

	// Switches lens of 3D monitor, returns false if plugin could not connect to the LeiaSR service.
	virtual bool Set3DModeEnabled(bool bNewValue) = 0;
	virtual bool Is3DModeEnabled() const = 0;

	// Creates monitor and eye camera entities in Scene, 3D image will be produced for Viewport area.
	// Previous binding is removed first, together with its entities.
	// If Viewport is ENGINE.GetDefaultViewport(), main window is the 3D area and plugin draws final result into it, before UI is rendered.
	virtual bool BindToScene(FocalEngine::FEScene* Scene, FocalEngine::FEViewport* Viewport) = 0;
	// Deletes monitor and eye camera entities from bound scene.
	virtual void Unbind() = 0;
	// Returns nullptr if plugin is not bound to any scene.
	virtual FocalEngine::FEScene* GetBoundScene() const = 0;

	virtual FocalEngine::FETexture* GetFinalResult() const = 0;

	// Scale between physical millimeters of the monitor and scene world units, value should be positive.
	virtual float GetWorldUnitsPerMillimeter() const = 0;
	virtual void SetWorldUnitsPerMillimeter(float NewValue) = 0;

	// Eye frustums are drawn with debug lines, which are rendered for every camera, so they are also visible in 3D view itself.
	virtual bool AreEyeFrustumsVisible() const = 0;
	virtual void SetEyeFrustumsVisible(bool bNewValue) = 0;

	// Outlines of the physical display and of the 3D area on it, drawn with debug lines like eye frustums.
	virtual bool IsMonitorVisible() const = 0;
	virtual void SetMonitorVisible(bool bNewValue) = 0;

	// Center of the 3D area, its local XY plane is the screen and the viewer is on its +Z side.
	// Eye cameras are its children, so moving or rotating it moves the whole 3D view.
	// It is recreated by BindToScene, so it should not be stored. Returns nullptr if plugin is not bound.
	virtual FocalEngine::FEEntity* GetMonitorEntity() const = 0;

	// False without LeiaSR Platform, LeiaSR service or connected 3D monitor.
	virtual bool Is3DMonitorAvailable() const = 0;

	virtual FocalEngine::FEEntity* GetLeftEyeEntity() const = 0;
	virtual FocalEngine::FEEntity* GetRightEyeEntity() const = 0;
};