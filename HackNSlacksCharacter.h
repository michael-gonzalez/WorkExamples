// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Teams.h"
#include "PlayerInputs.h"
#include "EquipSlots.h"
#include "BodySocket.h"
#include "Buff.h"
#include "SimulatingBody.h"
#include "WeaponSpawn.h"
#include "GameFramework/Character.h"
#include "HackNSlacksCharacter.generated.h"

class UAttackCollider;
class UAttackReceiver;
class UBuffDef;
class AItem;
class AGear;
class UAttackDictionary;
class AChest;
class UCharacterAnimInstance;
struct FAttackEntry;
class UAbility;

UCLASS(config=Game)
class AHackNSlacksCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHackNSlacksCharacter(const FObjectInitializer& ObjectInitializer);

	// get the current combo element being performed
	FAttackEntry* const GetCurrentAttack();

	UFUNCTION(BlueprintCallable, Category = Weapon)
	AWeapon* GetWeapon();

	// get the bone or socket that represents a body part that gear can be attached to
	const USkeletalMeshSocket* GetBoneSocket(EBodyParts eBodyPart);

	UFUNCTION(BlueprintCallable, Category = Attack)
	UAttackCollider* GetCollider(EBodyParts eBodyPart);

	UFUNCTION(BlueprintCallable, Category = Attack)
	void SetCollider(EBodyParts eBodyPart, UAttackCollider* pkCollider);

	UFUNCTION(BlueprintCallable, Category = Buff)
	FBuff& AddBuffDefault(TSubclassOf<UBuffDef> pkBuffDef);

	UFUNCTION(BlueprintCallable, Category = Buff)
	virtual FBuff& AddBuff(TSubclassOf<UBuffDef> pkBuffDef, float fIntensity, float fDuration, int32 iTickCount);

	// tell the character that there is an item they can pick up
	void AddNearbyItem(AItem* pkItem);

	// tell the character that there is an item they can pick up
	void RemoveNearbyItem(AItem* pkItem);

	void AddNearbyChest(AChest* pkChest);

	void RemoveNearbyChest(AChest* pkChest);

	virtual bool SetWeapon(AWeapon* pkWeap);

	UFUNCTION(BlueprintCallable, Category = Character)
	virtual float SetHealth(float fNewHealth);

	UFUNCTION(BlueprintCallable, Category = Character)
	virtual float ModHealth(float fMod);

	UFUNCTION(BlueprintCallable, Category = Dodge)
	virtual void SetDodging(bool bIsDodging);

	virtual void OnWalkingOffLedge_Implementation(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta) override;

	// apply an impulse to a bone - physical animation over duration
	UFUNCTION(BlueprintCallable, Category = Body)
	void AddHit(int32 iBodyIndex, float fDuration, FVector oHitFromLoc, float fStrikeForce);

	void AddSimulatingBody(const FName& sBoneName, float fDuration);

	UFUNCTION(BlueprintCallable, Category = AI)
	void ResetCurrentAttackRef();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	int32 iCurrentAttack;

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	float BaseLookUpRate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Character)
	float fHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Character)
	float fMaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Character)
	FWeaponSpawnStruct oSpawnWithWeapon;

	// which team the character belongs to, player, enemy, environment
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Game)
	ETeams eTeam;

	// Camera Variables

	//FollowCam
	/* Float that changes when the camera starts to rotate to follow the players facing direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CameraAdjustments)
	float fAngleInfluence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CameraAdjustments)
	float fCameraRotRate;

	//LockCam
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CameraAdjustments)
	AActor* aClosestActorInView;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock-On")
	bool bIsLockedOn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock-On")
	bool bDisableAutoCameraFollow;

	UPROPERTY(EditAnywhere, Category = "Gear")
	AGear* apkEquippedGear[(int32)EEquipSlots::Count];

	UPROPERTY(EditAnywhere, Category = "Mesh")
	USkeletalMeshComponent* apkBodyMeshes[(int32)EBodyParts::Count];

protected:

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void Strafe(float Value);

	/**
	* Called via input to turn at a given rate.
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	void TurnAtRate(float Rate);

	/**
	* Called via input to turn look up/down at a given rate.
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	void LookUpAtRate(float Rate);

	void UpdateSimulatingBodies();

	virtual void UpdateBuffs();

	void OnDeath();

	UFUNCTION()
	void OnDestroy();

	virtual void BeginPlay() override;

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	virtual void Jump();

	virtual void Falling() override;

	virtual void Landed(const FHitResult& Hit) override;

	virtual void OnLightAttack();

	virtual void OnHeavyAttack();

	virtual void AttackMove(FAttackEntry* poAttackEntry);
		
	virtual void UpdateCharge(FAttackEntry* poAttackEntry, UCharacterAnimInstance* pkCharAnim);

	virtual void OnEndCharge(FAttackEntry* poAttackEntry, UCharacterAnimInstance* pkCharAnim);

	void EndCharge();

	// when an attack will be performed - overridden in player for directional attacks
	virtual void OnPerformAttack(FAttackEntry* poAttackEntry, UCharacterAnimInstance* pkCharAnim, float fPlayRate = 1.0f);

	// perform a light attack or heavy attack depending on the given input
	void PerformAttack(EPlayerInputs eInput);

	// perform the current selected ability
	void PerformAbility();

	// actual logic for PerformAttack and PerformAbility
	void DoAttack(FAttackEntry* poAttackEntry);

	UFUNCTION()
	virtual void BeginOverlap(class AActor* OtherActor);

	UFUNCTION()
	virtual void EndOverlap(class AActor* OtherActor);

	void GetClosestItem();

	AChest* GetClosestOpenableChest();

	// ATTACK

public:
	void ResetCombo();
	
	// multiplier for attack damage - increased by buffs
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attack)
	float fDamageMultiplier;

	// character's current weapon
	AWeapon* pkWeapon;

protected:
	// if the character is charging an attack
	bool bCharging;

	// charge attack timer
	float fChargeTimer;

	// combo input timer
	UPROPERTY(BlueprintReadWrite, Category = Attack)
	float fComboTimer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attack)
	float fAttackTurnRate;

	// current attack the character is performing
	FAttackEntry* poCurrentAttack;

	// UNUSED - using weapons references

	// character's reference to their attack dictionary
	UAttackDictionary* pkAttackDict;

	// attack dictionary blueprint - for attack information
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Attack)
	TSubclassOf<UAttackDictionary> pkAttackDictClass;

	//

	TArray<UAttackCollider*> apkAttackColliders;

	UPROPERTY(EditAnywhere, Category = Attack)
	FBodySocket apoSockets[(int32)EBodyParts::Count];

	//

	// the character is on the ground
	bool bOnGround;

	// the character is sprinting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Player)
	bool bSprinting;

	// DODGE

	// the character is dodging
	UPROPERTY(BlueprintReadOnly, Category = Dodge)
	bool bDodging;
	
	// direction the character is dodging
	FVector oDodgeDir;

	// how many times the character has dodged recently
	int32 iDodgeCount;

	// maximum number of times the character can dodge before they have to wait for the dodge lock to finish
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Dodge)
	int32 iMaxDodgeCount;

	// how long the character has been unable to move after dodging
	float fDodgeLockTimer;

	// how long the character will be unable to move after dodging
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Dodge)
	float fDodgeLockDuration;

	// how much the character can control their movement while dodging
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Dodge)
	float fDodgeMoveControlFactor;
	
	// the fast the character moves while dodging
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Dodge)
	float fDodgeSpeed;

	// rate the player can move - some attacks/abilities may slow or stop movement
	float fMovementControlFactor;

	// if the character can jump - some attacks/abilities may prevent jumping
	bool bAllowJump;

	// movement during an attack
	FVector oAttackVelocity;

	// list of items the character is close enough to pick up
	TDoubleLinkedList<AItem*> apkNearbyItems;
	
	TDoubleLinkedList<AChest*> apkNearbyChests;

	// the physics bodies of the character that have been hit recently
	TDoubleLinkedList<FSimulatingBody> aoSimulatingBodies;

	UPROPERTY(BlueprintReadWrite, Category = Buff)
	TArray<FBuff> aoBuffs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Item)
	TArray<AItem*> apkInventory;

	// the closest item to the character
	AItem* pkClosestItem;

	UCharacterAnimInstance* pkCharAnim;

	// Camera Functions

	void CameraFollow(float DeltaTime);
};