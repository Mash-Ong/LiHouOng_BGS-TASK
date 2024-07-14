#include "SkateCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

ASkateCharacter::ASkateCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = false; // Rotate the arm based on the controller

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	SkateboardRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SkateBoard"));
	SkateboardRoot->SetupAttachment(RootComponent);

	SkateboardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkateboardMesh"));
	SkateboardMesh->SetupAttachment(SkateboardRoot);

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 600.0f;
	GetCharacterMovement()->AirControl = 0.2f;
	//// The following 3 properties are reused in calculating our custom skating movement
	//GetCharacterMovement()->MaxWalkSpeed = 2000.0f;
	//GetCharacterMovement()->MaxAcceleration = 0.0f;
	//GetCharacterMovement()->BrakingDecelerationWalking = 0.0f;

	/*CurrentVelocity = FVector::ZeroVector;
	Acceleration = 600.0f;
	Deceleration = 200.0f;
	AccelerationDecayRate = 0.05f;*/
}

void ASkateCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	DefaultSkateBoardRelRot = SkateboardMesh->GetRelativeRotation();
}

void ASkateCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SimulateSkatingMovement(DeltaTime);
	if (GetCharacterMovement()->GetLastUpdateVelocity().SizeSquared() > 0) // Is moving.
	{
		UpdateIKLocations();
	}
	//SkateboardRoot->SetRelativeRotation(FRotator(SkateboardRoot->GetRelativeRotation().Roll, GetControlRotation().Yaw, SkateboardRoot->GetRelativeRotation().Pitch));
}

void ASkateCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) 
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(PushAction, ETriggerEvent::Started, this, &ASkateCharacter::StartPushing);
		EnhancedInputComponent->BindAction(PushAction, ETriggerEvent::Completed, this, &ASkateCharacter::StopPushing);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASkateCharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASkateCharacter::Look);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ASkateCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	UpdateIKLocations();
}

void ASkateCharacter::UpdateIKLocations()
{
	FVector FW_HitLoc, BW_HitLoc;
	bool bFW_Hit = TraceForSurface(SkateboardMesh->GetSocketLocation("FW_Center"), 50.0f, FW_HitLoc);
	bool bBW_Hit = TraceForSurface(SkateboardMesh->GetSocketLocation("BW_Center"), 50.0f, BW_HitLoc);
	{
		FRotator LookAtRotation = FRotationMatrix::MakeFromX(FW_HitLoc - BW_HitLoc).Rotator();
		LookAtRotation = FMath::RInterpTo(SkateboardRoot->GetComponentRotation(), LookAtRotation, GetWorld()->GetDeltaSeconds(), 10.0f);
		SkateboardRoot->SetWorldRotation(LookAtRotation);
	}
}

void ASkateCharacter::Jump()
{
	Super::Jump();
	StopPushing();
}

void ASkateCharacter::Move(const FInputActionValue& Value)
{
	// Input is a Vector2D. Y is forward, X is right.
	MovementVector = Value.Get<FVector2D>();

	if (GetForwardInput() >= 0.0f)
	{
		// Calculate decay based on current speed
		/*float Speed = CurrentVelocity.Size();
		float EffectiveAcceleration = FMath::Exp(-AccelerationDecayRate * (Speed / GetCharacterMovement()->GetMaxSpeed()));*/
		AddMovementInput(GetSkatingForwardDir() * GetForwardInput(), 1.0f);
	}

	if (GetRightInput() != 0.0f)
		Turn();
}

void ASkateCharacter::Turn()
{
	float TurnRate;
	if (GetCharacterMovement()->IsMovingOnGround())
	{
		// The character is harder to turn if the speed is slow.
		const float FullTurnSpeed = 300.0f;
		TurnRate = GetVelocity().SquaredLength() / (FullTurnSpeed * FullTurnSpeed);
		TurnRate = FMath::Clamp(TurnRate, 0.0f, 1.0f);
	}
	else
	{
		TurnRate = 1.0f;
	}
	TurnRate *= GetRightInput() < 0 ? -1 : 1;
	AddControllerYawInput(GetCharacterMovement()->RotationRate.Yaw * TurnRate * GetWorld()->GetDeltaSeconds());
	//AddMovementInput(GetSkatingRightDir(), TurnRate);
}

void ASkateCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ASkateCharacter::StartPushing()
{
	if (GetCharacterMovement()->IsMovingOnGround())
	bShouldPush = true;
	GetWorld()->GetTimerManager().SetTimer(AutoPushTimerHandle, this, &ASkateCharacter::StartPushing, AutoPushInterval, false);
}

void ASkateCharacter::StopPushing()
{
	bShouldPush = false;
	if (GetWorld()->GetTimerManager().IsTimerActive(AutoPushTimerHandle))
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoPushTimerHandle);
	}
}

void ASkateCharacter::Push(const float Force)
{
	GetCharacterMovement()->AddImpulse(GetSkatingForwardDir() * Force);
	//CurrentVelocity += GetSkatingForwardDir() * Force;
}

bool ASkateCharacter::TraceForSurface(const FVector& Origin, const float TraceHalfHeight, FVector& ImpactPoint)
{
	FCollisionQueryParams QueryParams;
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.AddIgnoredActor(this);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin + FVector(0.0f, 0.0f, TraceHalfHeight),
		Origin + FVector(0.0f, 0.0f, -TraceHalfHeight), ECC_Camera, QueryParams))
	{
		ImpactPoint = Hit.ImpactPoint;
		return true;
	}
	return false;
}

void ASkateCharacter::SimulateSkatingMovement(float DeltaTime)
{
	/*Apply deceleration
		if (CurrentVelocity.Size() > 0)
		{
			FVector DecelerationVector = -CurrentVelocity.GetSafeNormal() * Deceleration * DeltaTime;
			if (DecelerationVector.Size() > CurrentVelocity.Size())
			{
				CurrentVelocity = FVector::ZeroVector;
			}
			else
			{
				CurrentVelocity += DecelerationVector;
			}
		}
		AddMovementInput(CurrentVelocity.GetSafeNormal(), 1.0f);
		MovementVector = FVector2D::ZeroVector;*/
}

FVector ASkateCharacter::GetSkatingForwardDir() const
{
	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	
	return FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

	//return FRotationMatrix(SkateboardMesh->GetComponentRotation() - DefaultSkateBoardRelRot).GetUnitAxis(EAxis::X);
}

FVector ASkateCharacter::GetSkatingRightDir() const
{
	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	return FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	//return FRotationMatrix(SkateboardMesh->GetComponentRotation() - DefaultSkateBoardRelRot).GetUnitAxis(EAxis::Y);
}
