#pragma once

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
	static constexpr int InterfaceVersion = 1;

	// Order of virtual functions is part of the interface, new functions should be added only at the end.
	virtual ~Leia3DPluginInterface() = default;
	virtual int GetInterfaceVersion() const { return InterfaceVersion; }

	// Switches lens of 3D monitor, returns false if plugin could not connect to the LeiaSR service.
	virtual bool Set3DModeEnabled(bool bNewValue) = 0;
	virtual bool Is3DModeEnabled() const = 0;
};