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

UENUM(BlueprintType)
enum class ESkatingState : uint8
{
	Standing, // No movement, standing on the board.
	Rolling, // No acceleration, flowing with the board until stopping.
	Pushing, // Held down the forward (A) key.
	Braking, // Held down the backward (D) key.
	Jumping
};

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
	UStaticMeshComponent* Skateboard;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

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

	bool TraceForSurface(const FVector& Origin, const float TraceHalfHeight, FVector& ImpactPoint);
	void SimulateSkatingMovement(float DeltaTime);
	FVector GetSkatingForwardDir() const;
	FVector GetSkatingRightDir() const;
protected:
	virtual void PostInitializeComponents() override;

	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void Landed(const FHitResult& Hit) override;

	// Perform raycasts to change the skateboard orientation and foot desired resting location.
	void UpdateIKLocations();

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

	UFUNCTION(BlueprintPure)
	ESkatingState GetState() const { return State; }

private:
	bool bShouldPush = false;

	ESkatingState State;

	FVector2D MovementVector;

	FRotator DefaultSkateBoardRelRot;

	float AutoPushInterval;
	FTimerHandle AutoPushTimerHandle;


	float Acceleration;
	float Deceleration;
	float AccelerationDecayRate;
	FVector CurrentVelocity;

};
