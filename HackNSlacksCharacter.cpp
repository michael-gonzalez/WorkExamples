// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "HacknSlacks.h"
#include "Runtime/Engine/Classes/PhysicsEngine/PhysicsAsset.h"
#include "BuffDef.h"
#include "AttackDictionary.h"
#include "WeaponFactory.h"
#include "Weapon.h"
#include "WeaponSpawn.h"
#include "Chest.h"
#include "CharacterAnimInstance.h"
#include "Ability.h"
#include "AttackEntry.h"
#include "HackNSlacksCharacter.h"

//////////////////////////////////////////////////////////////////////////
// AHnS_4_6Character

AHackNSlacksCharacter::AHackNSlacksCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// FollowCamera Default Values
	fCameraRotRate = 0.6f;
	fAngleInfluence = 5.0f;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	bOnGround = true;

	eTeam = ETeams::Enemy;

	for (int32 iSocket = 0; iSocket < (int32)EBodyParts::Count; iSocket++)
		apoSockets[iSocket].eBodyPart = (EBodyParts)iSocket;

	fHealth = fMaxHealth;
	fDamageMultiplier = 1.0f;

	TScriptDelegate<> oOnDestroy;
	oOnDestroy.BindUFunction(this, "OnDestroy");

	OnDestroyed.AddUnique(oOnDestroy);

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

FAttackEntry* const AHackNSlacksCharacter::GetCurrentAttack()
{
	return poCurrentAttack;
}

AWeapon* AHackNSlacksCharacter::GetWeapon()
{
	return pkWeapon;
}

const USkeletalMeshSocket* AHackNSlacksCharacter::GetBoneSocket(EBodyParts eBodyPart)
{
	return apoSockets[(uint32)eBodyPart].pkSocket;
}

UAttackCollider* AHackNSlacksCharacter::GetCollider(EBodyParts eBodyPart)
{
	return apkAttackColliders[(int32)eBodyPart];
}

void AHackNSlacksCharacter::SetCollider(EBodyParts eBodyPart, UAttackCollider* pkCollider)
{
	apkAttackColliders[(int32)eBodyPart] = pkCollider;
}

/* FBuff& AHackNSlacksCharacter::AddBuffDefault(TSubclassOf<UBuffDef> pkBuffClass)
{
	UBuffDef* pkBuffDef = (UBuffDef*)pkBuffClass->GetDefaultObject();

	return AddBuff(pkBuffClass, pkBuffDef->fBaseIntensity, pkBuffDef->fBaseDuration, pkBuffDef->iBaseTickCount);
} */

/* FBuff& AHackNSlacksCharacter::AddBuff(TSubclassOf<UBuffDef> pkBuffClass, float fIntensity, float fDuration, int32 iTickCount)
{
	UBuffDef* pkBuffDef = (UBuffDef*)pkBuffClass->GetDefaultObject();

	FBuff* poBuff = nullptr;

	// buffs get moved to the front of the list when they expire
	for (int32 iBuff = 0; iBuff < aoBuffs.Num(); iBuff++)
	{
		if (FBuff* poCurBuff = &aoBuffs[iBuff])
		{
			// character already has a buff of this type, reset the buff
			if (poCurBuff->pkBuffDef == pkBuffDef)
			{
				poCurBuff->ResetBuff(fIntensity, fDuration, iTickCount);

				return *poCurBuff;
			}
			// buff is not being used, save for later
			else if (poBuff == nullptr && !poCurBuff->bActive)
				poBuff = poCurBuff;
		}
	}

	// no inactive buff was found, create a new one
	if (!poBuff)
	{
		aoBuffs.Add(FBuff(this));

		poBuff = &aoBuffs[aoBuffs.Num() - 1];
	}

	poBuff->Init(pkBuffDef, fIntensity, fDuration, iTickCount);

	return *poBuff;
} */

void AHackNSlacksCharacter::AddNearbyItem(AItem* pkNearbyItem)
{
	if (!apkNearbyItems.FindNode(pkNearbyItem))
		apkNearbyItems.AddTail(pkNearbyItem);
}

void AHackNSlacksCharacter::RemoveNearbyItem(AItem* pkNearbyItem)
{
	apkNearbyItems.RemoveNode(pkNearbyItem);

	if (pkNearbyItem == pkClosestItem)
		GetClosestItem();
}

void AHackNSlacksCharacter::AddNearbyChest(AChest* pkNearbyChest)
{
	if (!apkNearbyChests.FindNode(pkNearbyChest))
		apkNearbyChests.AddTail(pkNearbyChest);
}

