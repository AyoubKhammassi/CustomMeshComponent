// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Components/MeshComponent.h"
#include "PhysicsEngine/ConvexElem.h"
#include "Engine/StaticMesh.h"
#include "DeformMeshComponent.generated.h"

//Forward declarations
class FPrimitiveSceneProxy;



/** Mesh section of the DeformMesh. A mesh section is a part of the mesh that is rendered with one material (1 material per section)*/
USTRUCT()
struct FDeformMeshSection
{
	GENERATED_BODY()
public:

	/** The static mesh that holds the mesh data for this section */
	UPROPERTY()
		UStaticMesh* StaticMesh;

	/** The secondary transform matrix that we'll use to deform this mesh section*/
	UPROPERTY()
		FMatrix DeformTransform;

	/** Local bounding box of section */
	UPROPERTY()
		FBox SectionLocalBox;

	/** Should we display this section */
	UPROPERTY()
		bool bSectionVisible;

	FDeformMeshSection()
		: SectionLocalBox(ForceInit)
		, bSectionVisible(true)
	{}

	/** Reset this section, clear all mesh info. */
	void Reset()
	{
		StaticMesh = nullptr;
		SectionLocalBox.Init();
		bSectionVisible = true;
	}
};

/**
*	Component that allows you deform the vertices of a mesh by supplying a secondary deform transform
*/
UCLASS(hidecategories = (Object, LOD), meta = (BlueprintSpawnableComponent), ClassGroup = Rendering)
class DEFORMMESH_API UDeformMeshComponent : public UMeshComponent
{
	GENERATED_BODY()
public:
	
	void CreateMeshSection(int32 SectionIndex, UStaticMesh* Mesh, const FMatrix& DeformTransform);

	void UpdateMeshSection(int32 SectionIndex, const FMatrix& DeformTransform);

	/** Clear a section of the DeformMesh. Other sections do not change index. */
	void ClearMeshSection(int32 SectionIndex);

	/** Clear all mesh sections and reset to empty state */
	void ClearAllMeshSections();

	/** Control visibility of a particular section */
	void SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility);

	/** Returns whether a particular section is currently visible */
	bool IsMeshSectionVisible(int32 SectionIndex) const;

	/** Returns number of sections currently created for this component */
	int32 GetNumSections() const;

	/**
	 *	Get pointer to internal data for one section of this Puzzle mesh component.
	 *	Note that pointer will becomes invalid if sections are added or removed.
	 */
	FDeformMeshSection* GetDeformMeshSection(int32 SectionIndex);

	/** Replace a section with new section geometry */
	void SetDeformMeshSection(int32 SectionIndex, const FDeformMeshSection& Section);


	
	//~ Begin UPrimitiveComponent Interface.
	/* PrimitiveComponents are SceneComponents that contain or generate some sort of geometry, generally to be rendered or used as collision data. (UE4 docs)
	* MeshComponents are primitive components, since they contain mesh data and render it
	* The most important method that we need to implement from the PrimitiveComponent interface is the CreateSceneProxy()
	* Any PrimitiveComponent has a scene proxy, which is the component's proxy in the render thread
	* Just like anything else on the game thread, we CAN'T just use it directly to issue render commands and create render resources
	* Instead, we create a proxy, and we delegate the render threads tasks to it.
	* PS: There's other methods (Collision related) from this interface that i'm not implementing, beacuse I'm only interested in the rendering
	*/
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.


	//~ Begin UMeshComponent Interface.
	/* MeshComponent is an abstract base for any component that is an instance of a renderable collection of triangles. (UE4 docs)
	*/
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.


private:

	//~ Begin USceneComponent Interface.
	/*A SceneComponent has a transform and supports attachment, but has no rendering or collision capabilities. (UE4 docs)
	* The PrimitiveComponent inherits from this class, and adds the rendering and collision capabilities
	* Any scene component, including our mesh component, has a transform and bounds
	* The transform is not managed directly here by the component, so we don't have to worry about that
	* But we need to manage the bounds of our component by implementing this method
	*/
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.


	/** Update LocalBounds member from the local box of each section */
	void UpdateLocalBounds();

	/** Array of sections of mesh */
	UPROPERTY()
		TArray<FDeformMeshSection> DeformMeshSections;

	/** Local space bounds of mesh */
	UPROPERTY()
		FBoxSphereBounds LocalBounds;

	friend class FDeformMeshSceneProxy;
};


