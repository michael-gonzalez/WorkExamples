// Fill out your copyright notice in the Description page of Project Settings.

#include "HacknSlacks.h"
#include "WeaponFactory.h"
#include "AttackDictionary.h"
#include "Weapon.h"
#include "Chest.h"
#include "Enemy.h"
#include "BuffDef.h"
#include "CharacterAnimInstance.h"
#include "HackNSlacksGameMode.h"
#include "HNSGameInstance.h"
#include "Runtime/Engine/Classes/Kismet/KismetMaterialLibrary.h"
#include "HacknSlacksPlayer.h"

AHacknSlacksPlayer::AHacknSlacksPlayer(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	eTeam = ETeams::Player;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = ObjectInitializer.CreateDefaultSubobject<USpringArmComponent>(this, TEXT("CameraBoom"));
	CameraBoom->AttachTo(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller
	
	// Create a follow camera
	FollowCamera = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("FollowCamera"));
	FollowCamera->AttachTo(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	socketFront = new FName("skt_ParticleFront");
	socketUpper = new FName("skt_ParticleUpper");
	socketLower = new FName("skt_ParticleLower");

	for (int32 iSheath = 0; iSheath < (int32)ESheaths::Count; iSheath++)
		aoSheaths[iSheath].eSheath = (ESheaths)iSheath;

	oLastCheckpoint = FTransform((FVector)NAN);
}

void AHacknSlacksPlayer::BeginPlay()
{
	Super::BeginPlay();

	oLastGroundPosition = GetActorLocation();

	if (pkSoftLockSphere)
	{
		TScriptDelegate<> kBeginOverlap;
		kBeginOverlap.BindUFunction(this, "SoftLockSphereBeginOverlap");

		pkSoftLockSphere->OnComponentBeginOverlap.Add(kBeginOverlap);

		TScriptDelegate<> kEndOverlap;
		kEndOverlap.BindUFunction(this, "SoftLockSphereEndOverlap");

		pkSoftLockSphere->OnComponentEndOverlap.Add(kEndOverlap);
	}

	iLives = 3;

	UHNSGameInstance::pkPlayer = this;

	// TEST
	//AddBuff(UBuffDef::StaticClass(), 1.0f, 10.0f, 1.0f);
}

void AHacknSlacksPlayer::TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

	// reset soft lock target is the target is being destroyed
	if (pkSoftLockedTarget && pkSoftLockedTarget->IsPendingKill())
		pkSoftLockedTarget = nullptr;

	if (UCharacterMovementComponent* pkCharMovement = GetCharacterMovement())
	{
		// set velocity to dodging speed
		if (bDodging)
			pkCharMovement->Velocity = FVector(oDodgeDir.X * fDodgeSpeed, oDodgeDir.Y * fDodgeSpeed, pkCharMovement->Velocity.Z);

		// scale deceleration based on movement speed
		pkCharMovement->BrakingDecelerationWalking = FMath::Lerp(fMinDeceleration, fMaxDeceleration, FMath::Min(1.0f, pkCharMovement->Velocity.Size() / fSprintSpeed));
	}

	if (bOnGround)
	{
		// save ground position
		oLastGroundPosition = GetActorLocation();
		fAirTimer = 0.0f;
	}
	else if ((fAirTimer += DeltaTime) >= fMaxAirTime)
	{
		// reset to last saved ground position
		SetActorLocation(oLastGroundPosition);
		fAirTimer = 0.0f;
	}

	if (InputComponent)
	{
		FVector oInputDir = FVector(InputComponent->GetAxisValue("MoveForward"), InputComponent->GetAxisValue("MoveRight"), 0.0f);

		FVector oTargetDir = oInputDir.IsZero() ? FollowCamera->GetForwardVector() : FRotator(0.0f, FollowCamera->GetComponentRotation().Yaw, 0.0f).RotateVector(oInputDir.GetSafeNormal());

		// only get a new target each tick if the player is not attacking
		//if (!poCurrentAttack)
			GetAttackAngle(oTargetDir);

		if (pkSoftLockArrow)
		{
			if (pkSoftLockedTarget)
			{
				// set arrows position to above the soft lock target
				pkSoftLockArrow->SetWorldLocation(pkSoftLockedTarget->GetActorLocation() + FVector(0.0f, 0.0f, pkSoftLockedTarget->GetSimpleCollisionHalfHeight() * 2));
				pkSoftLockArrow->SetHiddenInGame(false);
			}
			else
				pkSoftLockArrow->SetHiddenInGame(true);
		}
	}

	if (iComboHitCount > 0 && (fComboNoHitTimer += DeltaTime) >= fComboNoHitDuration)
		ResetComboCounter();

	if (pkCharAnim)
	{
		// dunno if this check could still cause a crash - maybe store references instead?
		if (pkClosestAngleTarget != nullptr && pkClosestAngleTarget->IsValidLowLevel())
			pkCharAnim->oLookRot = (pkClosestAngleTarget->GetActorLocation() - GetActorLocation()).Rotation();
		else
			pkCharAnim->oLookRot = FollowCamera->GetComponentRotation();
	}

	PitchAutoAdjustment(DeltaTime);
}

