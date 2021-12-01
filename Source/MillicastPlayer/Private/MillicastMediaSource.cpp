// Copyright CoSMoSoftware 2021. All Rights Reserved.

#include "MillicastMediaSource.h"
#include "MillicastPlayerPrivate.h"

#include <api/video/i420_buffer.h>
#include <common_video/libyuv/include/webrtc_libyuv.h>

#include <RenderTargetPool.h>
#include <XRThreadUtils.h>

UMillicastMediaSource::UMillicastMediaSource()
{
}

bool UMillicastMediaSource::Initialize(const FMillicastSignalingData& /*data*/)
{
  UE_LOG(LogMillicastPlayer, Log, TEXT("Initialize Media Source"));
  Buffer = nullptr;
  BufferSize = 0;
  return true;
}

void UMillicastMediaSource::BeginDestroy()
{
  Super::BeginDestroy();
}

/*
 * IMediaOptions interface
 */

FString UMillicastMediaSource::GetMediaOption(const FName& Key, const FString& DefaultValue) const
{
	if (Key == MillicastPlayerOption::StreamName)
	{
		return StreamName;
	}
	if (Key == MillicastPlayerOption::AccountId)
	{
		return AccountId;
	}
	return Super::GetMediaOption(Key, DefaultValue);
}

bool UMillicastMediaSource::HasMediaOption(const FName& Key) const
{

	if (   Key == MillicastPlayerOption::StreamName
		|| Key == MillicastPlayerOption::AccountId)
	{
		return true;
	}

	return Super::HasMediaOption(Key);
}

/*
 * UMediaSource interface
 */

FString UMillicastMediaSource::GetUrl() const
{
        return StreamUrl;
}

bool UMillicastMediaSource::Validate() const
{
	/*FString FailureReason;

	UE_LOG(LogMillicastPlayer, Warning, TEXT("Not yet implemented but good to be there"));
	return false;*/

	// TODO : check if stream name and account id are not empty

	return true;
}

/**
	Attempts to change the Video Texture object used as the video frame capture object
*/
void UMillicastMediaSource::ChangeVideoTexture(UMillicastMediaTexture2D* InVideoTexture)
{
	// FScopeLock Lock(&RenderSyncContext);

	if (IsValid(VideoTexture))
	{
		// make sure that the old texture is not referencing the rendering of this texture
		VideoTexture->UpdateTextureReference(FRHICommandListExecutor::GetImmediateCommandList(), nullptr);
	}

	// Just copy the new texture here.
	VideoTexture = InVideoTexture;
}

/**
	Updates the DynamicMaterial with the VideoTexture of this object
*/
void UMillicastMediaSource::UpdateMaterialTexture(UMaterialInstanceDynamic* MaterialInstance, FString ParameterName)
{
	// Ensure that both the material instance and the video texture are valid
	if (IsValid(MaterialInstance) && IsValid(VideoTexture))
	{
		// Call the function to set the texture parameter with the proper texture
		MaterialInstance->SetTextureParameterValue(FName(*ParameterName), this->VideoTexture);
	}
}

void UMillicastMediaSource::OnFrame(const webrtc::VideoFrame& frame)
{
  AsyncTask(ENamedThreads::ActualRenderingThread, [=]() {
      // code to execute on game thread here
    FScopeLock Lock(&RenderSyncContext);

    uint32_t Size = webrtc::CalcBufferSize(webrtc::VideoType::kARGB,
                                           frame.width(),
                                           frame.height());

    if(Size > BufferSize) {
      delete [] Buffer;
      Buffer = new uint8_t[Size];
      BufferSize = Size;
    }

    webrtc::ConvertFromI420(frame, webrtc::VideoType::kARGB, 0, Buffer);

    FIntPoint FrameSize = FIntPoint(frame.width(), frame.height());
    FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

    if (!RenderTargetDescriptor.IsValid() ||
            RenderTargetDescriptor.GetSize() != FIntVector(FrameSize.X, FrameSize.Y, 0))
    {
            // Create the RenderTarget descriptor
            RenderTargetDescriptor = FPooledRenderTargetDesc::Create2DDesc(
                    FrameSize, PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false);

            // Update the shader resource for the 'SourceTexture'
            FRHIResourceCreateInfo CreateInfo;
            TRefCountPtr<FRHITexture2D> DummyTexture2DRHI;
            RHICreateTargetableShaderResource2D(FrameSize.X, FrameSize.Y, PF_B8G8R8A8, 1, TexCreate_Dynamic,
                                                                                    TexCreate_RenderTargetable, false, CreateInfo, SourceTexture,
                                                                                    DummyTexture2DRHI);

            // Find a free target-able texture from the render pool
            GRenderTargetPool.FindFreeElement(RHICmdList, RenderTargetDescriptor, RenderTarget,
                                                                                  TEXT("MILLICASTPLAYER"));
    }

    // Create the update region structure
    FUpdateTextureRegion2D Region(0, 0, 0, 0, FrameSize.X, FrameSize.Y);

    // Set the Pixel data of the webrtc Frame to the SourceTexture
    RHIUpdateTexture2D(SourceTexture, 0, Region, frame.width() * 4, (uint8*&)Buffer);

    VideoTexture->UpdateTextureReference(RHICmdList, (FTexture2DRHIRef&)SourceTexture);
  });
}

#if WITH_EDITOR
bool UMillicastMediaSource::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	return true;
}

void UMillicastMediaSource::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(InPropertyChangedEvent);
}
#endif //WITH_EDITOR
