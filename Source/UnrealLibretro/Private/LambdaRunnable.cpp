#pragma once

#include "LambdaRunnable.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h" 
#include <Runtime\Core\Public\Containers\UnrealString.h>

FThreadSafeCounter FLambdaRunnable::ThreadNumber{0};

FLambdaRunnable::FLambdaRunnable(TFunction< void()> InFunction)
{
	FunctionPointer = InFunction;
	Finished = false;
	Number = ThreadNumber.Increment();
	
	FString threadStatGroup = FString::Printf(TEXT("FLambdaRunnable%d"), Number);
	Thread = FRunnableThread::Create(this, *threadStatGroup, 0, EThreadPriority::TPri_Lowest); // @todo: Thread priority is low because if you set it any higher windows can basically deadlock when stopping your game in the editor. Its kind of outside of my level of understanding to debug this at the moment.
																							   // From tests I've done so far it seems to be an issue isolated to FRunnableThread and even exists in the lower level FThread as well.
																							   // if enough work is being performed in outstanding FRunnableThread's to saturate all cores of the processor when you try to stop the game in the editor,
																							   // then it will lock up Windows entirely
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

FLambdaRunnable* FLambdaRunnable::RunLambdaOnBackGroundThread(TFunction< void()> InFunction)
{
	FLambdaRunnable* Runnable;
	Runnable = new FLambdaRunnable(InFunction);
	//UE_LOG(LogClass, Log, TEXT("FLambdaRunnable RunLambdaBackGroundThread"));
	return Runnable;
}