void AHacknSlacksPlayer::ReceiveAnyDamage(float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser)
{
	Super::ReceiveAnyDamage(Damage, DamageType, InstigatedBy, DamageCauser);

	// update shaders
	UpdateFX();
}

float AHacknSlacksPlayer::SetHealth(float fNewHealth)
{
	Super::SetHealth(fNewHealth);

	// update shaders
	UpdateFX();

	return fHealth;
}

float AHacknSlacksPlayer::ModHealth(float fMod)
{
	Super::ModHealth(fMod);

	// update shaders
	UpdateFX();

	return fHealth;
}

void AHacknSlacksPlayer::SetTokens(int32 iNewTokens)
{
	iTokens = FMath::Max(iNewTokens, 0);
}

// change number of tokens the player has
/*void AHacknSlacksPlayer::ModTokens(int32 iMod)
{
	iTokens = FMath::Max(iTokens + iMod, 0);
}*/

/*void AHacknSlacksPlayer::SetCrystals(int32 iNewCrystals)
{
	iCrystals = FMath::Max(iNewCrystals, 0);
}*/

// change number of tokens the player has
/*void AHacknSlacksPlayer::ModCrystals(int32 iMod)
{
	iCrystals = FMath::Max(iCrystals + iMod, 0);
}*/

// set if the player is dodging, set ground friction
void AHacknSlacksPlayer::SetDodging(bool bIsDodging)
{
	if (bDodging != bIsDodging)
	{
		UCharacterMovementComponent* pkCharMovement = GetCharacterMovement();

		if (bDodging)
			pkCharMovement->GroundFriction = fSavedFriction;
		else
		{
			fSavedFriction = pkCharMovement->GroundFriction;
			pkCharMovement->GroundFriction = 0.0f;
		}

		Super::SetDodging(bIsDodging);
	}
}

// when the soft lock sphere begins overlapping an enemy
void AHacknSlacksPlayer::SoftLockSphereBeginOverlap(class AActor* pkOther, class UPrimitiveComponent* pkOtherComp, int32 iOtherBodyIndex, bool bFromSweep, const FHitResult &oSweepResult)
{
	if (AEnemy* pkEnemy = Cast<AEnemy>(pkOther))
		if (pkEnemy->fHealth > 0.0f && !apkNearbyEnemies.FindNode(pkEnemy))
			apkNearbyEnemies.AddTail(pkEnemy);
}

// when the soft lock sphere stops overlapping an enemy
void AHacknSlacksPlayer::SoftLockSphereEndOverlap(class AActor* pkOther, class UPrimitiveComponent* pkOtherComp, int32 iOtherBodyIndex)
{
	if (AEnemy* pkEnemy = Cast<AEnemy>(pkOther))
		apkNearbyEnemies.RemoveNode(pkEnemy);
}

