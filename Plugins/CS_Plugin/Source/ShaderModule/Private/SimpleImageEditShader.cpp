#include "SimpleImageEditShader.h"
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
#include "Engine/TextureRenderTarget2D.h"
#include "RendererInterface.h"
#include "RenderTargetPool.h"


void AMyShaderExecutor::ExecuteShaderAndUpdate(UTextureRenderTarget2D* InputRenderTarget2D)
{
    // we first check if the input render target is valid, if not, the following code will not be executed
    if (!InputRenderTarget2D || !InputRenderTarget2D->GetResource())
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid Input RenderTarget."));
        return;
    }
    // get the render target resource of the input render target
    FTextureRenderTargetResource* InputRenderTargetResource = InputRenderTarget2D->GameThread_GetRenderTargetResource();
    // for the remaining code, we need to execute it on the render thread
    // so we use ENQUEUE_RENDER_COMMAND to execute the code on the render thread
    ENQUEUE_RENDER_COMMAND(ExecuteShaderAndUpdateCmd)(
        [InputRenderTargetResource, this](FRHICommandListImmediate& RHICmdList)
        {
            // create a RDG builder
            FRDGBuilder GraphBuilder(RHICmdList);

            // get the Render Hardware Interface reference of the input render target(the low level texture reference)
            FTextureRHIRef InputTextureRHI = InputRenderTargetResource->GetRenderTargetTexture();
            // declare the texture reference of the input render target in RDG
            FRDGTextureRef InputTexture;
            // here we do the following things:
            // 1. check if the input render target is already defined in PoolRenderTarget and create a new one if not
            // 2. register the input render target in RDG using its PoolRenderTarget reference
            
            if (!InputRenderTarget) {
                InputRenderTarget = CreateRenderTarget(InputTextureRHI, TEXT("InputTextureAsPooledRenderTarget"));
                InputTexture = GraphBuilder.RegisterExternalTexture(InputRenderTarget, TEXT("InputTexture"));
            }
            else {
                // the RDG reference of the input render target needs to be registered in the RDG builder in every execution
                InputTexture = GraphBuilder.RegisterExternalTexture(InputRenderTarget, TEXT("InputTexture"));
                // but the InputRenderTarget itself only needs to be created once
            }
            
            // create a new output render target in RDG
            FRDGTextureRef OutputTexture;

            // similarly, we need to check if the output render target is already defined in PoolRenderTarget and create a new one if not
            if (!OutputRenderTarget)
            {
                // create a new output texture with the same size and format as the input texture
                // first is its description
                FIntPoint Size = InputRenderTargetResource->GetSizeXY();
                FPooledRenderTargetDesc OutputDesc(FPooledRenderTargetDesc::Create2DDesc(
                    Size,                                     // texture size
                    InputTextureRHI->GetFormat(),             // pixel format
                    FClearValueBinding::None,                 // clear value
                    TexCreate_None,					          // InFlags
                    TexCreate_ShaderResource | TexCreate_UAV, // InTargetableFlags
                    false                                     // bInForceSeparateTargetAndShaderResource
                ));

                // create the output render target in the render target pool
                GRenderTargetPool.FindFreeElement(RHICmdList, OutputDesc, OutputRenderTarget, TEXT("OutputRenderTarget"));


                // then register the output render target in RDG
                OutputTexture = GraphBuilder.RegisterExternalTexture(OutputRenderTarget, TEXT("OutputTexture"));
            }
            else
            {
                // still, the RDG reference of the output render target needs to be registered in the RDG builder in every execution
                OutputTexture = GraphBuilder.RegisterExternalTexture(OutputRenderTarget, TEXT("OutputTexture"));
                // but the OutputRenderTarget itself only needs to be created once
            }

            // set the parameters of the shader
            FSimpleImageEditShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FSimpleImageEditShader::FParameters>();
            FRDGTextureSRVRef InputTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(InputTexture));
            PassParameters->InputTexture = InputTextureSRV;
            FRDGTextureUAVRef OutputTextureUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutputTexture));
            PassParameters->OutputTexture = OutputTextureUAV;

            // add the shader to the render graph
            TShaderMapRef<FSimpleImageEditShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
            FIntVector DispatchSize(
                // it is hard-coded to 8, 8, 1 in our hlsl shader
                FMath::DivideAndRoundUp(OutputTexture->Desc.Extent.X, 8),
                FMath::DivideAndRoundUp(OutputTexture->Desc.Extent.Y, 8),
                1
            );
            // add the shader to the render graph
            // this time, we don't use async compute, no async compute flag
            GraphBuilder.AddPass(
                RDG_EVENT_NAME("ExecuteImageEditShader"),
                PassParameters,
                ERDGPassFlags::Compute,
                [PassParameters, ComputeShader, DispatchSize](FRHICommandList& RHICmdList)
                {
                    FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, DispatchSize);
                }
            );

            // add a copy texture pass to copy the output texture to the input texture
            // this is easy in RDG, we just need to call AddCopyTexturePass
            AddCopyTexturePass(GraphBuilder, OutputTexture, InputTexture);

            // finally, execute the render graph
            GraphBuilder.Execute();
        }
    );
}
