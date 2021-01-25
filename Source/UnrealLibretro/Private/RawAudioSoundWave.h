#pragma once
#include "CoreMinimal.h"
#include "Sound/SoundWave.h"
#include "Containers/CircularQueue.h"

#include "RawAudioSoundWave.generated.h"


/**
 * Implements a playable sound asset for streams of raw pcm data.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALLIBRETRO_API URawAudioSoundWave : public USoundWave
{
	GENERATED_UCLASS_BODY()

public:

	// USoundWave overrides

	virtual int32 GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded) override;
	virtual int32 GetResourceSizeForFormat( FName Format ) override;
	virtual void InitAudioResource( FByteBulkData& CompressedData ) override;
	virtual bool InitAudioResource( FName Format ) override;

public:

	// UObject overrides
	
	virtual void GetAssetRegistryTags( TArray<FAssetRegistryTag>& OutTags ) const override;
	virtual void Serialize( FArchive& Ar ) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	
	

public:
	// Notifications from downstream
//virtual void Play() override;
//	virtual void Pause() override;
//	virtual void Stop() override;

	/** Holds queued audio samples. */
	typedef uint32 libretro_frame;
	TSharedPtr<TCircularQueue<int32>, ESPMode::ThreadSafe> AudioQueue; // TCircularQueue is thread safe and lock free in single producer single consumer scenarios

	bool bSetupDelegates;
};