void AHackNSlacksCharacter::RemoveNearbyChest(AChest* pkNearbyChest)
{
	apkNearbyChests.RemoveNode(pkNearbyChest);
}

/*/ called by a weapon when it is picked up
// do not call this, use Weapon->PickUp(this) instead
// returns if the weapon can be picked up
bool AHackNSlacksCharacter::SetWeapon(AWeapon* pkWeap)
{
	if (pkWeapon)
		pkWeapon->Drop();

	pkWeapon = pkWeap;

	return true;
} */

float AHackNSlacksCharacter::SetHealth(float fNewHealth)
{
	fHealth = FMath::Clamp(fNewHealth, 0.0f, fMaxHealth);

	if (fHealth <= 0.0f)
		OnDeath();

	return fHealth;
}

float AHackNSlacksCharacter::ModHealth(float fMod)
{
	fHealth = FMath::Clamp(fHealth + fMod, 0.0f, fMaxHealth);

	if (fHealth <= 0.0f)
		OnDeath();

	return fHealth;
}

void AHackNSlacksCharacter::SetDodging(bool bIsDodging)
{
	bDodging = bIsDodging;
}

/*void AHackNSlacksCharacter::OnWalkingOffLedge_Implementation(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta)
{
	if (!poCurrentAttack && !bDodging)
		Super::OnWalkingOffLedge_Implementation(PreviousFloorImpactNormal, PreviousFloorContactNormal, PreviousLocation, TimeDelta);
} */

void AHackNSlacksCharacter::AddHit(int32 iBodyIndex, float fDuration, FVector oHitFromLoc, float fStrikeForce)
{
	USkeletalMeshComponent* pkSkeleton = GetMesh();

	UPhysicsAsset* pkPhys = pkSkeleton->GetPhysicsAsset();

	FName sBoneName;

	for (auto& kBone : pkPhys->BodySetupIndexMap)
	{
		if (kBone.Value == iBodyIndex)
		{
			sBoneName = kBone.Key;
			break;
		}
	}

	// *** NEEDS TO BE FIXED ***  */
	// if body is not bound to a bone (should always be) or if the bone is the root bone
	if (sBoneName.IsNone() /* || sBoneName == pkSkeleton->GetBodySetup()->BoneName */)
		return;

	//pkSkeleton->SetAllBodiesBelowSimulatePhysics(sBoneName, true);

	//pkSkeleton->AddImpulse((pkSkeleton->GetBoneLocation(sBoneName) - oHitFromLoc).GetSafeNormal() * fStrikeForce, sBoneName, true);

	//AddSimulatingBody(sBoneName, fDuration);
}

/*void AHackNSlacksCharacter::AddSimulatingBody(const FName& sBoneName, float fDuration)
{
	if (auto pkHeadBody = aoSimulatingBodies.GetHead())
	{
		if (FSimulatingBody* poBody = &pkHeadBody->GetValue())
		{
			if (!poBody->bActive)
			{
				poBody->Init(sBoneName, fDuration);

				aoSimulatingBodies.AddTail(*poBody);
				aoSimulatingBodies.RemoveNode(pkHeadBody);

				return;
			}
		}
	}

	aoSimulatingBodies.AddTail(FSimulatingBody(sBoneName, fDuration));
} */

void AHackNSlacksCharacter::TurnAtRate(float Rate)
{
	GetCharacterMovement()->bOrientRotationToMovement = true;

	float fTurnRate = Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds();

	if (poCurrentAttack)
		fTurnRate *= poCurrentAttack->fTurnControlFactor;

	// calculate delta for this frame from the rate information
	AddControllerYawInput(fTurnRate);
}

void AHackNSlacksCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AHackNSlacksCharacter::MoveForward(float Value)
{
	float fMoveControlFactor = 1.0f;
	
	if (bDodging)
		fMoveControlFactor = fDodgeMoveControlFactor;
	else if (iDodgeCount > 0 && fDodgeLockTimer < fDodgeLockDuration)
		fMoveControlFactor = 0.0f;
	else if (poCurrentAttack)
		fMoveControlFactor = poCurrentAttack->fMoveControlFactor;

	if ((Controller != NULL) && (Value != 0.0f) && fMoveControlFactor != 0.0f)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value * fMoveControlFactor);
	}
}

void AHackNSlacksCharacter::Strafe(float Value)
{
	float fMoveControlFactor = 1.0f;

	if (bDodging)
		fMoveControlFactor = fDodgeMoveControlFactor;
	else if (iDodgeCount > 0 && fDodgeLockTimer < fDodgeLockDuration)
		fMoveControlFactor = 0.0f;
	else if (poCurrentAttack)
		fMoveControlFactor = poCurrentAttack->fMoveControlFactor;

	if ((Controller != NULL) && (Value != 0.0f) && fMoveControlFactor != 0.0f)
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value * fMoveControlFactor);
	}
}

