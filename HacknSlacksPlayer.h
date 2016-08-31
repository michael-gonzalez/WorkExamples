// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine.h"
#include "Sheaths.h"
#include "WeaponTypes.h"
#include "BodyPoses.h"
#include "Sheath.h"
#include "HackNSlacksCharacter.h"
#include "GameFramework/Character.h"
#include "HacknSlacksPlayer.generated.h"

class AEnemy;
struct FAbilityData;
	
UCLASS(config = Game)
class HACKNSLACKS_API AHacknSlacksPlayer : public AHackNSlacksCharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;

public:
	AHacknSlacksPlayer(const FObjectInitializer& ObjectInitializer);

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	virtual void BeginPlay() override;

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	virtual void ReceiveAnyDamage(float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	virtual float SetHealth(float fNewHealth) override;

	virtual float ModHealth(float fMod) override;

	UFUNCTION(BlueprintCallable, Category = Tokens)
	void SetTokens(int32 iNewTokens);

	// modify the number of tokens the player has
	UFUNCTION(BlueprintCallable, Category = Tokens)
	void ModTokens(int32 iMod);

	UFUNCTION(BlueprintCallable, Category = Crystals)
	void SetCrystals(int32 iNewCrystals);

	UFUNCTION(BlueprintCallable, Category = Crystals)
	void ModCrystals(int32 iMod);

	void SetDodging(bool bIsDodging) override;

	UFUNCTION()
	void SoftLockSphereBeginOverlap(class AActor* pkOther, class UPrimitiveComponent* pkOtherComp, int32 iOtherBodyIndex, bool bFromSweep, const FHitResult &oSweepResult);

	UFUNCTION()
	void SoftLockSphereEndOverlap(class AActor* pkOther, class UPrimitiveComponent* pkOtherComp, int32 iOtherBodyIndex);

	UFUNCTION(BlueprintImplementableEvent, Category = Buff)
	void OnAddBuff(const FBuff& oBuff);

	FBuff& AddBuff(TSubclassOf<UBuffDef> pkBuffDef, float fIntensity, float fDuration, int32 iTickCount) override;

	// increase combo counter
	UFUNCTION(BlueprintCallable, Category = Combo)
	void AddComboHit(float fDamage);

	UFUNCTION(BlueprintCallable, Category = Combo)
	void ResetComboCounter();

	// sets the players current weapon - no animation, called by anim notifies
	bool SetWeapon(AWeapon* pkNewWeapon) override;

	// calls SetWeapon(AWeapon) with the weapon in the specified sheath
	UFUNCTION(BlueprintCallable, Category = Weapon)
	bool SetWeapon(ESheaths eSheath);

	// choose animation for changing weapon
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void AnimSetWeapon(ESheaths eSheath);

	// assign menu weapon to Slacks
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void AssignWeapon(AWeapon* pkSheathWeapon);

	UFUNCTION(BlueprintImplementableEvent, Category = Combo)
	void OnComboCountIncreased(int32 iNewComboCount);

	UFUNCTION(BlueprintImplementableEvent, Category = Combo)
	void OnComboCountReset(int32 iLastComboCount);

	UFUNCTION(BlueprintImplementableEvent, Category = Player)
	void OnLevelStreamed();

	// shift active buffs left when a buff is removed
	void ShiftBuffs(int32 iIndex);

	// convenience function to access the player's controller as a player controller
	UFUNCTION(BlueprintCallable, Category = Player)
	APlayerController* GetPlayerController();

	UPROPERTY(BlueprintReadOnly, Category = Crystals)
	int32 iCrystals;

	// capsule for tracking nearby enemies that will be targeted on directional attacks
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Player)
	USphereComponent* pkSoftLockSphere;

	FName* socketUpper;
	
	FName* socketLower;
	
	FName* socketFront;
	
	// number of buffs active on the player
	UPROPERTY(BlueprintReadWrite, Category = Buff)
	int32 iActiveBuffs;

	UPROPERTY(EditAnywhere, Category = Sheath)
	FSheath aoSheaths[(int32)ESheaths::Count + 1];

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* InputComponent) override;
	// End of APawn interface

	// input callbacks
	void OnDodge();
	void OnSwapWeapon();
	void OnInteract();
	void OnJump();
	void OnSprint();
	void OnEndSprint();
	void OnShoot();
	void OnAim();
	void OnAbility();
	void OnCharacterOverview();
	void OnPause();

	static bool CanPause();

	void EndDodge();

	virtual void Jump() override;

	// when the player touches the ground after being airborne
	virtual void Landed(const FHitResult& Hit) override;

	virtual bool CanJumpInternal_Implementation() const override;

	// move during an attack
	virtual void AttackMove(FAttackEntry* poAttackEntry) override;

	// while the player is charging an attack
	virtual void UpdateCharge(FAttackEntry* poAttackEntry, UCharacterAnimInstance* pkCharAnim) override;

	// when the player finishes charging an attack
	virtual void OnEndCharge(FAttackEntry* poAttackEntry, UCharacterAnimInstance* pkCharAnim) override;

	// when the player performs an attack
	virtual void OnPerformAttack(FAttackEntry* poAttackEntry, UCharacterAnimInstance* pkCharAnim, float fPlayRate) override;

	// play animation and turn player to target rotation during animation
	void PlayAnimToAngle(UCharacterAnimInstance* pkCharAnim, EBodyPoses eBodyPose, UAnimSequenceBase* pkAnim, float fPlayRate, bool bIsAttack, float fCurAngle, float fTargetAngle);

	// get angle to attack towards
	float GetAttackAngle(FVector oTargetDir, bool bSoftLock = true);

	// get enemy closest to the players viewing angle
	void GetClosestAngleEnemy(FVector oTargetDir);

	// get difference in viewing angle to soft locked enemy
	float GetAngleDiffToSoftLocked(FVector oTargetDir);

	// get which animation to use for the dodge
	UAnimSequenceBase* GetDodgeAnim();

	UFUNCTION(BlueprintCallable, Category = Buff)
	void UpdateBuffs() override;

	UFUNCTION(BlueprintImplementableEvent, Category = Buff)
	void OnRemoveBuff(const FBuff& oBuff, int32 iIndex);

	// update post processing effects
	void UpdateFX();

	// Testing for camera auto pitch adjustments
	void PitchAutoAdjustment(float DeltaTime);

	bool bMoving;
	bool bPitchAdjust;
	bool bDisablePitchAdjust;

	float fPitchAutoAdjustTimer;

	FRotator rTargetPitchRotation;
	FRotator rCurrentPitchRotation;

	//////////////

	// number of times the player has jumped since last touching the ground
	int32 iJumpCount;

	// maximum number of times the player can jump before they must touch the ground again
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Player)
	int32 iMaxJumpCount;

	// how long the player can not hit an enemy before their combo hit count is reset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Combo)
	float fComboNoHitDuration;

	// timer for duration
	UPROPERTY(BlueprintReadWrite, Category = Combo)
	float fComboNoHitTimer;

	// number of hits the player has scored this combo (reset on taking damage?)
	UPROPERTY(BlueprintReadWrite, Category = Combo)
	int32 iComboHitCount;

	// the player's current experience (make a value for total xp?)
	int32 iCurrentXP;

	// player's token count
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Player)
	int32 iTokens;

	// number of lives the player has left on this level - reset level if no lives left
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Player)
	int32 iLives;

	// number of lives the player will start each level with
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Player)
	int32 iMaxLives;

	// using a transform for location and rotation - where to respawn on death if lives > 0
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Player)
	FTransform oLastCheckpoint;

	// MOVEMENT

	// speed at which the player moves while running
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float fRunSpeed;

	// speed at which the player moves while sprinting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float fSprintSpeed;

	// how long the player has been in the air
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float fAirTimer;

	// maximum time the player can be in the air before having their position reset to the last grounded location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float fMaxAirTime;

	// ground deceleration at running or slower speeds
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Movement)
	float fMinDeceleration;

	// ground deceleration at sprinting or faster speeds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float fMaxDeceleration;

	// last position the player was standing on ground - to reset to, if you get stuck
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	FVector oLastGroundPosition;

	// DODGE

	// dodge animation for dodging forward
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Dodge)
	UAnimSequenceBase* pkFrontDodge;

	// storing ground friction, friction is set to zero during dodge
	float fSavedFriction;

	// TARGETING

	// maximum angle difference for directional attack auto-targeting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attack)
	float fMaxDirectionalDeviation;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = Ability)
	TArray<UAbilityData*> apkAbilities;

	// nearby enemy that is closest to player's viewing angle
	UPROPERTY(BlueprintReadWrite, Category = Attack)
	AEnemy* pkClosestAngleTarget;

	// nearby enemy that the player will attack
	UPROPERTY(BlueprintReadWrite, Category = Attack)
	AEnemy* pkSoftLockedTarget;

	// list of nearby enemies for directional attacks
	TDoubleLinkedList<AEnemy*> apkNearbyEnemies;

	// arrow to indicate which enemy is the soft lock target
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visual)
	UArrowComponent* pkSoftLockArrow;
};