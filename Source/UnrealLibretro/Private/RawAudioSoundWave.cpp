#include "RawAudioSoundWave.h"
#include "CoreMinimal.h"
#include "ActiveSound.h"
#include "UnrealLibretro.h"

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
    auto SamplesIWillGive = FMath::Min(64, SamplesNeeded);

    auto FramesDequeued = 0;
    int32 AudioFrame;
    while (FramesDequeued < SamplesIWillGive && AudioQueue->Peek(AudioFrame)) {
        ((int32*)PCMData)[FramesDequeued] = AudioFrame;
        AudioQueue->Dequeue();
        FramesDequeued++;
    }

    if (SamplesIWillGive != FramesDequeued) {
        UE_LOG(Libretro, Verbose, TEXT("Buffer overrun by %d bytes. Filling with 0 data"), 4 * (SamplesIWillGive - FramesDequeued));
        FMemory::Memzero(PCMData+4*FramesDequeued, 4 * (SamplesIWillGive - FramesDequeued));
    }
    
    return 4 * SamplesIWillGive; // Note: THIS FUNCTION EXPECTS BYTES READ TO BE RETURNED NOT SAMPLES READ I HAVE BEEN BURNED BY THIS TOO MANY TIMES
                                 //       Also what we return here implicitly affects some things:
                                 //           - returning 0 implies stop playing
                                 //           - The smaller the number the higher the frequency we are polled at
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



