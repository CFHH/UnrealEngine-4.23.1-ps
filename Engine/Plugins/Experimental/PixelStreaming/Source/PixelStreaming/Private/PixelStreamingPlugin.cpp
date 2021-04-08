// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingPlugin.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "Streamer.h"
#include "RenderingThread.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RendererInterface.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "PixelStreamingInputDevice.h"
#include "PixelStreamingInputComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Dom/JsonObject.h"
#include "Misc/App.h"
#include "Misc/MessageDialog.h"

#if PLATFORM_WINDOWS
	#include "Windows/WindowsHWrapper.h"
	//#pragma comment(lib, "../../../../../../Intermediate/Build/Win64/UE4Editor/Development/SlateRHIRenderer/UE4Editor-SlateRHIRenderer.lib")
	//#pragma comment(lib, "G:/UnrealEngine-4.23.1-ps/Engine/Intermediate/Build/Win64/UE4Editor/Development/SlateRHIRenderer/UE4Editor-SlateRHIRenderer.lib")
#endif
#include "Runtime/Renderer/Private/PostProcess/SceneRenderTargets.h"

DEFINE_LOG_CATEGORY(PixelStreaming);
DEFINE_LOG_CATEGORY(PixelStreamingInput);
DEFINE_LOG_CATEGORY(PixelStreamingNet);
DEFINE_LOG_CATEGORY(PixelStreamingCapture);

/** IModuleInterface implementation */
void FPixelStreamingPlugin::StartupModule()
{
	// Check to see if we can use the Pixel Streaming plugin on this platform.
	// If not then we avoid setting up our delegates to prevent access to the
	// plugin. Note that Pixel Streaming is not currently performed in the
	// Editor.
	if (!GIsEditor && !CheckPlatformCompatibility())
	{
		return;
	}

	// detect hardware capabilities, init nvidia capture libs, etc
	check(GDynamicRHI);
	void* Device = GDynamicRHI->RHIGetNativeDevice();
	// During cooking RHI device is invalid, skip error logging in this case as it causes the build to fail.
	if (Device)
	{
		FString RHIName = GDynamicRHI->GetName();
		if (RHIName != TEXT("D3D11") && RHIName != TEXT("OpenGL"))
		{
			UE_LOG(PixelStreaming, Error, TEXT("Failed to initialise Pixel Streaming plugin because it only supports DX11 or OpenGL (Linux Only) RHI found was %s"), *RHIName);
			return;
		}
	}

	// subscribe to engine delegates here for init / framebuffer creation / whatever
	if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddRaw(this, &FPixelStreamingPlugin::OnBackAndDepthBufferReady_RenderThread);
			//FSlateApplication::Get().GetRenderer()->OnBackAndDepthBufferReadyToPresent().AddRaw(this, &FPixelStreamingPlugin::OnBackAndDepthBufferReady_RenderThread);
			FSlateApplication::Get().GetRenderer()->OnPreResizeWindowBackBuffer().AddRaw(this, &FPixelStreamingPlugin::OnPreResizeWindowBackbuffer);
		}

	}

	FGameModeEvents::GameModePostLoginEvent.AddRaw(this, &FPixelStreamingPlugin::OnGameModePostLogin);
	FGameModeEvents::GameModeLogoutEvent.AddRaw(this, &FPixelStreamingPlugin::OnGameModeLogout);

	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);

	FApp::SetUnfocusedVolumeMultiplier(1.0f);
}

void FPixelStreamingPlugin::ShutdownModule()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().RemoveAll(this);
		//FSlateApplication::Get().GetRenderer()->OnBackAndDepthBufferReadyToPresent().RemoveAll(this);
		FSlateApplication::Get().GetRenderer()->OnPreResizeWindowBackBuffer().RemoveAll(this);
	}

	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}

bool FPixelStreamingPlugin::CheckPlatformCompatibility() const
{
	bool bCompatible = true;

	#if PLATFORM_WINDOWS
	bool bWin8OrHigher = FWindowsPlatformMisc::VerifyWindowsVersion(6, 2);
	if (!bWin8OrHigher)
	{
		FString ErrorString(TEXT("Failed to initialize Pixel Streaming plugin because minimum requirement is Windows 8"));
		FText ErrorText = FText::FromString(ErrorString);
		FText TitleText = FText::FromString(TEXT("Pixel Streaming Plugin"));
		FMessageDialog::Open(EAppMsgType::Ok, ErrorText, &TitleText);
		UE_LOG(PixelStreaming, Error, TEXT("%s"), *ErrorString);
		bCompatible = false;
	}
	#endif

	if (!FStreamer::CheckPlatformCompatibility())
	{
		bCompatible = false;
	}

	return bCompatible;
}

