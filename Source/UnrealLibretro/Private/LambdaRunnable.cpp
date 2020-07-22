#pragma once

#include "LambdaRunnable.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h" 
#include <Runtime\Core\Public\Containers\UnrealString.h>

FThreadSafeCounter FLambdaRunnable::ThreadNumber{0};

FLambdaRunnable::FLambdaRunnable(TUniqueFunction< void()> &&InFunction)
{
	FunctionPointer = MoveTemp(InFunction);
	Finished = false;
	Number = ThreadNumber.Increment();
	
	FString threadStatGroup = FString::Printf(TEXT("FLambdaRunnable%d"), Number);
	Thread = FRunnableThread::Create(this, *threadStatGroup, 0,
		EThreadPriority::TPri_SlightlyBelowNormal); // This is actually normal thread priority on Windows and probably other platforms as well. You might be tempted to set it higher, but it will deadlock Windows if you have enough work available to saturate all cores. And yes I do mean completely halt the OS.
}

FLambdaRunnable::~FLambdaRunnable()
{
	EnsureCompletion();
	delete Thread;
}

//Init
bool FLambdaRunnable::Init()
{
	//UE_LOG(LogClass, Log, TEXT("FLambdaRunnable %d Init"), Number);
	return true;
}

//Run
uint32 FLambdaRunnable::Run()
{
	if (FunctionPointer)
		FunctionPointer();

	//UE_LOG(LogClass, Log, TEXT("FLambdaRunnable %d Run complete"), Number);
	return 0;
}

//stop
void FLambdaRunnable::Stop()
{
	Finished = true;
}

void FLambdaRunnable::Exit()
{

}

void FLambdaRunnable::EnsureCompletion() // @todo: the timing error might be here also this will leak memory
{
	Stop();
	Thread->Suspend(false);
	Thread->WaitForCompletion();

}

FLambdaRunnable* FLambdaRunnable::RunLambdaOnBackGroundThread(TUniqueFunction< void()> &&InFunction)
{
	FLambdaRunnable* Runnable;
	Runnable = new FLambdaRunnable(MoveTemp(InFunction));
	//UE_LOG(LogClass, Log, TEXT("FLambdaRunnable RunLambdaBackGroundThread"));
	return Runnable;
}