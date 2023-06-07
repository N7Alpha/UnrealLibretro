#include "LambdaRunnable.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h" 
#include "Containers/UnrealString.h"

FThreadSafeCounter FLambdaRunnable::ThreadNumber{0};

FLambdaRunnable::FLambdaRunnable(FString ThreadName, TUniqueFunction< void()> InFunction)
{
    FunctionPointer = MoveTemp(InFunction);
    Number = ThreadNumber.Increment();
    
    FString threadStatGroup = FString::Printf(TEXT("%s%d"), *ThreadName, Number);
    Thread = FRunnableThread::Create(this, *threadStatGroup, 0,
        EThreadPriority::TPri_SlightlyBelowNormal); // This is actually normal thread priority on Windows and probably other platforms as well. You might be tempted to set it higher, but it will deadlock Windows if you have enough work available to saturate all cores. And yes I do mean completely halt the OS.
}

FLambdaRunnable::~FLambdaRunnable()
{
    Thread->Suspend(false);
    Thread->WaitForCompletion();
    delete Thread;
}

//Run
uint32 FLambdaRunnable::Run()
{
    if (FunctionPointer)
        FunctionPointer();

    return 0;
}

FLambdaRunnable* FLambdaRunnable::RunLambdaOnBackGroundThread(FString ThreadName, TUniqueFunction< void()> InFunction)
{
    FLambdaRunnable* Runnable;
    Runnable = new FLambdaRunnable(ThreadName, MoveTemp(InFunction));
    return Runnable;
}
