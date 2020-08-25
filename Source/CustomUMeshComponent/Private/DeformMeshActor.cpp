// Fill out your copyright notice in the Description page of Project Settings.


#include "DeformMeshActor.h"
#include "EngineUtils.h"


// Sets default values
ADeformMeshActor::ADeformMeshActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	DeformMeshComp = CreateDefaultSubobject<UDeformMeshComponent>(TEXT("Deform Mesh Component"));
	TestTransform1 = CreateDefaultSubobject<USceneComponent>(TEXT("Transform Controller 1"));
	TestTransform2 = CreateDefaultSubobject<USceneComponent>(TEXT("Transform Controller 2"));
	controller = CreateDefaultSubobject<AActor>(TEXT("Controller"));

}

// Called when the game starts or when spawned
void ADeformMeshActor::BeginPlay()
{
	Super::BeginPlay();
	const auto Transform1 = TestTransform1->GetComponentTransform();
	const auto Transform2 = TestTransform2->GetComponentTransform();

	DeformMeshComp->CreateMeshSection(0, TestMesh, Transform1);
	//DeformMeshComp->CreateMeshSection(1, TestMesh, Transform2);
	
}

// Called every frame
void ADeformMeshActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const auto Transform1 = controller->GetTransform();// TestTransform1->GetComponentTransform();
	const auto Transform2 = TestTransform2->GetComponentTransform();
	DeformMeshComp->UpdateMeshSectionTransform(0, Transform1);
	//DeformMeshComp->UpdateMeshSectionTransform(1, Transform2);
	DeformMeshComp->FinishTransformsUpdate();

}