/*FBuff& AHacknSlacksPlayer::AddBuff(TSubclassOf<UBuffDef> pkBuffDef, float fIntensity, float fDuration, int32 iTickCount)
{
	FBuff& oBuff = Super::AddBuff(pkBuffDef, fIntensity, fDuration, iTickCount);

	if (oBuff.iIndex == -1)
		oBuff.iIndex = iActiveBuffs++;

	OnAddBuff(oBuff);

	return oBuff;
}*/

// do any other logic for hits here, lifesteal, etc
void AHacknSlacksPlayer::AddComboHit(float fDamage)
{
	// reset timer to zero
	fComboNoHitTimer = 0.0f;

	// increase combo counter
	iComboHitCount++;

	// call blueprint event
	OnComboCountIncreased(iComboHitCount);
}

void AHacknSlacksPlayer::ResetComboCounter()
{
	// call blueprint event
	OnComboCountReset(iComboHitCount);

	fComboNoHitTimer = 0.0f;
	iComboHitCount = 0;
}

// change weapon - does not set any animation
/*bool AHacknSlacksPlayer::SetWeapon(AWeapon* pkNewWeapon)
{
	// same weapon
	if (pkWeapon == pkNewWeapon)
		return true;

	ResetCombo();

	// unequip current weapon
	if (pkWeapon)
	{
		FSheath& oSheath = aoSheaths[(int32)pkWeapon->eSheath];

		oSheath.pkWeapon = pkWeapon;
		pkWeapon->Unequip(GetMesh(), oSheath.sSocketName);
	}

	// equip weapon
	pkWeapon = pkNewWeapon;

	if (pkWeapon)
	{
		FSheath& oSheath = aoSheaths[(int32)pkWeapon->eSheath];

		// need to check in case the previous weapon was placed in this sheath
		if (pkWeapon == oSheath.pkWeapon)
			oSheath.pkWeapon = nullptr;

		pkWeapon->Equip();
	}

	return true;
}*/

// enum version to swap with sheathed weapon
/*bool AHacknSlacksPlayer::SetWeapon(ESheaths eSheath)
{
	return SetWeapon(aoSheaths[(int32)eSheath].pkWeapon);
}*/

// choose animation for changing weapon
/*void AHacknSlacksPlayer::AnimSetWeapon(ESheaths eNewSheath)
{
	ESheaths eCurSheath = pkWeapon ? pkWeapon->eSheath : ESheaths::Count;
	
	FSheath& oSheath = aoSheaths[(int32)eCurSheath];
	FSheathAnim& oSheathAnim = oSheath.aoSheathAnims[(int32)eNewSheath];

	// anim notify will call SetWeapon
	if (oSheathAnim.pkSwapAnim)
		pkCharAnim->SetAnim(EBodyPoses::UpperBody, oSheathAnim.pkSwapAnim, 1.0f, false);
	else
		SetWeapon(aoSheaths[(int32)eNewSheath].pkWeapon);
}*/

// called from the armory to apply weapons to slacks
/*void AHacknSlacksPlayer::AssignWeapon(AWeapon* pkSheathWeapon)
{
	FSheath& oSheath = aoSheaths[(int32)pkSheathWeapon->eSheath];

	pkSheathWeapon->pkOwner = this;
	pkSheathWeapon->oMainWeapon.pkCollider->pkOwner = this;
	pkSheathWeapon->oOffWeapon.pkCollider->pkOwner = this;

	// weapon should render behind objects
	pkSheathWeapon->oMainWeapon.SetRenderCustomDepth(true);
	pkSheathWeapon->oOffWeapon.SetRenderCustomDepth(true);
	
	pkSheathWeapon->pkMesh->SetSimulatePhysics(false);
	
	pkSheathWeapon->SetOnGround(false);

	// equip weapon
	if (!pkWeapon)
		SetWeapon(pkSheathWeapon);
	// sheath weapon
	else
	{
		oSheath.pkWeapon = pkSheathWeapon;
		pkSheathWeapon->pkMesh->AttachTo(GetMesh(), oSheath.sSocketName, EAttachLocation::SnapToTarget, false);
	}
}*/

