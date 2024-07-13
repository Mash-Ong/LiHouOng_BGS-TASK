// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiHouOng_BGS_TASKGameMode.h"
#include "LiHouOng_BGS_TASKCharacter.h"
#include "UObject/ConstructorHelpers.h"

ALiHouOng_BGS_TASKGameMode::ALiHouOng_BGS_TASKGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
