#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "SimpleComputeShader.generated.h"






// 
#define NUM_THREADS_SimpleComputeShader_X 1
#define NUM_THREADS_SimpleComputeShader_Y 1
#define NUM_THREADS_SimpleComputeShader_Z 1




struct SHADERMODULE_API FSimpleComputeShaderDispatchParams
{
	int X;
	int Y;
	int Z;

	
	int Input[4];
	int Output;
	
	

	FSimpleComputeShaderDispatchParams(int x, int y, int z)
		: X(x)
		, Y(y)
		, Z(z)
	{
	}
};

// This is a public interface that we define so outside code can invoke our compute shader.
class SHADERMODULE_API FSimpleComputeShaderInterface {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FSimpleComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FSimpleComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	)
	{
		//UE_LOG(LogTemp, Warning, TEXT("DispatchGameThread"));
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
	}

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FSimpleComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	)
	{
		if (IsInRenderingThread()) {
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
		}else{
			DispatchGameThread(Params, AsyncCallback);
		}
	}
};



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSimpleComputeShaderLibrary_AsyncExecutionCompleted, const int, Value);


UCLASS() // Change the _API to match your project
class SHADERMODULE_API USimpleComputeShaderLibrary_AsyncExecution : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	
	// Execute the actual load
	virtual void Activate() override {
		// Create a dispatch parameters struct and fill it the input array with our args
		FSimpleComputeShaderDispatchParams Params(1, 1, 1);
		Params.Input[0] = Arg1;
		Params.Input[1] = Arg2;
		Params.Input[2] = Arg3;
		Params.Input[3] = Arg4;

		// Dispatch the compute shader and wait until it completes
		FSimpleComputeShaderInterface::Dispatch(Params, [this](int OutputVal) {
			this->Completed.Broadcast(OutputVal);
		});
	}
	
	
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "ComputeShader", WorldContext = "WorldContextObject"))
	static USimpleComputeShaderLibrary_AsyncExecution* ExecuteBaseComputeShader(UObject* WorldContextObject, int Arg1, int Arg2, int Arg3, int Arg4 ) {
		USimpleComputeShaderLibrary_AsyncExecution* Action = NewObject<USimpleComputeShaderLibrary_AsyncExecution>();
		Action->Arg1 = Arg1;
		Action->Arg2 = Arg2;
		Action->Arg3 = Arg3;
		Action->Arg4 = Arg4;
		Action->RegisterWithGameInstance(WorldContextObject);

		return Action;
	}

	UPROPERTY(BlueprintAssignable)
	FOnSimpleComputeShaderLibrary_AsyncExecutionCompleted Completed;

	
	int Arg1;
	int Arg2;
	int Arg3;
	int Arg4;
	
};