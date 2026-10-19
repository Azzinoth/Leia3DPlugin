#include "SubSystems/FocalEngine/SubSystems/PluginSystem/FEPluginConnector.h"
#include "Public/Leia3DPluginInterface.h"
using namespace FocalEngine;

class Leia3DPlugin : public FEPluginCore, public Leia3DPluginInterface
{
public:
	void OnLoad() override;
	void OnUnload() override;
	void OnUpdate(double DeltaTime) override;
};