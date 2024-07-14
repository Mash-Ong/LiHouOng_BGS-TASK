#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SkateCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UStaticMeshComponent;
struct FInputActionValue;

UCLASS()
class LIHOUONG_BGS_TASK_API ASkateCharacter : public ACharacter
{
	GENERATED_BODY()

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Mesh, meta = (AllowPrivateAccess = "true"))
	USceneComponent* SkateboardRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Mesh, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* SkateboardMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** W key for acceleration (push). S key for deceleration (brake). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* PushAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* BrakeAction;

public:
	// Sets default values for this character's properties
	ASkateCharacter();

private:
	virtual void Jump() override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Turn();
	void StartPushing();
	void StopPushing();

	bool TraceForSurface(const FVector& Origin, const float TraceHalfHeight, FVector& ImpactPoint, bool IgnoreSelf = true);
	void SimulateSkatingMovement(float DeltaTime);

	// Change pitch and target arm length based on the character's velocity.
	void UpdateCameraBoom();
	FVector GetSkatingForwardDir() const;
	FVector GetSkatingRightDir() const;
	void AlignSkateboardWithVelocity();
	bool IsBraking() const;

protected:

	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void Landed(const FHitResult& Hit) override;

	// Perform raycasts to change the skateboard orientation and foot desired resting location.
	void UpdateIKLocations();

	void StartBraking();

	void StopBraking();

	// Will play a timeline animation as feedback for braking in the blueprint.
	UFUNCTION(BlueprintImplementableEvent)
	void PlayBrakeAnim();

	// Will play a timeline animation as feedback for braking in the blueprint.
	UFUNCTION(BlueprintImplementableEvent)
	void ReverseBrakeAnim();

public:
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintPure)
	float GetForwardInput() const { return MovementVector.Y; }

	UFUNCTION(BlueprintPure)
	float GetRightInput() const { return MovementVector.X; }

	UFUNCTION(BlueprintPure)
	bool ShouldPush() const { return bShouldPush; }

	UFUNCTION(BlueprintCallable)
	void Push(const float Force);

	// For leg IK in the animation blueprint.
	UFUNCTION(BlueprintCallable)
	void GetFootPlacements(FVector& LF_Loc, FVector& RF_Loc);

private:
	bool bShouldPush = false;

	FVector2D MovementVector;

	float AutoPushInterval;
	FTimerHandle AutoPushTimerHandle;

	// To make the character harder to turn if the speed is slow.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	float FullTurnSpeed = 300.0f;

	// CameraBoom's relative pitch will change between -CameraPitchRange and CameraPitchRange.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true", ClampMin = "-70.0", ClampMax = "0.0"))
	float MinCamPitch = -15.0f;

	// CameraBoom's relative pitch will change between -CameraPitchRange and CameraPitchRange.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "70.0"))
	float MaxCamPitch = 45.0f;
};
