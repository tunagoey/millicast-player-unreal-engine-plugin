// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "MillicastAudioActor.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Components/AudioComponent.h"

#include "MillicastPlayerPrivate.h"

#include "Serialization/BufferArchive.h"

AMillicastAudioActor::AMillicastAudioActor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer), SoundStreaming(nullptr)
{
    UE_LOG(LogMillicastPlayer, Verbose, TEXT("%S"), __FUNCTION__);

    AudioComponent = ObjectInitializer.CreateDefaultSubobject<UAudioComponent>(
        this,
        TEXT("UAudioComponent")
        );
    AudioComponent->SetSound(SoundStreaming);
    AudioComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
}

AMillicastAudioActor::~AMillicastAudioActor() noexcept
{
    UE_LOG(LogMillicastPlayer, Verbose, TEXT("%S"), __FUNCTION__);
}

FMillicastAudioParameters AMillicastAudioActor::GetAudioParameters() const
{
    return AudioParameters;
}

void AMillicastAudioActor::UpdateAudioParameters(FMillicastAudioParameters Parameters) noexcept
{
    UE_LOG(LogMillicastPlayer, Verbose, TEXT("%S"), __FUNCTION__);

    AudioParameters = MoveTemp(Parameters);

    if (SoundStreaming)
    {
        UE_LOG(LogMillicastPlayer, Verbose, TEXT("New audio parameters are: %d %d %d"), 
            AudioParameters.SamplesPerSecond, AudioParameters.NumberOfChannels, AudioParameters.GetNumberBytesPerSample());
        SoundStreaming->SetSampleRate(AudioParameters.SamplesPerSecond);
        SoundStreaming->NumChannels = AudioParameters.NumberOfChannels;
        SoundStreaming->SampleByteSize = AudioParameters.GetNumberBytesPerSample();
    }
}

void AMillicastAudioActor::Initialize()
{
    UE_LOG(LogMillicastPlayer, Verbose, TEXT("%S"), __FUNCTION__);

    if (SoundStreaming == nullptr)
    {
        InitSoundWave();
    }
    else
    {
        UE_LOG(LogMillicastPlayer, Verbose, TEXT("Reset audio"));
        SoundStreaming->ResetAudio();
    }

    if (AudioComponent)
    {
        UE_LOG(LogMillicastPlayer, Verbose, TEXT("AudioComponent starts play"));
        AudioComponent->Play(0.0f);
    }    
}

void AMillicastAudioActor::Shutdown()
{
    UE_LOG(LogMillicastPlayer, Verbose, TEXT("%S"), __FUNCTION__);

    if (AudioComponent && AudioComponent->IsPlaying())
    {
        UE_LOG(LogMillicastPlayer, Verbose, TEXT("Stop Audio component"));
        AudioComponent->Stop();
    }
    SoundStreaming = nullptr;
}

void AMillicastAudioActor::QueueAudioData(const uint8* AudioData, int32 NumSamples)
{
    /* Don't queue if IsVirtualized is true because the buffer is not actually playing, this will desync with video*/
    if (AudioComponent->IsPlaying() && !AudioComponent->IsVirtualized())
    {
        SoundStreaming->QueueAudio(AudioData, NumSamples * AudioParameters.GetNumberBytesPerSample());
    }
    else
    {
        UE_LOG(LogMillicastPlayer, VeryVerbose, TEXT("Not able to queue audio data"));
    }
}

void AMillicastAudioActor::InitSoundWave()
{
    UE_LOG(LogMillicastPlayer, Verbose, TEXT("%S"), __FUNCTION__);

    SoundStreaming = NewObject<USoundWaveProcedural>(this);
    SoundStreaming->SetSampleRate(AudioParameters.SamplesPerSecond);
    SoundStreaming->NumChannels = AudioParameters.NumberOfChannels;
    SoundStreaming->SampleByteSize = AudioParameters.GetNumberBytesPerSample();
    SoundStreaming->Duration = INDEFINITELY_LOOPING_DURATION;
    SoundStreaming->SoundGroup = SOUNDGROUP_Voice;
    SoundStreaming->bLooping = true;
    SoundStreaming->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;

    UE_LOG(LogMillicastPlayer, Verbose, TEXT("SoundStreaming: %d %d %d %d %f %d %d"),
        AudioParameters.SamplesPerSecond, AudioParameters.NumberOfChannels, SoundStreaming->SampleByteSize,
        SoundStreaming->Duration, SoundStreaming->SoundGroup, SoundStreaming->bLooping, SoundStreaming->VirtualizationMode);

    if (AudioComponent == nullptr)
    {
        UE_LOG(LogMillicastPlayer, Verbose, TEXT("Create AudioComponent"));
        auto AudioDevice = GEngine->GetMainAudioDevice();
        if (AudioDevice)
        {
            AudioComponent = AudioDevice->CreateComponent(SoundStreaming);
            AudioComponent->bIsUISound = false;
            AudioComponent->bAllowSpatialization = false;
            AudioComponent->SetVolumeMultiplier(1.0f);
            // AudioComponent->AddToRoot();
        }
        else
        {
            UE_LOG(LogMillicastPlayer, Warning, TEXT("Could not get AudioDevice"));
        }
    }
    else
    {
        AudioComponent->Sound = SoundStreaming;
    }

    const FSoftObjectPath VoiPSoundClassName = GetDefault<UAudioSettings>()->VoiPSoundClass;
    if (AudioComponent && VoiPSoundClassName.IsValid())
    {
        AudioComponent->SoundClassOverride = LoadObject<USoundClass>(nullptr, *VoiPSoundClassName.ToString());
    }
}
