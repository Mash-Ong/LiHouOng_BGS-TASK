#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SkateCharacter.generated.h"

UCLASS()
class LIHOUONG_BGS_TASK_API ASkateCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASkateCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