//////////////////////////////////////////////////////////////////////////
// Input

void AHacknSlacksPlayer::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	Super::SetupPlayerInputComponent(InputComponent);

	// Set up gameplay key bindings
	check(InputComponent);
	InputComponent->BindAction("Jump", IE_Pressed, this, &AHacknSlacksPlayer::Jump);
	InputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	InputComponent->BindAxis("MoveForward", this, &AHacknSlacksPlayer::MoveForward);
	InputComponent->BindAxis("MoveRight", this, &AHacknSlacksPlayer::Strafe);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	InputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	InputComponent->BindAxis("TurnRate", this, &AHacknSlacksPlayer::TurnAtRate);
	InputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	InputComponent->BindAxis("LookUpRate", this, &AHacknSlacksPlayer::LookUpAtRate);

	InputComponent->BindAction("LightAttack", IE_Pressed, this, &AHacknSlacksPlayer::OnLightAttack);
	InputComponent->BindAction("HeavyAttack", IE_Pressed, this, &AHacknSlacksPlayer::OnHeavyAttack);
	InputComponent->BindAction("LightAttack", IE_Released, this, &AHacknSlacksPlayer::EndCharge);
	InputComponent->BindAction("HeavyAttack", IE_Released, this, &AHacknSlacksPlayer::EndCharge);
	//InputComponent->BindAction("Dodge", IE_Pressed, this, &AHacknSlacksPlayer::OnDodge);
	InputComponent->BindAction("SwapWeapon", IE_Pressed, this, &AHacknSlacksPlayer::OnSwapWeapon);
	InputComponent->BindAction("Interact", IE_Pressed, this, &AHacknSlacksPlayer::OnInteract);
	InputComponent->BindAction("Jump", IE_Pressed, this, &AHacknSlacksPlayer::OnJump);
	InputComponent->BindAction("Sprint", IE_Pressed, this, &AHacknSlacksPlayer::OnSprint);
	InputComponent->BindAction("Sprint", IE_Released, this, &AHacknSlacksPlayer::OnEndSprint);
	InputComponent->BindAction("Shoot", IE_Pressed, this, &AHacknSlacksPlayer::OnShoot);
	InputComponent->BindAction("Aim", IE_Pressed, this, &AHacknSlacksPlayer::OnAim);
	InputComponent->BindAction("Ability", IE_Pressed, this, &AHacknSlacksPlayer::OnAbility);
	InputComponent->BindAction("CharacterOverview", IE_Pressed, this, &AHacknSlacksPlayer::OnCharacterOverview);
	InputComponent->BindAction("Pause", IE_Pressed, this, &AHacknSlacksPlayer::OnPause).bExecuteWhenPaused = true;
}

// input callbacks

