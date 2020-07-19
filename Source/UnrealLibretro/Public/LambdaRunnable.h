#pragma once
#include <Runtime\Core\Public\HAL\ThreadSafeBool.h>
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

	//static TArray<FLambdaRunnable*> Runnables;
	static FThreadSafeCounter ThreadNumber;

public:
	//~~~ Thread Core Functions ~~~
	/** Use this thread-safe boolean to allow early exits for your threads */
	FThreadSafeBool Finished;

	//Constructor / Destructor
	FLambdaRunnable(TUniqueFunction< void()> &&InFunction);
	virtual ~FLambdaRunnable();

	// Begin FRunnable interface.
	virtual bool Init();
	virtual uint32 Run();
	virtual void Stop();
	virtual void Exit() override;
	FRunnableThread* Thread;
	// End FRunnable interface

	/** Makes sure this thread has stopped properly */
	void EnsureCompletion();

	/*
	Runs the passed lambda on the background thread, new thread per call
	*/
	static FLambdaRunnable* RunLambdaOnBackGroundThread(TUniqueFunction< void()> &&InFunction);
};