void AHackNSlacksCharacter::UpdateSimulatingBodies()
{
	float fDelta = GetWorld()->GetDeltaSeconds();

	// active bodies get moved to the end
	auto pkBodyIter = aoSimulatingBodies.GetTail();

	while (pkBodyIter != nullptr)
	{
		FSimulatingBody* poBody = &pkBodyIter->GetValue();

		// only update active bodies, inactive bodies get moved to the front
		if (poBody->bActive)
		{
			// move body to front if it has become inactive
			if (!poBody->Update(fDelta, GetMesh()))
			{
				auto PrevBody = pkBodyIter->GetPrevNode();

				// move body to front of list
				aoSimulatingBodies.AddHead(*poBody);
				aoSimulatingBodies.RemoveNode(pkBodyIter);

				pkBodyIter = PrevBody;
			}
			else
				pkBodyIter = pkBodyIter->GetPrevNode();
		}
		else
			break;
	}
}

/* void AHackNSlacksCharacter::UpdateBuffs()
{
	float fDelta = GetWorld()->GetDeltaSeconds();

	// active buffs get moved to the end
	for (int32 iBuff = 0; iBuff < aoBuffs.Num(); iBuff++)
	{
		FBuff* poBuff = &aoBuffs[iBuff];

		// only update active buffs
		if (poBuff->bActive)
			poBuff->Update(fDelta);
	}
} */

void AHackNSlacksCharacter::OnDeath()
{
	for (int32 iBuff = 0; iBuff < aoBuffs.Num(); iBuff++)
	{
		FBuff* poBuff = &aoBuffs[iBuff];

		poBuff->bActive = false;
		poBuff->fTimer = poBuff->fDuration;
	}
}

void AHackNSlacksCharacter::OnDestroy()
{
	OnDeath();
}

void AHackNSlacksCharacter::BeginPlay()
{
	Super::BeginPlay();

	pkCharAnim = Cast<UCharacterAnimInstance>(GetMesh()->GetAnimInstance());

	pkAttackDict = UAttackDictionary::GetManager(pkAttackDictClass);

	USkeletalMeshComponent* pkSkeleton = GetMesh();

	for (int32 iSocket = 0; iSocket < (int32)EBodyParts::Count; iSocket++)
		apoSockets[iSocket].Init(pkSkeleton);

	// get body part colliders
	GetComponents<UAttackCollider>(apkAttackColliders);

	// fill in missing parts
	while (apkAttackColliders.Num() < (int32)EBodyParts::Count)
		apkAttackColliders.Add(nullptr);

	// sort colliders to match the enum
	for (int32 iCollider = 0; iCollider < apkAttackColliders.Num(); iCollider++)
	{
		UAttackCollider* pkTemp = apkAttackColliders[iCollider];

		if (!pkTemp)
			continue;

		int32 iBodyPart = (int32)pkTemp->eBodyPart;

		apkAttackColliders[iCollider] = apkAttackColliders[iBodyPart];
		apkAttackColliders[iBodyPart] = pkTemp;
	}

	// create starting weapon if assigned
	if (oSpawnWithWeapon.eWeaponType != EWeaponTypes::Count)
		if (AWeapon* pkWeap = oSpawnWithWeapon.SpawnWeapon())
			pkWeap->PickUp(this);
	
	//if (pkSkeleton && pkCharAnim)
	//	pkCharAnim->oBaseHeadRot = pkSkeleton->GetSocketTransform("head").Rotator();

	fHealth = fMaxHealth;
}

// character update
void AHackNSlacksCharacter::TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
	
	UpdateSimulatingBodies();

	UpdateBuffs();

	CameraFollow(DeltaTime);

	// character is attacking
	if (poCurrentAttack)
	{
		// attack is a chargable attack
		if (bCharging)
		{
			fChargeTimer += DeltaTime;

			UpdateCharge(poCurrentAttack, pkCharAnim);

			// attack has reached max charge
			if (fChargeTimer >= poCurrentAttack->fMaxCharge)
				EndCharge();
		}
		else if (fComboTimer > poCurrentAttack->fEndComboWait)
			ResetCombo();
		else
		{
			fComboTimer += DeltaTime;

			AttackMove(poCurrentAttack);
		}
	}

	if (!bDodging && iDodgeCount > 0)
	{
		fDodgeLockTimer += DeltaTime;
		
		if (fDodgeLockTimer >= fDodgeLockDuration)
		{
			iDodgeCount = 0;
			fDodgeLockTimer = 0.0f;
		}
	}

	if (!bDodging && !poCurrentAttack && pkCharAnim && pkCharAnim->bHasTargetAngle)
		pkCharAnim->bHasTargetAngle = false;

	GetClosestItem();
}