void AHacknSlacksPlayer::OnDodge()
{
	// change to can dodge function
	if (!poCurrentAttack && bOnGround && !bDodging && iDodgeCount < iMaxDodgeCount)
	{
		// stop attack
		ResetCombo();

		if (InputComponent)
		{
			// set dodge direction
			oDodgeDir = FVector(InputComponent->GetAxisValue("MoveForward"), InputComponent->GetAxisValue("MoveRight"), 0.0f);

			if (oDodgeDir.IsZero())
				oDodgeDir = GetActorForwardVector();
			else
				oDodgeDir = FRotator(0.0f, FollowCamera->GetComponentRotation().Yaw, 0.0f).RotateVector(oDodgeDir);
		}

		if (!bDodging)
		{
			// save ground friction
			if (UCharacterMovementComponent* pkCharMovement = GetCharacterMovement())
			{
				fSavedFriction = pkCharMovement->GroundFriction;
				pkCharMovement->GroundFriction = 0.0f;
			}
		}

		bDodging = true;

		// full body animation
		if (bIsLockedOn)
		{
			float TempAxisValue = InputComponent->GetAxisValue("MoveRight");
			if (TempAxisValue >= 0.2f)
				oDodgeDir = GetActorRightVector();
			else if (TempAxisValue <= -0.2f)
				oDodgeDir = -GetActorRightVector();
		}
		
		// turn player towards dodge direction
		if (pkCharAnim)
			PlayAnimToAngle(pkCharAnim, EBodyPoses::FullBody, GetDodgeAnim(), 1.0f, false, GetActorRotation().Yaw, FMath::RadiansToDegrees(FMath::Atan2(oDodgeDir.Y, oDodgeDir.X)));

		iDodgeCount++;

		fDodgeLockTimer = 0.0f;
	}
}

/*void AHacknSlacksPlayer::OnSwapWeapon()
{
	// player has no other weapon to switch to
	//if (iCurrentWeapons <= 1)
	//	return;

	int32 iSheathCount = (int32)ESheaths::Count;

	int32 iCurSheath = pkWeapon ? (int32)pkWeapon->eSheath + 1 : 0;
	int32 iSheathIndex = iCurSheath;

	for (int32 iSheath = 0; iSheath < iSheathCount; iSheath++, iSheathIndex++)
	{
		if (iSheathIndex >= iSheathCount)
			iSheathIndex = 0;

		if (aoSheaths[iSheathIndex].pkWeapon)
		{
			AnimSetWeapon((ESheaths)iSheathIndex);

			return;
		}
	}

	// should the player be able to have no weapon equipped
	SetWeapon(nullptr);
}*/

void AHacknSlacksPlayer::OnInteract()
{
	// if a chest that can be opened is nearby, open the chest, otherwise pick up the closest item if one is available
	if (AChest* pkChest = GetClosestOpenableChest())
		pkChest->Open();
	else if (pkClosestItem)
	{
		ResetCombo();
		pkClosestItem->PickUp(this);
	}
}

// this is when the player pressed the jump key, even if they don't actually jump
void AHacknSlacksPlayer::OnJump()
{

}

void AHacknSlacksPlayer::OnSprint()
{
	if (bOnGround)
		bSprinting = true;
}

void AHacknSlacksPlayer::OnEndSprint()
{
	bSprinting = false;
}

void AHacknSlacksPlayer::OnShoot()
{

}

void AHacknSlacksPlayer::OnAim()
{

}

void AHacknSlacksPlayer::OnAbility()
{
	pkCharAnim->SetAnim(EBodyPoses::FullBody, apkAbilities[0]->pkChargingAnim, 1.0f, false);
}

void AHacknSlacksPlayer::OnCharacterOverview()
{

}

void AHacknSlacksPlayer::OnPause()
{
	if (CanPause())
		AHackNSlacksGameMode::TogglePaused();
}

// if the game can be paused
bool AHacknSlacksPlayer::CanPause()
{
	return true;
}

// when the player finishes dodging
void AHacknSlacksPlayer::EndDodge()
{
	GetCharacterMovement()->GroundFriction = fSavedFriction;
	bDodging = false;
}

void AHacknSlacksPlayer::Jump()
{
	if (CanJump())
	{
		Super::Jump();

		iJumpCount++;
	}
}

// when the player touches the ground after being airborne
void AHacknSlacksPlayer::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	iJumpCount = 0;
}

// if the player can jump
bool AHacknSlacksPlayer::CanJumpInternal_Implementation() const
{
	UAttackDictionary* pkDict = pkWeapon ? pkWeapon->GetAttackDictionary() : pkAttackDict;

	return (!poCurrentAttack || poCurrentAttack->bAllowJump) && (!bOnGround && iJumpCount < iMaxJumpCount || Super::CanJumpInternal_Implementation());
}

