#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "PixelShaderUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "UnifiedBuffer.h"
#include "CanvasTypes.h"
#include "MeshDrawShaderBindings.h"
#include "RHIGPUReadback.h"
#include "MeshPassUtils.h"
#include "MaterialShader.h"

#include "SimpleImageEditShader.generated.h"


// this is a compute shader example that takes a 2D texture as input and modifies it
// the shader will be executed in the AMyShaderExecutor class


// the compute shader class, will be used to execute the shader in MyShaderExecutor
class FSimpleImageEditShader : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FSimpleImageEditShader);
    SHADER_USE_PARAMETER_STRUCT(FSimpleImageEditShader, FGlobalShader);
    // we need RDG type for shader parameters in order to use the render graph
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InputTexture)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
    END_SHADER_PARAMETER_STRUCT()

public:
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
    }
};

IMPLEMENT_GLOBAL_SHADER(FSimpleImageEditShader, "/CS_Shaders/CS_ImageEdit.usf", "MainCS", SF_Compute);


// a class containing the shader execution logic and the blueprint callable function
// as well as two render targets, one for input and one for output
UCLASS(Blueprintable)
class SHADERMODULE_API AMyShaderExecutor : public APawn
{
    GENERATED_BODY()

public:
    // Call this function to execute the shader (can be called from blueprint for multiple times)
    // takes a render target as input, executes the shader and updates the output render target
    // then copies the output render target to the input render target
    // so that the input render target will be modified
    UFUNCTION(BlueprintCallable, Category = "ShaderExecution")
    void ExecuteShaderAndUpdate(UTextureRenderTarget2D* InputRenderTarget2D);

    // just a visual representation of the render target, can be set in the editor
    // then you can create a material and set the render target as the texture
    // and then set the material to the static mesh to see the computed image result
    UPROPERTY(EditAnywhere)
    UStaticMeshComponent* static_mesh_for_visualization;

    // the render target that will be used as input for the shader, can be set in the editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ShaderDemo)
    class UTextureRenderTarget2D* RenderTarget;

    AMyShaderExecutor() {
        InputRenderTarget = nullptr;
        OutputRenderTarget = nullptr;
    };

private:
    // the render target that will be used as input and output parameter for the shader
    // we store their IPooledRenderTarget references so that we can re-use them in the execute function
    TRefCountPtr<IPooledRenderTarget> InputRenderTarget;
    TRefCountPtr<IPooledRenderTarget> OutputRenderTarget;
};






