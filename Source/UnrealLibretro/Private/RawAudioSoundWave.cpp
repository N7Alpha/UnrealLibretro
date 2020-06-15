#include "RawAudioSoundWave.h"
#include "Engine.h"
#include "ActiveSound.h"

/* UMediaSoundWave structors
 *****************************************************************************/

URawAudioSoundWave::URawAudioSoundWave( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	bSetupDelegates = false;
	bLooping = false;
	bProcedural = true;
	Duration = INDEFINITELY_LOOPING_DURATION;
	DecompressionType = DTYPE_Procedural;
	VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
}

int32 URawAudioSoundWave::GeneratePCMData( uint8* PCMData, const int32 SamplesNeeded )
{
	auto SamplesIWillGive = FMath::Min(200, SamplesNeeded);

	auto FramesDequeued = 0;
	int32 AudioFrame;
	while (FramesDequeued < SamplesIWillGive && QueuedAudio->Peek(AudioFrame)) {
		((int32*)PCMData)[FramesDequeued] = AudioFrame;
		QueuedAudio->Dequeue();
		FramesDequeued++;
	}

	if (SamplesIWillGive != FramesDequeued) {
		UE_LOG(LogTemp, Warning, TEXT("Buffer overrun by %d bytes. Filling with 0 data"), 4 * (SamplesIWillGive - FramesDequeued));
		FMemory::Memzero(PCMData+4*FramesDequeued, 4 * (SamplesIWillGive - FramesDequeued));
	}

	return 4 * SamplesIWillGive; // THIS FUNCTION EXPECTS BYTES READ TO BE RETURNED NOT SAMPLES READ I HAVE BEEN BURNED BY THIS TOO MANY TIMES
}


int32 URawAudioSoundWave::GetResourceSizeForFormat( FName Format )
{
	return 0;
}


void URawAudioSoundWave::InitAudioResource( FByteBulkData& CompressedData )
{
	check(false); // should never be pushing compressed data to this class
}


bool URawAudioSoundWave::InitAudioResource( FName Format )
{
	return true;
}


/* UObject overrides
 *****************************************************************************/

void URawAudioSoundWave::GetAssetRegistryTags( TArray<FAssetRegistryTag>& OutTags ) const
{
}



void URawAudioSoundWave::Serialize( FArchive& Ar )
{
	// do not call the USoundWave version of serialize
	USoundBase::Serialize(Ar);
}

void URawAudioSoundWave::PostLoad()
{
	Super::PostLoad();

	if (!HasAnyFlags(RF_ClassDefaultObject) && !GIsBuildMachine)
	{
		  //InitializeTrack();
	}
}

void URawAudioSoundWave::BeginDestroy()
{
	Super::BeginDestroy();
}



