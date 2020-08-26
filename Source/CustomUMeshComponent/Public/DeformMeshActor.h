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

/*
 * This is a simple actor that has a DeformMeshComponent
 * It uses the DeformMeshComponent API to create mesh sections and update their deform transforms
*/
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

	//We're creating a mesh section from this static mesh
	UPROPERTY(EditAnywhere)
		UStaticMesh* TestMesh;

	// We're using the transform of this actor as a deform transform
	UPROPERTY(EditAnywhere)
		AActor* Controller;

};