/*void AHackNSlacksCharacter::Jump()
{
	Super::Jump();

	bOnGround = false;
}*/

/*void AHackNSlacksCharacter::Falling()
{
	Super::Falling();

	bOnGround = false;
}*/

/*void AHackNSlacksCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	bOnGround = true;
}*/

/*void AHackNSlacksCharacter::OnLightAttack()
{
	PerformAttack(EPlayerInputs::LightAttack);
} */

/*void AHackNSlacksCharacter::OnHeavyAttack()
{
	PerformAttack(EPlayerInputs::HeavyAttack);
}*/

void AHackNSlacksCharacter::AttackMove(FAttackEntry* poAttackEntry)
{
	FRotator oActorRotation = GetActorRotation();

	FVector oLocalVelocity = oActorRotation.UnrotateVector(GetCharacterMovement()->Velocity);

	GetCharacterMovement()->Velocity = oActorRotation.RotateVector(pkCharAnim->GetAnimVelocity(oLocalVelocity));
}

void AHackNSlacksCharacter::UpdateCharge(FAttackEntry* poAttackEntry, UCharacterAnimInstance* pkCharAnim)
{
	
}

void AHackNSlacksCharacter::OnEndCharge(FAttackEntry* poAttackEntry, UCharacterAnimInstance* pkCharAnim)
{

}

void AHackNSlacksCharacter::EndCharge()
{
	if (bCharging)
	{
		bCharging = false;

		pkWeapon->fCharge = fChargeTimer;

		if (poCurrentAttack)
		{
			if (pkCharAnim)
			{
				// set timer to match the position of the animation
				fComboTimer = fChargeTimer * pkCharAnim->Montage_GetPlayRate(pkCharAnim->pkCurrentAttackAnim);

				pkCharAnim->Montage_SetPlayRate(pkCharAnim->pkCurrentAttackAnim, poCurrentAttack->fPlayRate);

				OnEndCharge(poCurrentAttack, pkCharAnim);
			}

			poCurrentAttack->OnAttack(this, fChargeTimer);
		}

		fChargeTimer = 0.0f;
	}
}

/*void AHackNSlacksCharacter::OnPerformAttack(FAttackEntry* poAttackEntry, UCharacterAnimInstance* pkCharAnim, float fPlayRate)
{
	pkCharAnim->SetAnim(poAttackEntry->eBodyPose, poAttackEntry->pkAttackAnim, fPlayRate, true);
}*/

/*void AHackNSlacksCharacter::PerformAttack(EPlayerInputs eInput)
{
	if (!bDodging && pkWeapon && pkWeapon->GetAttackDictionary()->DoAttack(poCurrentAttack, eInput, fComboTimer, bSprinting, !bOnGround))
	{
		DoAttack(poCurrentAttack);
		iCurrentAttack = poCurrentAttack->iAIAppropsResponse;
	}
}*/

void AHackNSlacksCharacter::DoAttack(FAttackEntry* poAttackEntry)
{
	if (poAttackEntry)
	{
		fComboTimer = 0.0f;

		poCurrentAttack = poAttackEntry;

		pkWeapon->fCharge = 0.0f;

		for (TArray<UAttackCollider*>::TIterator pkIter = apkAttackColliders.CreateIterator(); pkIter; ++pkIter)
			if ((*pkIter))
				(*pkIter)->SetColliderActive(false);

		bCharging = poAttackEntry->fMaxCharge > 0.0f;

		poAttackEntry->OnAttack(this);

		if (!bCharging)
			AttackMove(poAttackEntry);

		if (pkCharAnim)
		{
			// need to set animation speed for charged attacks
			if (bCharging)
				OnPerformAttack(poAttackEntry, pkCharAnim, poAttackEntry->fPlayRate * 0.1f / poAttackEntry->fMaxCharge);
			else
				OnPerformAttack(poAttackEntry, pkCharAnim, poAttackEntry->fPlayRate);
		}
	}
}

void AHackNSlacksCharacter::BeginOverlap(class AActor* pkOtherActor)
{

}

void AHackNSlacksCharacter::EndOverlap(class AActor* pkOtherActor)
{

}

