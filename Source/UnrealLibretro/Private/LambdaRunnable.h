#pragma once
#include "HAL/ThreadSafeBool.h"
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

/*
Long duration lambda wrapper, which are generally not supported by the taskgraph system. New thread per lambda and they will auto-delete upon
completion.
*/
class UNREALLIBRETRO_API FLambdaRunnable : public FRunnable
{
private:
	/** Thread to run the worker FRunnable on */
	
	uint64 Number;

	//Lambda function pointer
	TUniqueFunction< void()> FunctionPointer;

	static FThreadSafeCounter ThreadNumber;

public:
	//Constructor / Destructor
	FLambdaRunnable(FString ThreadName, TUniqueFunction< void()> InFunction);
	virtual ~FLambdaRunnable();

	// Begin FRunnable interface.
	virtual uint32 Run();
	FRunnableThread* Thread;
	// End FRunnable interface

	/*
	Runs the passed lambda on the background thread, new thread per call
	*/
	static FLambdaRunnable* RunLambdaOnBackGroundThread(FString ThreadName, TUniqueFunction< void()> InFunction);
};