void FPixelStreamingPlugin::UpdateViewport(FSceneViewport* Viewport)
{
	FRHIViewport* const ViewportRHI = Viewport->GetViewportRHI().GetReference();
}

/*
struct DepthPixel
{
	float depth;
	char stencil;
	char unused1;
	char unused2;
	char unused3;
};
*/

void FPixelStreamingPlugin::OnBackAndDepthBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer/*, const FTexture2DRHIRef& DepthBuffer*/)
{
	check(IsInRenderingThread());

	if (!Streamer)
	{
		FString IP = TEXT("0.0.0.0");
		FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingIP="), IP);
		uint16 Port = 8124;
		FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingPort="), Port);

		Streamer = MakeUnique<FStreamer>(*IP, Port, BackBuffer);
	}
	
	/*
	FTexture2DRHIRef DepthBuffer;
	ENQUEUE_RENDER_COMMAND(FPixelStreamingOnBackBufferReady)(
		[&DepthBuffer](FRHICommandListImmediate& RHICmdList)
		{
			DepthBuffer = FSceneRenderTargets::Get(RHICmdList).GetSceneDepthSurface();
		});
	FlushRenderingCommands();
	*/

	Streamer->OnFrameBufferReady(BackBuffer);
}

void FPixelStreamingPlugin::OnPreResizeWindowBackbuffer(void* BackBuffer)
{
	if (Streamer)
	{
		FPixelStreamingPlugin* Plugin = this;
		ENQUEUE_RENDER_COMMAND(FPixelStreamingOnPreResizeWindowBackbuffer)(
			[Plugin](FRHICommandListImmediate& RHICmdList)
			{
				Plugin->OnPreResizeWindowBackbuffer_RenderThread();
			});	

		// Make sure OnPreResizeWindowBackbuffer_RenderThread is executed before continuing
		FlushRenderingCommands();
	}
}

void FPixelStreamingPlugin::OnPreResizeWindowBackbuffer_RenderThread()
{
	Streamer->OnPreResizeWindowBackbuffer();
}

TSharedPtr<class IInputDevice> FPixelStreamingPlugin::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	InputDevice = MakeShareable(new FPixelStreamingInputDevice(InMessageHandler, InputComponents));
	return InputDevice;
}

FPixelStreamingInputDevice& FPixelStreamingPlugin::GetInputDevice()
{
	return *InputDevice;
}

TSharedPtr<FPixelStreamingInputDevice> FPixelStreamingPlugin::GetInputDevicePtr()
{
	return InputDevice;
}

void FPixelStreamingPlugin::AddClientConfig(TSharedRef<FJsonObject>& JsonObject)
{
	checkf(InputDevice.IsValid(), TEXT("No Input Device available when populating Client Config"));

	JsonObject->SetBoolField(TEXT("FakingTouchEvents"), InputDevice->IsFakingTouchEvents());

	FString PixelStreamingControlScheme;
	if (FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingControlScheme="), PixelStreamingControlScheme))
	{
		JsonObject->SetStringField(TEXT("ControlScheme"), PixelStreamingControlScheme);
	}

	float PixelStreamingFastPan;
	if (FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingFastPan="), PixelStreamingFastPan))
	{
		JsonObject->SetNumberField(TEXT("FastPan"), PixelStreamingFastPan);
	}
}

void FPixelStreamingPlugin::SendResponse(const FString& Descriptor)
{
	Streamer->SendResponse(Descriptor);
}

void FPixelStreamingPlugin::OnGameModePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	UWorld* NewPlayerWorld = NewPlayer->GetWorld();
	for (TObjectIterator<UPixelStreamingInputComponent> ObjIt; ObjIt; ++ObjIt)
	{
		UPixelStreamingInputComponent* InputComponent = *ObjIt;
		UWorld* InputComponentWorld = InputComponent->GetWorld();
		if (InputComponentWorld == NewPlayerWorld)
		{
			InputComponents.Push(InputComponent);
		}
	}
	if (InputComponents.Num() == 0)
	{
		UPixelStreamingInputComponent* InputComponent = NewObject<UPixelStreamingInputComponent>(NewPlayer);
		InputComponent->RegisterComponent();
		InputComponents.Push(InputComponent);
	}
}

void FPixelStreamingPlugin::OnGameModeLogout(AGameModeBase* GameMode, AController* Exiting)
{
	InputComponents.Empty();
}

IMPLEMENT_MODULE(FPixelStreamingPlugin, PixelStreaming)