void AHackNSlacksCharacter::GetClosestItem()
{
	pkClosestItem = nullptr;

	if (apkNearbyItems.Num() == 0)
		return;

	auto pkItemIter = apkNearbyItems.GetHead();

	pkClosestItem = ((pkItemIter->GetValue()!= NULL)?pkItemIter->GetValue():nullptr);

	if (pkClosestItem == nullptr || !pkClosestItem->IsValidLowLevel()) return;

	float fClosestDistSQ = FVector::DistSquared(pkClosestItem->GetActorLocation(), GetActorLocation());

	while ((pkItemIter = pkItemIter->GetNextNode()) != nullptr)
	{
		AItem* pkItem = pkItemIter->GetValue();

		float fDistSQ = FVector::DistSquared(pkItem->GetActorLocation(), GetActorLocation());

		if (fDistSQ < fClosestDistSQ)
		{
			pkClosestItem = pkItem;
			fClosestDistSQ = fDistSQ;
		}
	}
}

/*AChest* AHackNSlacksCharacter::GetClosestOpenableChest()
{
	if (apkNearbyChests.Num() == 0)
		return nullptr;

	AChest* pkClosestChest = nullptr;

	auto pkChestIter = apkNearbyChests.GetHead();

	float fClosestDistSQ = 0.0f;

	while (pkChestIter != nullptr)
	{
		AChest* pkChest = pkChestIter->GetValue();

		if (pkChest->CanOpen())
		{
			float fDistSQ = FVector::DistSquared(pkChest->GetActorLocation(), GetActorLocation());

			if (pkClosestChest == nullptr || fDistSQ < fClosestDistSQ)
			{
				pkClosestChest = pkChest;
				fClosestDistSQ = fDistSQ;
			}
		}

		pkChestIter = pkChestIter->GetNextNode();
	}

	return pkClosestChest;
}*/

void AHackNSlacksCharacter::ResetCombo()
{
	GetCharacterMovement()->MovementMode = EMovementMode::MOVE_Walking;

	fComboTimer = 0.0f;
	fChargeTimer = 0.0f;

	bCharging = false;

	poCurrentAttack = nullptr;

	if (pkCharAnim)
	{
		pkCharAnim->StopAttackAnim();
		pkCharAnim->oAnimVelocity = FAnimVelocity();
	}

	for (TArray<UAttackCollider*>::TIterator pkIter = apkAttackColliders.CreateIterator(); pkIter; ++pkIter)
		if ((*pkIter))
			(*pkIter)->SetColliderActive(false);
}

void AHackNSlacksCharacter::CameraFollow(float DeltaTime)
{
	if (bIsLockedOn == false && bDisableAutoCameraFollow == false)
	{
		FRotator PlayerRotator = GetControlRotation();
		FRotator TempYawRotator;

		// Getting Yaw as that is the only axis to rotate
		TempYawRotator.Yaw = PlayerRotator.Yaw; TempYawRotator.Pitch = 0; TempYawRotator.Roll = 0;

		FVector ForwardVector = FRotationMatrix(TempYawRotator).GetUnitAxis(EAxis::X) * GetInputAxisValue("MoveForward");
		FVector RightVector = FRotationMatrix(TempYawRotator).GetUnitAxis(EAxis::Y) * GetInputAxisValue("MoveRight");

		FVector MoveDirection = ForwardVector + RightVector;
		MoveDirection.Normalize();

		FRotator XVecRotator = MoveDirection.Rotation();
		GetControlRotation();

		/*DeltaRotator is MoveDirections Rotation - Controllers rotation then normalised*/
		FRotator DeltaRotator = XVecRotator - GetControlRotation();
		DeltaRotator.Normalize();

		float InputLength = FMath::Abs(GetInputAxisValue("MoveForward")) + FMath::Abs(GetInputAxisValue("MoveRight"));
		FMath::Clamp(InputLength, 0.f, 1.0f);


		FVector ControllerForwardVec = FRotationMatrix(TempYawRotator).GetUnitAxis(EAxis::X);

	

		float CameraMoveDotProd = FVector::DotProduct(ControllerForwardVec, MoveDirection);

		CameraMoveDotProd = 1 - FMath::Abs(CameraMoveDotProd);

		float FirstStep = InputLength * DeltaTime * FMath::Pow(CameraMoveDotProd, fAngleInfluence) * fCameraRotRate;
		FMath::Clamp(FirstStep, 0.f, 1.0f);
		AddControllerYawInput(FirstStep * DeltaRotator.Yaw);
	}


	
}

void AHackNSlacksCharacter::ResetCurrentAttackRef()
{
	iCurrentAttack = 0;

}