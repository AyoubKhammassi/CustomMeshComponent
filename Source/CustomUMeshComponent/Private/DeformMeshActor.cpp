// Fill out your copyright notice in the Description page of Project Settings.


#include "DeformMeshActor.h"
#include "EngineUtils.h"


// Sets default values
ADeformMeshActor::ADeformMeshActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	DeformMeshComp = CreateDefaultSubobject<UDeformMeshComponent>(TEXT("Deform Mesh Component"));
	Controller = CreateDefaultSubobject<AActor>(TEXT("Controller"));

}

// Called when the game starts or when spawned
void ADeformMeshActor::BeginPlay()
{
	Super::BeginPlay();

	const auto Transform = Controller->GetTransform();
	//We create a new deform mesh section using the static mesh and the transform of the actor
	DeformMeshComp->CreateMeshSection(0, TestMesh, Transform);

	
}

// Called every frame
void ADeformMeshActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const auto Transform = Controller->GetTransform();// TestTransform1->GetComponentTransform();
	//We update the deform transform of the previously created deform mesh section
	DeformMeshComp->UpdateMeshSectionTransform(0, Transform);
	//We finalize all the deform transforms updates, in our case, just one
	DeformMeshComp->FinishTransformsUpdate();

}

