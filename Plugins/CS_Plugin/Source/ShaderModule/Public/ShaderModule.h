#pragma once

#include "CoreMinimal.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "RenderGraphResources.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"

class SHADERMODULE_API FShaderModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};