// move as part of an attack
void AHacknSlacksPlayer::AttackMove(FAttackEntry* poAttackEntry)
{
	Super::AttackMove(poAttackEntry);

	if (pkSoftLockedTarget && poAttackEntry->bRangeScalesMovement)
	{
		if (UCharacterMovementComponent* pkCharMove = GetCharacterMovement())
		{
			float fRangeToTarget = (pkSoftLockedTarget->GetActorLocation() - GetActorLocation()).Size();

			float fVelocityMag = pkCharMove->Velocity.Size();

			pkCharMove->Velocity *= FMath::Min(1.0f, fRangeToTarget / fVelocityMag);
		}
	}
}

void AHacknSlacksPlayer::UpdateCharge(FAttackEntry* poAttackEntry, UCharacterAnimInstance* pkCharAnim)
{
	FVector oInputDir = FVector(InputComponent->GetAxisValue("MoveForward"), InputComponent->GetAxisValue("MoveRight"), 0.0f);

	FVector oTargetDir = oInputDir.IsZero() ? GetActorForwardVector() : FRotator(0.0f, FollowCamera->GetComponentRotation().Yaw, 0.0f).RotateVector(oInputDir);

	pkCharAnim->oTargetRot = FRotator(0.0f, GetAttackAngle(oTargetDir), 0.0f);
}

void AHacknSlacksPlayer::OnEndCharge(FAttackEntry* poAttackEntry, UCharacterAnimInstance* pkCharAnim)
{

}

/*void AHacknSlacksPlayer::OnPerformAttack(FAttackEntry* poAttackEntry, UCharacterAnimInstance* pkCharAnim, float fPlayRate)
{
	// current input direction
	FVector oInputDir = FVector(InputComponent->GetAxisValue("MoveForward"), InputComponent->GetAxisValue("MoveRight"), 0.0f);

	FVector oTargetDir = oInputDir.IsZero() ? GetActorForwardVector() : FRotator(0.0f, FollowCamera->GetComponentRotation().Yaw, 0.0f).RotateVector(oInputDir);
	
	// if a charging attack, use last stored input direction during charge, otherwise use current input direction (may be zero)
	PlayAnimToAngle(pkCharAnim, poAttackEntry->eBodyPose, poAttackEntry->pkAttackAnim, fPlayRate, true, GetActorRotation().Yaw, GetAttackAngle(oTargetDir));
	
	//Add anim trails
	//if (pkWeapon->pkTrails)
	//pkWeapon->pkTrails->BeginTrails(*socketUpper, *socketLower, ETrailWidthMode_FromCentre, 1);
}*/

// play an attack animation blended with a turning animation
// attack to perform, animation instance, player's current facing angle, angle to attack towards
/*void AHacknSlacksPlayer::PlayAnimToAngle(UCharacterAnimInstance* pkCharAnim, EBodyPoses eBodyPose, UAnimSequenceBase* pkAnim, float fPlayRate, bool bIsAttack, float fCurAngle, float fTargetAngle)
{
	// set target angle for animation
	pkCharAnim->oStartRot = FRotator(0.0f, fCurAngle, 0.0f);
	pkCharAnim->oTargetRot = FRotator(0.0f, fTargetAngle, 0.0f);
	pkCharAnim->bHasTargetAngle = true;

	pkCharAnim->SetAnim(eBodyPose, pkAnim, fPlayRate, bIsAttack);

	if (bIsAttack)
	{
		// choose blending animation based on angle difference
		float fAngleDiff = FMath::Abs(fCurAngle - fTargetAngle);

		// set blend based on angle difference - maybe use generic blend animations for every attack?
		//if (fAngleDiff < 45.0f)			// front
		//	pkCharAnim->SetAttackBlend(nullptr);
		//else if (fAngleDiff < 135.0f)	// side
		//	pkCharAnim->SetAttackBlend(poAttackEntry->pkSideAnim);
		//else							// back
		//	pkCharAnim->SetAttackBlend(poAttackEntry->pkBackAnim);
	}
}*/

