#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "SimpleComputeShaderParallel.generated.h"






// 
#define NUM_THREADS_SimpleComputeShaderParallel_X 2
#define NUM_THREADS_SimpleComputeShaderParallel_Y 1
#define NUM_THREADS_SimpleComputeShaderParallel_Z 1




struct SHADERMODULE_API FSimpleComputeShaderParallelDispatchParams
{
	int X;
	int Y;
	int Z;

	
	int Input[4];
	int Output[2];
	
	

	FSimpleComputeShaderParallelDispatchParams(int x, int y, int z)
		: X(x)
		, Y(y)
		, Z(z)
	{
	}
};

// This is a public interface that we define so outside code can invoke our compute shader.
class SHADERMODULE_API FSimpleComputeShaderParallelInterface {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FSimpleComputeShaderParallelDispatchParams Params,
		TFunction<void(TArray<int32> OutputVals)> AsyncCallback
	);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FSimpleComputeShaderParallelDispatchParams Params,
		TFunction<void(TArray<int32> OutputVals)> AsyncCallback
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
		FSimpleComputeShaderParallelDispatchParams Params,
		TFunction<void(TArray<int32> OutputVals)> AsyncCallback
	)
	{
		if (IsInRenderingThread()) {
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
		}else{
			DispatchGameThread(Params, AsyncCallback);
		}
	}
};



DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSimpleComputeShaderParallelLibrary_AsyncExecutionCompleted, const int, Value1, const int, Value2);


UCLASS() // Change the _API to match your project
class SHADERMODULE_API USimpleComputeShaderParallelLibrary_AsyncExecution : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	
	// Execute the actual load
	virtual void Activate() override {
		// Create a dispatch parameters struct and fill it the input array with our args
		FSimpleComputeShaderParallelDispatchParams Params(1, 1, 1);
		Params.Input[0] = Arg1;
		Params.Input[1] = Arg2;
		Params.Input[2] = Arg3;
		Params.Input[3] = Arg4;

		// Dispatch the compute shader and wait until it completes
		FSimpleComputeShaderParallelInterface::Dispatch(Params, [this](TArray<int32> OutputVals) {
			this->Completed.Broadcast(OutputVals[0], OutputVals[1]);
		});
	}
	
	
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "ComputeShader", WorldContext = "WorldContextObject"))
	static USimpleComputeShaderParallelLibrary_AsyncExecution* ExecuteBaseComputeShaderParallel(UObject* WorldContextObject, int Arg1, int Arg2, int Arg3, int Arg4 ) {
		USimpleComputeShaderParallelLibrary_AsyncExecution* Action = NewObject<USimpleComputeShaderParallelLibrary_AsyncExecution>();
		Action->Arg1 = Arg1;
		Action->Arg2 = Arg2;
		Action->Arg3 = Arg3;
		Action->Arg4 = Arg4;
		Action->RegisterWithGameInstance(WorldContextObject);

		return Action;
	}

	UPROPERTY(BlueprintAssignable)
	FOnSimpleComputeShaderParallelLibrary_AsyncExecutionCompleted Completed;

	
	int Arg1;
	int Arg2;
	int Arg3;
	int Arg4;
	
};