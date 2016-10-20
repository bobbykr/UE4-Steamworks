//Copyright 2016 davevillz, https://github.com/davevill

#include "SteamworksPrivatePCH.h"
#include "SteamworksGameInstance.h"
#include "Runtime/Networking/Public/Networking.h"
#include "Http.h"
#include "UniqueNetIdSteam.h"
#include "SteamworksGameMode.h"





class FSteamworksCallbacks
{

public:

	STEAM_GAMESERVER_CALLBACK(FSteamworksCallbacks, OnValidateTicket, ValidateAuthTicketResponse_t, OnValidateTicketCallback);



	TWeakObjectPtr<USteamworksGameInstance> GameInstance;

	FSteamworksCallbacks(USteamworksGameInstance* Owner) :
		GameInstance(Owner),
		OnValidateTicketCallback(this, &FSteamworksCallbacks::OnValidateTicket)
	{

	}


	


};

void FSteamworksCallbacks::OnValidateTicket(ValidateAuthTicketResponse_t* pResponse)
{
	if (pResponse->m_eAuthSessionResponse == k_EAuthSessionResponseOK)
	{

		FUniqueNetIdSteam SteamId(pResponse->m_SteamID);


		ASteamworksGameMode* GameMode = GameInstance->GetWorld()->GetAuthGameMode<ASteamworksGameMode>();

		if (GameMode)
		{
			GameMode->GetGameSessionClass();
		}

		int32 i = 0;
		i++;
		//Set a pending kick request, so when user fully logins will get kicked, if already in should happend instantly

		//TODO ban the user if this happends

		return;
	}
}

USteamworksGameInstance::USteamworksGameInstance()
{
}



#define STEAM_TICKET_BUFFER_SIZE 32768
char ticketBuffer[STEAM_TICKET_BUFFER_SIZE];
uint32 ticketSize;

void USteamworksGameInstance::Init()
{
	Super::Init();



	Callbacks = new FSteamworksCallbacks(this);



	FHttpModule* HTTP = &FHttpModule::Get();

	if (HTTP && HTTP->IsHttpEnabled())
	{

		FString TargetHost = "http://api.ipify.org";

		TSharedRef<IHttpRequest> Request = HTTP->CreateRequest();
		Request->SetVerb("GET");
		Request->SetURL(TargetHost);
		Request->SetHeader("User-Agent", "SteamworksUnreal/1.0");
		Request->SetHeader("Content-Type", "text/html");

		TWeakObjectPtr<USteamworksGameInstance> Instance = this;

		Request->OnProcessRequestComplete().BindLambda(
			[=](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded)
			{
				if (bSucceeded && Instance.IsValid())
				{
					Instance->OnPublicAddressResolved(Response->GetContentAsString());
				}
			}
		);

		Request->ProcessRequest();
	}

	if (IsDedicatedServerInstance() == false)
	{
		if (SteamAPI_Init())
		{
			UE_LOG(SteamworksLog, Log, TEXT("SteamAPI_Init() succeeded"));
		}
		else
		{
			UE_LOG(SteamworksLog, Warning, TEXT("SteamAPI_Init() failed, make sure to run this with steam or if in development add the steam_appid.txt in the binary folder"));
		}	

		GetWorld()->GetTimerManager().SetTimer(PollHandle, this, &USteamworksGameInstance::Poll, 0.5f, true);	
	}
}

ULocalPlayer* USteamworksGameInstance::CreateInitialPlayer(FString& OutError)
{
	ULocalPlayer* LocalPlayer = Super::CreateInitialPlayer(OutError);

	return LocalPlayer;
}

void USteamworksGameInstance::OnPublicAddressResolved(FString IpString)
{
	//if (IsRunningDedicatedServer())
	{
		FIPv4Address Ip;
		
		FIPv4Address::Parse(IpString, Ip);

		//TODO get settings from config
		if (!SteamGameServer_Init(Ip.Value, 7776, 7777, 7778, EServerMode::eServerModeAuthentication, "0.0.1"))
		{
			UE_LOG(SteamworksLog, Log, TEXT("SteamGameServer_Init() failed"));
		}

		if (SteamGameServer())
		{
			// Set the "game dir".
			// This is currently required for all games.  However, soon we will be
			// using the AppID for most purposes, and this string will only be needed
			// for mods.  it may not be changed after the server has logged on
			//SteamGameServer()->SetModDir( pchGameDir );

			// These fields are currently required, but will go away soon.
			// See their documentation for more info
			SteamGameServer()->SetProduct( "Pavlov" );
			SteamGameServer()->SetGameDescription( "VR Shooter" );

			// We don't support specators in our game.
			// .... but if we did:
			//SteamGameServer()->SetSpectatorPort( ... );
			//SteamGameServer()->SetSpectatorServerName( ... );

			// Initiate Anonymous logon.
			// Coming soon: Logging into authenticated, persistent game server account
			SteamGameServer()->LogOnAnonymous();


			// We want to actively update the master server with our presence so players can
			// find us via the steam matchmaking/server browser interfaces
			SteamGameServer()->EnableHeartbeats(true);


			UE_LOG(SteamworksLog, Log, TEXT("SteamGameServer LogOnAnonymous"));



		}
			
	}
}

void USteamworksGameInstance::Shutdown()
{
	/*if (!IsDedicatedServerInstance())
	{
		SteamAPI_Shutdown();
	}
	else
	{
		SteamGameServer_Shutdown();
	}*/

	delete Callbacks;
	Callbacks = nullptr;

	Super::Shutdown();
}

void USteamworksGameInstance::Poll()
{
	SteamAPI_RunCallbacks();
}