// get angle to attack - towards soft lock target or input direction or straight forward
float AHacknSlacksPlayer::GetAttackAngle(FVector oTargetDir, bool bSoftLock)
{
	if (oTargetDir.IsZero())
		oTargetDir = FollowCamera->GetForwardVector();

	if (bSoftLock)
	{
		GetClosestAngleEnemy(oTargetDir);

		pkSoftLockedTarget = nullptr;

		//  player has a nearby enemy
		if (pkClosestAngleTarget)
		{
			// get angle from player to enemy
			FVector oFaceDir = pkClosestAngleTarget->GetActorLocation() - GetActorLocation();

			float fAttackDir = FMath::Atan2(oFaceDir.Y, oFaceDir.X);

			// difference in input and soft lock angles
			float fAngleDiff = FMath::Atan2(oTargetDir.Y, oTargetDir.X) - fAttackDir;

			// prevents angles close to -PI and PI from being regarded as far away from each other
			if (fAngleDiff < -PI)
				fAngleDiff += PI * 2;
			else if (fAngleDiff > PI)
				fAngleDiff -= PI * 2;

			// nearby enemy can be soft locked
			if (FMath::RadiansToDegrees(FMath::Abs(fAngleDiff)) <= fMaxDirectionalDeviation)
			{
				pkSoftLockedTarget = pkClosestAngleTarget;
				return FMath::RadiansToDegrees(fAttackDir);
			}
		}
	}

	return FMath::RadiansToDegrees(FMath::Atan2(oTargetDir.Y, oTargetDir.X));
}

// get enemy whose angle from the player is closest to the attack direction
void AHacknSlacksPlayer::GetClosestAngleEnemy(FVector oTargetDir)
{
	pkClosestAngleTarget = nullptr;

	auto pkEnemyIter = apkNearbyEnemies.GetHead();

	float fMinAngleDiff = 0.0f;

	// most similar angle from the player to the enemy to this angle becomes the soft locked target
	float fTargetAngle = FMath::Atan2(oTargetDir.Y, oTargetDir.X);

	// check each nearby enemy
	while (pkEnemyIter != nullptr)
	{
		AEnemy* pkEnemy = pkEnemyIter->GetValue();

		// enemy does not exist or enemy is not alive
		if (!pkEnemy || pkEnemy->fHealth <= 0.0f)
		{
			auto pkNextEnemy = pkEnemyIter->GetNextNode();
			apkNearbyEnemies.RemoveNode(pkEnemyIter);
			pkEnemyIter = pkNextEnemy;

			continue;
		}

		// direction from player to enemy
		FVector oEnemyDir = pkEnemy->GetActorLocation() - GetActorLocation();

		// difference in angle from player to enemy, to the target angle
		float fAngleDiff = FMath::Atan2(oEnemyDir.Y, oEnemyDir.X) - fTargetAngle;

		// prevents angles close to -PI and PI from being regarded as far away from each other
		if (fAngleDiff < -PI)
			fAngleDiff += PI * 2;
		else if (fAngleDiff > PI)
			fAngleDiff -= PI * 2;

		// need a positive value for comparison - closest to zero, sign doesn't matter
		fAngleDiff = FMath::Abs(fAngleDiff);

		// if first target checked, or target is closer to input angle from player
		if (pkClosestAngleTarget == nullptr || fAngleDiff < fMinAngleDiff)
		{
			pkClosestAngleTarget = pkEnemy;
			fMinAngleDiff = fAngleDiff;
		}

		pkEnemyIter = pkEnemyIter->GetNextNode();
	}
}

