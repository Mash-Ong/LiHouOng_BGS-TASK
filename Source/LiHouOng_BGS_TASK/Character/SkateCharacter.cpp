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
	
	// Setting to slide along wall
	GetCharacterMovement()->bEnablePhysicsInteraction = true;
	GetCharacterMovement()->bMaintainHorizontalGroundVelocity = false;
	GetCharacterMovement()->bAlwaysCheckFloor = true;
	GetCharacterMovement()->bUseFlatBaseForFloorChecks = true;
}

void ASkateCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SimulateSkatingMovement(DeltaTime);
	UpdateCameraBoom();
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

		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Started, this, &ASkateCharacter::StartBraking);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ASkateCharacter::StopBraking);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASkateCharacter::Move);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ASkateCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (GetForwardInput() > 0.0f)
	{
		StartPushing();
	}
	UpdateIKLocations();
}

void ASkateCharacter::UpdateIKLocations()
{
	FVector FW_HitLoc, BW_HitLoc;
	bool bFW_Hit = TraceForSurface(SkateboardMesh->GetSocketLocation("FW_Center"), 100.0f, FW_HitLoc);
	bool bBW_Hit = TraceForSurface(SkateboardMesh->GetSocketLocation("BW_Center"), 100.0f, BW_HitLoc);
	{
		FRotator LookAtRotation = FRotationMatrix::MakeFromX(FW_HitLoc - BW_HitLoc).Rotator();
		LookAtRotation = FMath::RInterpTo(SkateboardRoot->GetComponentRotation(), LookAtRotation, GetWorld()->GetDeltaSeconds(), 10.0f);
		SkateboardRoot->SetWorldRotation(LookAtRotation);
	}
}

void ASkateCharacter::StartBraking()
{
	StopPushing();
	PlayBrakeAnim();
}


void ASkateCharacter::StopBraking()
{
	ReverseBrakeAnim();

	if (GetForwardInput() > 0.0f)
		StartPushing();
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

	if (GetRightInput() != 0.0f)
		Turn();

	if (GetForwardInput() >= 0.0f)
	{
		AddMovementInput(GetSkatingForwardDir() * GetForwardInput(), 1.0f);
	}
	else
	{
		// Apply a force to decelerate the actor
		if (GetVelocity().SquaredLength() <= 0.0f)
			return;

		GetCharacterMovement()->AddForce(-GetVelocity() * 250.0f);
	}
}

void ASkateCharacter::Turn()
{
	float TurnRate;
	if (GetCharacterMovement()->IsMovingOnGround())
	{
		// The character is harder to turn if the speed is slow.
		TurnRate = GetVelocity().SquaredLength() / (FullTurnSpeed * FullTurnSpeed);
		TurnRate = FMath::Clamp(TurnRate, 0.0f, 1.0f);
	}
	else
	{
		TurnRate = 1.0f;
	}
	TurnRate *= GetRightInput() < 0 ? -1 : 1;
	AddControllerYawInput(GetCharacterMovement()->RotationRate.Yaw * TurnRate * GetWorld()->GetDeltaSeconds());
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
}

void ASkateCharacter::GetFootPlacements(FVector& LF_Loc, FVector& RF_Loc)
{
	TraceForSurface(GetMesh()->GetBoneLocation("LeftFoot"), 10.0f, LF_Loc);
	TraceForSurface(GetMesh()->GetBoneLocation("LeftFoot"), 10.0f, RF_Loc);
}

bool ASkateCharacter::TraceForSurface(const FVector& Origin, const float TraceHalfHeight, FVector& ImpactPoint, bool IgnoreSelf)
{
	FCollisionQueryParams QueryParams;
	QueryParams.bReturnPhysicalMaterial = true;
	if (IgnoreSelf)
	{
		QueryParams.AddIgnoredActor(this);
	}
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin + FVector(0.0f, 0.0f, TraceHalfHeight),
		Origin + FVector(0.0f, 0.0f, -TraceHalfHeight), ECC_Camera, QueryParams))
	{
		ImpactPoint = Hit.ImpactPoint;
		return true;
	}
	ImpactPoint = Origin;
	return false;
}

void ASkateCharacter::SimulateSkatingMovement(float DeltaTime)
{
	if (GetCharacterMovement()->GetLastUpdateVelocity().SizeSquared() > 0) // Is moving.
	{
		UpdateIKLocations();
		AlignSkateboardWithVelocity();
	}
	
	MovementVector = FVector2D::ZeroVector;
}

void ASkateCharacter::UpdateCameraBoom()
{
	FRotator CamBoomRot = CameraBoom->GetRelativeRotation();
	float Alpha = FMath::Clamp(FMath::Abs(GetVelocity().Z) / 15.0f, 0.0f, 1.0f);
	CamBoomRot.Pitch = Alpha * (GetVelocity().Z < 0.0f ? MinCamPitch : MaxCamPitch);
	CameraBoom->SetRelativeRotation(CamBoomRot);

	// Camera focus to the actor.
 	FRotator LookAtRotation = FRotationMatrix::MakeFromX(CameraBoom->GetComponentLocation() - FollowCamera->GetComponentLocation()).Rotator();
 	LookAtRotation = FMath::RInterpTo(FollowCamera->GetComponentRotation(), LookAtRotation, GetWorld()->GetDeltaSeconds(), 10.0f);
 	FollowCamera->SetWorldRotation(LookAtRotation);
}

FVector ASkateCharacter::GetSkatingForwardDir() const
{
	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	
	return FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
}

FVector ASkateCharacter::GetSkatingRightDir() const
{
	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	return FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
}

void ASkateCharacter::AlignSkateboardWithVelocity()
{
	if (GetCharacterMovement()->IsMovingOnGround() && !IsBraking())
	{
		// Try to align skateboard with the velocity.
		const FVector MovingDir = GetVelocity().GetSafeNormal();
		FRotator DesiredRot = MovingDir.Rotation();
		float DotProduct = FVector::DotProduct(SkateboardRoot->GetForwardVector(), MovingDir);
		if (DotProduct < 0)
		{
			DesiredRot.Yaw += 180.0f;
		}
		SkateboardRoot->SetWorldRotation(FMath::RInterpTo(SkateboardRoot->GetComponentRotation(), DesiredRot, GetWorld()->GetDeltaSeconds(), 10.0f));
	}
}

bool ASkateCharacter::IsBraking() const
{
	return GetForwardInput() < 0.0f;
}
