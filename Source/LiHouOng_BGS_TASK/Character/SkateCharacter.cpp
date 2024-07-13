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
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	Skateboard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Skateboard"));
	Skateboard->SetupAttachment(RootComponent);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 600.0f;
	GetCharacterMovement()->AirControl = 0.2f;
	GetCharacterMovement()->MaxWalkSpeed = 2000.0f;
	// Disable the following 2 properties to prevent interference with our custom skating movement
	GetCharacterMovement()->MaxAcceleration = 0.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 0.0f;

	CurrentVelocity = FVector::ZeroVector;
	Acceleration = 10000.0f;
	Deceleration = 200.0f;
	AccelerationDecayRate = 0.05f;
}

void ASkateCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	DefaultSkateBoardRelRot = Skateboard->GetRelativeRotation();
}

void ASkateCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SimulateSkatingMovement(DeltaTime);
	if (GetCharacterMovement()->GetLastUpdateVelocity().SizeSquared() > 0) // Is moving.
	{
		UpdateIKLocations();
	}
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
}

void ASkateCharacter::UpdateIKLocations()
{
	FVector FW_HitLoc, BW_HitLoc;
	TraceForSurface(Skateboard->GetSocketLocation("FW_Center"), 30.0f, FW_HitLoc);
	TraceForSurface(Skateboard->GetSocketLocation("BW_Center"), 30.0f, BW_HitLoc);
	FRotator LookAtRotation = FRotationMatrix::MakeFromX(FW_HitLoc - BW_HitLoc).Rotator() + DefaultSkateBoardRelRot;
	LookAtRotation = FMath::RInterpTo(Skateboard->GetComponentRotation(), LookAtRotation, GetWorld()->GetDeltaSeconds(), 10.0f);
	Skateboard->SetWorldRotation(LookAtRotation);
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
		float Speed = CurrentVelocity.Size();
		float EffectiveAcceleration = Acceleration * FMath::Exp(-AccelerationDecayRate * (Speed / GetCharacterMovement()->GetMaxSpeed()));

		CurrentVelocity += GetSkatingForwardDir() * GetForwardInput() * EffectiveAcceleration * GetWorld()->GetDeltaSeconds();
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
		TurnRate = GetVelocity().SquaredLength() / (GetCharacterMovement()->GetMaxSpeed() * GetCharacterMovement()->GetMaxSpeed());
		TurnRate = FMath::Clamp(TurnRate, 0.0f, 1.0f);
	}
	else
	{
		TurnRate = 1.0f;
	}
	TurnRate *= GetRightInput() < 0 ? -1 : 1;
	CurrentVelocity += GetSkatingRightDir() * TurnRate * GetWorld()->GetDeltaSeconds();
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
	CurrentVelocity += GetSkatingForwardDir() * Force;
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
	// Apply deceleration
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

	AddMovementInput(CurrentVelocity.GetSafeNormal(), CurrentVelocity.Size() * DeltaTime);
	MovementVector = FVector2D::ZeroVector;
}

FVector ASkateCharacter::GetSkatingForwardDir() const
{
	return FRotationMatrix(Skateboard->GetComponentRotation() + DefaultSkateBoardRelRot).GetUnitAxis(EAxis::X);
}

FVector ASkateCharacter::GetSkatingRightDir() const
{
	return FRotationMatrix(Skateboard->GetComponentRotation() + DefaultSkateBoardRelRot).GetUnitAxis(EAxis::Y);
}
