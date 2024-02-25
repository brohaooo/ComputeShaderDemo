#include "SimpleComputeShader.h"
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

DECLARE_STATS_GROUP(TEXT("SimpleComputeShader"), STATGROUP_SimpleComputeShader, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("SimpleComputeShader Execute"), STAT_SimpleComputeShader_Execute, STATGROUP_SimpleComputeShader);

// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
class SHADERMODULE_API FSimpleComputeShader : public FGlobalShader
{
public:
	
	DECLARE_GLOBAL_SHADER(FSimpleComputeShader);
	SHADER_USE_PARAMETER_STRUCT(FSimpleComputeShader, FGlobalShader);
	

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		/*
		* Here's where you define one or more of the input parameters for your shader.
		* Some examples:
		*/
		// SHADER_PARAMETER(uint32, MyUint32) // On the shader side: uint32 MyUint32;
		// SHADER_PARAMETER(FVector3f, MyVector) // On the shader side: float3 MyVector;

		// SHADER_PARAMETER_TEXTURE(Texture2D, MyTexture) // On the shader side: Texture2D<float4> MyTexture; (float4 should be whatever you expect each pixel in the texture to be, in this case float4(R,G,B,A) for 4 channels)
		// SHADER_PARAMETER_SAMPLER(SamplerState, MyTextureSampler) // On the shader side: SamplerState MySampler; // CPP side: TStaticSamplerState<ESamplerFilter::SF_Bilinear>::GetRHI();

		// SHADER_PARAMETER_ARRAY(float, MyFloatArray, [3]) // On the shader side: float MyFloatArray[3];

		// SHADER_PARAMETER_UAV(RWTexture2D<FVector4f>, MyTextureUAV) // On the shader side: RWTexture2D<float4> MyTextureUAV;
		// SHADER_PARAMETER_UAV(RWStructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWStructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_UAV(RWBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWBuffer<FMyCustomStruct> MyCustomStructs;

		// SHADER_PARAMETER_SRV(StructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: StructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Buffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: Buffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Texture2D<FVector4f>, MyReadOnlyTexture) // On the shader side: Texture2D<float4> MyReadOnlyTexture;

		// SHADER_PARAMETER_STRUCT_REF(FMyCustomStruct, MyCustomStruct)

		
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, Input)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, Output)
		

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		//const FPermutationDomain PermutationVector(Parameters.PermutationId);
		
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);


		/*
		* Here you define constants that can be used statically in the shader code.
		* Example:
		*/
		// OutEnvironment.SetDefine(TEXT("MY_CUSTOM_CONST"), TEXT("1"));

		/*
		* These defines are used in the thread count section of our shader
		*/
		OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_SimpleComputeShader_X);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_SimpleComputeShader_Y);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_SimpleComputeShader_Z);

		// This shader must support typed UAV load and we are testing if it is supported at runtime using RHIIsTypedUAVLoadSupported
		//OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		// FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}
private:
};

// This will tell the engine to create the shader and where the shader entry point is.
//                            ShaderType                            ShaderPath                     Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FSimpleComputeShader, "/CS_Shaders/CS_sum.usf", "MySimpleComputeShader", SF_Compute);

void FSimpleComputeShaderInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FSimpleComputeShaderDispatchParams Params, TFunction<void(int OutputVal)> AsyncCallback) {
	// create a new render graph builder, which will be used to create the resources (e.g. buffers, textures, passes, etc.)
	// and execute the shader
	FRDGBuilder GraphBuilder(RHICmdList);

	{
		// monitor the execution time of this function
		SCOPE_CYCLE_COUNTER(STAT_SimpleComputeShader_Execute);
		DECLARE_GPU_STAT(SimpleComputeShader)
		RDG_EVENT_SCOPE(GraphBuilder, "SimpleComputeShader");
		RDG_GPU_STAT_SCOPE(GraphBuilder, SimpleComputeShader);

		// get a reference of our shader from the global shader map
		TShaderMapRef<FSimpleComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		

		bool bIsShaderValid = ComputeShader.IsValid();
		// ensure the shader is valid before we continue
		if (bIsShaderValid) {
			// allocate the parameters for the shader
			FSimpleComputeShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FSimpleComputeShader::FParameters>();

			// input and output are both buffers, so we need to create them and pass them to the shader
			const void* RawData = (void*)Params.Input;
			int NumInputs = 4;
			int InputSize = sizeof(int);
			FRDGBufferRef InputBuffer = CreateUploadBuffer(GraphBuilder, TEXT("InputBuffer"), InputSize, NumInputs, RawData, InputSize * NumInputs);
			// the input buffer is a SRV (Shader Resource View) in the shader, so we need to create it as such
			PassParameters->Input = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(InputBuffer, PF_R32_SINT));

			FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(sizeof(int32), 1),
				TEXT("OutputBuffer"));
			// similarly, the output buffer is a UAV (Unordered Access View) in the shader, so we need to create
			PassParameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_SINT));
			
			// get the group count for the shader (this is the number of thread groups that will be dispatched, currently set to 1)
			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(Params.X, Params.Y, Params.Z), FComputeShaderUtils::kGolden2DGroupSize);
			// add a pass to the render graph to execute the shader, this is an async compute pass
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteSimpleComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
			{
				FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
			});

			// add a pass to the render graph to copy the output buffer to the CPU:
			// first we create a GPU buffer readback object
			FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteSimpleComputeShaderOutput"));
			// then we add a pass to the render graph to copy the output buffer to the readback object
			AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, OutputBuffer, 0u);

			// add a task to the render graph to execute the callback when the readback is ready
			// this is a recursive lambda that will keep checking if the readback is ready, and if it is, it will execute the callback
			auto RunnerFunc = [GPUBufferReadback, AsyncCallback](auto&& RunnerFunc) -> void {
				if (GPUBufferReadback->IsReady()) {
					
					int32* Buffer = (int32*)GPUBufferReadback->Lock(1);
					int OutVal = Buffer[0];
					
					GPUBufferReadback->Unlock();

					AsyncTask(ENamedThreads::GameThread, [AsyncCallback, OutVal]() {
						AsyncCallback(OutVal);
					});

					delete GPUBufferReadback;
				} else {
					AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
						RunnerFunc(RunnerFunc);
					});
				}
			};

			AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
				RunnerFunc(RunnerFunc);
			});
			
		} else {
			// We silently exit here as we don't want to crash the game if the shader is not found or has an error.
			
		}
	}

	// finally, execute the render graph, so that all the passes we added will be executed
	GraphBuilder.Execute();
}