// get difference in angle to the enemy from the player, compared to the attack direction
float AHacknSlacksPlayer::GetAngleDiffToSoftLocked(FVector oTargetDir)
{
	if (!pkSoftLockedTarget)
		return PI * 2;

	// get angle from player to enemy
	FVector oFaceDir = pkSoftLockedTarget->GetActorLocation() - GetActorLocation();

	float fAttackDir = FMath::Atan2(oFaceDir.Y, oFaceDir.X);

	// difference in input and soft lock angles
	float fAngleDiff = FMath::Atan2(oTargetDir.Y, oTargetDir.X) - fAttackDir;

	// prevents angles close to -PI and PI from being regarded as far away from each other
	if (fAngleDiff < -PI)
		fAngleDiff += PI * 2;
	else if (fAngleDiff > PI)
		fAngleDiff -= PI * 2;

	return fAngleDiff;
}

// get dodge animation based on input and facing directions
UAnimSequenceBase* AHacknSlacksPlayer::GetDodgeAnim()
{
	//if (pkHardLockTarget)
	//else
	return pkFrontDodge;
}

/*void AHacknSlacksPlayer::UpdateBuffs()
{
	float fDelta = GetWorld()->GetDeltaSeconds();

	// active buffs get moved to the end
	for (int32 iBuff = 0; iBuff < aoBuffs.Num(); iBuff++)
	{
		FBuff* poBuff = &aoBuffs[iBuff];

		// only update active buffs
		if (poBuff->bActive)
		{
			if (!poBuff->Update(fDelta))
			{
				iActiveBuffs--;

				OnRemoveBuff(*poBuff, iBuff);
			}
		}
	}
}*/

/*void AHacknSlacksPlayer::ShiftBuffs(int32 iBuffIndex)
{
	for (int32 iBuff = 0; iBuff < aoBuffs.Num(); iBuff++)
	{
		FBuff* poBuff = &aoBuffs[iBuff];

		// only update active buffs
		if (poBuff->bActive && poBuff->iIndex > iBuffIndex)
			poBuff->ShiftWidget();
	}
}*/

APlayerController* AHacknSlacksPlayer::GetPlayerController()
{
	return Cast<APlayerController>(Controller);
}

// update shader parameters
/*void AHacknSlacksPlayer::UpdateFX()
{
	float fHealthFactor = fHealth / fMaxHealth;

	UHNSGameInstance::SetShaderValue("Saturation", fHealthFactor);
	UHNSGameInstance::SetShaderValue("VignetteIntensity", 1.0f - fHealthFactor);
}*/

void AHacknSlacksPlayer::PitchAutoAdjustment(float DeltaTime)
{
	if (InputComponent)
	{
		if (InputComponent->GetAxisValue("MoveForward") > 0.025f || InputComponent->GetAxisValue("MoveForward") < -0.025f)
			bMoving = true;
		else
		{
			bMoving = false;
			fPitchAutoAdjustTimer = 0.f;
			bDisablePitchAdjust = false;
			bPitchAdjust = false;
		}

		if (bMoving == true)
		{

			fPitchAutoAdjustTimer += DeltaTime;

			if (fPitchAutoAdjustTimer > 2.0f)
				bPitchAdjust = true;
		}

		if (bPitchAdjust == true && bDisablePitchAdjust == false && bMoving == true)
		{
			rTargetPitchRotation = GetControlRotation();
			rTargetPitchRotation.Pitch = 340;
			rCurrentPitchRotation = GetControlRotation();

			if (rCurrentPitchRotation.Pitch < 91.f || rCurrentPitchRotation.Pitch > 370.f)
				Controller->SetControlRotation(FMath::RInterpTo(GetControlRotation(), rTargetPitchRotation, DeltaTime, 2.0f));

			else if (rCurrentPitchRotation.Pitch > 269.f && rCurrentPitchRotation.Pitch < 340.f)
				Controller->SetControlRotation(FMath::RInterpTo(GetControlRotation(), rTargetPitchRotation, DeltaTime, 2.0f));

			else
				bDisablePitchAdjust = true;
		}
	}
}