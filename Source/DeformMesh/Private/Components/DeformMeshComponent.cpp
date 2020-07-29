#include "Components/DeformMeshComponent.h"
#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "PrimitiveSceneProxy.h"
#include "Containers/ResourceArray.h"
#include "EngineGlobals.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "StaticMeshResources.h"
#include "MeshMaterialShader.h"
#include "ShaderParameters.h"
#include "RHIUtilities.h"

#include "MeshMaterialShader.h"



//Forward Declarations
class FDeformMeshSceneProxy;
class FDeformMeshProxySection;
class FDeformMeshVertexFactoryShaderParameters;
struct FDeformMeshVertexFactory;


///////////////////////////////////////////////////////////////////////
// The DeformMesh class methods

void UDeformMeshComponent::CreateMeshSection(int32 SectionIndex, UStaticMesh* Mesh, const FMatrix& Transform)
{
	// Ensure sections array is long enough
	if (SectionIndex >= DeformMeshSections.Num())
	{
		DeformMeshSections.SetNum(SectionIndex + 1, false);
	}

	// Reset this section (in case it already existed)
	FDeformMeshSection& NewSection = DeformMeshSections[SectionIndex];
	NewSection.Reset();

	// Fill in the mesh section with the needed data
	// i'm assuming that the StaticMesh has only one section and I'm inly using that
	// But the StaticMeshComponent is also a MeshComponent and has multiple mesh sections
	// If you're interested in all the Sections of the StaticMesh, you can apply the same logic for each section
	NewSection.StaticMesh = Mesh;
	NewSection.DeformTransform = Transform;

	NewSection.StaticMesh->CalculateExtendedBounds();
	NewSection.SectionLocalBox += NewSection.StaticMesh->GetBoundingBox();

	//Add this sections material to the list of the component's materials, with the same index as the section
	SetMaterial(SectionIndex, NewSection.StaticMesh->GetMaterial(0));
	

	UpdateLocalBounds(); // Update overall bounds
	MarkRenderStateDirty(); // New section requires recreating scene proxy
}

/// <summary>
/// Update the Transform Matrix that we use to deform the mesh
/// The update of the state in the game thread in simple, but for the scene proxy update, we need to enqueue a render command
/// </summary>
/// <param name="SectionIndex"> The index for the section that we want to update its DeformTransform </param>
/// <param name="Transform"> The new Transform Matrix </param>
void UDeformMeshComponent::UpdateMeshSection(int32 SectionIndex, const FMatrix& Transform)
{
	if (SectionIndex < DeformMeshSections.Num())
	{
		//Set game thread state
		DeformMeshSections[SectionIndex].DeformTransform = Transform;

		if (SceneProxy)
		{
			// Enqueue command to modify render thread info
			FDeformMeshSceneProxy* DeformMeshSceneProxy = (FDeformMeshSceneProxy*)SceneProxy;
			ENQUEUE_RENDER_COMMAND(FDeformMeshTransformsUpdate)(
				[DeformMeshSceneProxy, SectionIndex, Transform](FRHICommandListImmediate& RHICmdList)
				{
					DeformMeshSceneProxy->UpdateSectionTransform_RenderThread(SectionIndex, Transform);
				});
		}
		UpdateLocalBounds();		 // Update overall bounds
		MarkRenderTransformDirty();  // Need to send new bounds to render thread
	}
}

void UDeformMeshComponent::ClearMeshSection(int32 SectionIndex)
{
	if (SectionIndex < DeformMeshSections.Num())
	{
		DeformMeshSections[SectionIndex].Reset();
		UpdateLocalBounds();
		MarkRenderStateDirty();
	}
}

void UDeformMeshComponent::ClearAllMeshSections()
{
	DeformMeshSections.Empty();
	UpdateLocalBounds();
	MarkRenderStateDirty();
}

void UDeformMeshComponent::SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility)
{
	if (SectionIndex < DeformMeshSections.Num())
	{
		// Set game thread state
		DeformMeshSections[SectionIndex].bSectionVisible = bNewVisibility;

		if (SceneProxy)
		{
			// Enqueue command to modify render thread info
			FDeformMeshSceneProxy* DeformMeshSceneProxy = (FDeformMeshSceneProxy*)SceneProxy;
			ENQUEUE_RENDER_COMMAND(FDeformMeshSectionVisibilityUpdate)(
				[DeformMeshSceneProxy, SectionIndex, bNewVisibility](FRHICommandListImmediate& RHICmdList)
				{
					DeformMeshSceneProxy->SetSectionVisibility_RenderThread(SectionIndex, bNewVisibility);
				});
		}
	}
}

bool UDeformMeshComponent::IsMeshSectionVisible(int32 SectionIndex) const
{
	return (SectionIndex < DeformMeshSections.Num()) ? DeformMeshSections[SectionIndex].bSectionVisible : false;
}

int32 UDeformMeshComponent::GetNumSections() const
{
	return DeformMeshSections.Num();
}


FDeformMeshSection* UDeformMeshComponent::GetDeformMeshSection(int32 SectionIndex)
{
	if (SectionIndex < DeformMeshSections.Num())
	{
		return &DeformMeshSections[SectionIndex];
	}
	else
	{
		return nullptr;
	}
}

void UDeformMeshComponent::SetDeformMeshSection(int32 SectionIndex, const FDeformMeshSection& Section)
{
	// Ensure sections array is long enough
	if (SectionIndex >= DeformMeshSections.Num())
	{
		DeformMeshSections.SetNum(SectionIndex + 1, false);
	}

	DeformMeshSections[SectionIndex] = Section;

	UpdateLocalBounds(); // Update overall bounds
	MarkRenderStateDirty(); // New section requires recreating scene proxy
}

FPrimitiveSceneProxy* UDeformMeshComponent::CreateSceneProxy()
{
	if (!SceneProxy)
		return new FDeformMeshSceneProxy(this);
	else
		return SceneProxy;
}

int32 UDeformMeshComponent::GetNumMaterials() const
{
	return DeformMeshSections.Num();
}


//Use this to update the Bounds by taking in consideration the deform transform
FBoxSphereBounds UDeformMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds Ret(LocalBounds.TransformBy(LocalToWorld));

	Ret.BoxExtent *= BoundsScale;
	Ret.SphereRadius *= BoundsScale;

	return Ret;
}

void UDeformMeshComponent::UpdateLocalBounds()
{
	FBox LocalBox(ForceInit);

	for (const FDeformMeshSection& Section : DeformMeshSections)
	{
		LocalBox += Section.SectionLocalBox;
	}

	LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0); // fallback to reset box sphere bounds

	// Update global bounds
	UpdateBounds();
	// Need to send to render thread
	MarkRenderTransformDirty();
}

