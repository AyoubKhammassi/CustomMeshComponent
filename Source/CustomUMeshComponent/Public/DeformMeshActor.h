// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/DeformMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DeformMeshActor.generated.h"

UCLASS()
class CUSTOMUMESHCOMPONENT_API ADeformMeshActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADeformMeshActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere)
		UDeformMeshComponent* DeformMeshComp;

	UPROPERTY(EditAnywhere)
		UStaticMesh* TestMesh;

	UPROPERTY(EditAnywhere)
		USceneComponent* TestTransform1;


	UPROPERTY(EditAnywhere)
		USceneComponent* TestTransform2;

	UPROPERTY(EditAnywhere)
		AActor* controller;

};
