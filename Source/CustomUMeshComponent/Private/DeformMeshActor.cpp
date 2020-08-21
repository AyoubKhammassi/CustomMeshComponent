// Fill out your copyright notice in the Description page of Project Settings.


#include "DeformMeshActor.h"
#include "EngineUtils.h"


// Sets default values
ADeformMeshActor::ADeformMeshActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	DeformMeshComp = CreateDefaultSubobject<UDeformMeshComponent>(TEXT("Deform Mesh Component"));
	TestTransform = CreateDefaultSubobject<USceneComponent>(TEXT("Transform Controller"));

}

// Called when the game starts or when spawned
void ADeformMeshActor::BeginPlay()
{
	Super::BeginPlay();
	const FMatrix TransformMatrix = TestTransform->GetComponentTransform().ToMatrixWithScale().GetTransposed();
	DeformMeshComp->CreateMeshSection(0, TestMesh, TransformMatrix);
	
}

// Called every frame
void ADeformMeshActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const FMatrix TransformMatrix = TestTransform->GetComponentTransform().ToMatrixWithScale().GetTransposed();
	DeformMeshComp->UpdateMeshSectionTransform(0, TransformMatrix);
	DeformMeshComp->FinishTransformsUpdate();

}

