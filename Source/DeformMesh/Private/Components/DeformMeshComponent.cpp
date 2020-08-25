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
class FDeformMeshSectionProxy;
class FDeformMeshVertexFactoryShaderParameters;
struct FDeformMeshVertexFactory;


///////////////////////////////////////////////////////////////////////
// The DeformMesh Vertex factory
/*
 * We're inheriting from the FLocalvertexfactory because most of the logic is reusible
 * However there's some data and functions that we're interested in
 * You can inherit directly from FVertexFactory and implement the logic that suits you, but you'll have to implement everything from scratch
*/
///////////////////////////////////////////////////////////////////////
struct FDeformMeshVertexFactory : FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FDeformMeshVertexFactory);
public:


	FDeformMeshVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLocalVertexFactory(InFeatureLevel, "FDeformMeshVertexFactory")
	{
		//We're not interested in Manual vertex fetch so we disable it 
		bSupportsManualVertexFetch = false;
	}

	/* Should we cache the material's shadertype on this platform with this vertex factory? */
	/* Given these parameters, we can decide in which permutations should this vertex factory be included*/
	/* For example, we're only intersted in unlit materials, so we only return true when 
	1 Material Domain is Surface
	2 Shading Model is Unlit
	* We also add the permutation for the default material, because if that's not found for this vertex factory, the engine would crash
	* That's because the default material is the fallback for all other materials, so it needs to be included for all vertex factories
	*/
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
	{
		if ((Parameters.MaterialParameters.MaterialDomain == MD_Surface && Parameters.MaterialParameters.ShadingModels == MSM_Unlit) || Parameters.MaterialParameters.bIsDefaultMaterial)
		{
			return true;
		}
		return false;
	}

	/* Modify compile environment to enable DeformMesh  deformation */
	/* We do this by using preprocessor directives, so when compilation happens, only the code that we're interested in gets in the compiled shader*/
	/* Check LocalVertexFactory.ush */
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		const bool ContainsManualVertexFetch = OutEnvironment.GetDefinitions().Contains("MANUAL_VERTEX_FETCH");
		if (!ContainsManualVertexFetch)
		{
			OutEnvironment.SetDefine(TEXT("MANUAL_VERTEX_FETCH"), TEXT("0"));
		}

		OutEnvironment.SetDefine(TEXT("DEFORM_MESH"), TEXT("1"));
	}


	/* This is the main method that we're interested in*/
	/* Here we can initialize our RHI resources, so we can decide what would be in the final streams and the vertex declaration*/
	/* In the LocalVertexFactory, 3 vertex declarations are initialized; PositionOnly, PositionAndNormalOnly, and the default, which is the one that will be used in the final rendering*/
	/* PositionOnly is mandatory if you're enabling depth passes, however we can get rid of the PositionAndNormal since we're not interested in shading and we're only supporting unlit materils*/
	virtual void InitRHI() override 
	{

		// Check if this vertex factory has a valid feature level that is supported by the current platform
		check(HasValidFeatureLevel());


		//The vertex declaration element lists (Nothing but an array of FVertexElement)
		FVertexDeclarationElementList Elements; //Used for the Default vertex stream
		FVertexDeclarationElementList PosOnlyElements; // Used for the PositionOnly vertex stream

		if (Data.PositionComponent.VertexBuffer != NULL)
		{
			//We add the position stream component to both elemnt lists
			Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));
			PosOnlyElements.Add(AccessStreamComponent(Data.PositionComponent, 0, EVertexInputStreamType::PositionOnly));
		}

		//Initialize the Position Only vertex declaration which will be used in the depth pass
		InitDeclaration(PosOnlyElements, EVertexInputStreamType::PositionOnly);

		//We add all the available texcoords to the default element list, that's all what we'll need for unlit shading 
		if (Data.TextureCoordinates.Num())
		{
			const int32 BaseTexCoordAttribute = 4;
			for (int32 CoordinateIndex = 0; CoordinateIndex < Data.TextureCoordinates.Num(); CoordinateIndex++)
			{
				Elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[CoordinateIndex],
					BaseTexCoordAttribute + CoordinateIndex
				));
			}

			for (int32 CoordinateIndex = Data.TextureCoordinates.Num(); CoordinateIndex < MAX_STATIC_TEXCOORDS / 2; CoordinateIndex++)
			{
				Elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
					BaseTexCoordAttribute + CoordinateIndex
				));
			}
		}

		check(Streams.Num() > 0);

		InitDeclaration(Elements);
		check(IsValidRef(GetDeclaration()));
	}

	/* No need to override the ReleaseRHI() method, since we're not crearting any additional resources*/
	/* The base FVertexFactory::ReleaseRHI() will empty the 3 vertex streams and release the 3 vertex declarations (Probably just decrement the ref count since a declaration is cached and can be used by multiple vertex factories)*/


	//Setters
	inline void SetTransformIndex(uint16 Index) { TransformIndex = Index; }
	inline void SetSceneProxy(FDeformMeshSceneProxy* Proxy) { SceneProxy = Proxy; }
private:
	//We need to pass this as a shader parameter, so we store it in the vertex factory and we use in the vertex factory shader parameters
	uint16 TransformIndex;
	//All the mesh sections proxies keep a pointer to the scene proxy of the component so they can acess the unified SRV
	FDeformMeshSceneProxy* SceneProxy;

	friend class FDeformMeshVertexFactoryShaderParameters;
};

///////////////////////////////////////////////////////////////////////




/** Class representing a single section of the puzzle mesh */
class FDeformMeshSectionProxy
{
public:
	////////////////////////////////////////////////////////
	/** Material applied to this section */
	UMaterialInterface* Material;
	/** Index buffer for this section */
	FRawStaticIndexBuffer IndexBuffer;
	/** Vertex factory for this section */
	FDeformMeshVertexFactory VertexFactory;
	/** Whether this section is currently visible */
	bool bSectionVisible;
	/* Max vertix index is an info that is needed when rendering the mesh, so we cache it here so we don't have to pointer chase it later*/
	uint32 MaxVertexIndex;

	/* For each section, we'll create a vertex factory to store the per-instance mesh data*/
	FDeformMeshSectionProxy(ERHIFeatureLevel::Type InFeatureLevel)
		: Material(NULL)
		, VertexFactory(InFeatureLevel)
		, bSectionVisible(true)
	{}
};


///////////////////////////////////////////////////////////////////////


static inline void InitOrUpdateResource(FRenderResource* Resource)
{
	if (!Resource->IsInitialized())
	{
		Resource->InitResource();
	}
	else
	{
		Resource->UpdateRHI();
	}
}

/* Helper function to initialize the vertex buffers of the vertex factory from the static mesh vertex buffers
* We're using this so we can initialize only the data that we're interested in.
*/
static void InitVertexFactoryData(FDeformMeshVertexFactory* VertexFactory, FStaticMeshVertexBuffers* VertexBuffers)
{
	ENQUEUE_RENDER_COMMAND(StaticMeshVertexBuffersLegacyInit)(
		[VertexFactory, VertexBuffers](FRHICommandListImmediate& RHICmdList)
		{
			//Initialize or update the RHI vertex buffers
			InitOrUpdateResource(&VertexBuffers->PositionVertexBuffer);
			InitOrUpdateResource(&VertexBuffers->StaticMeshVertexBuffer);

			//Use the RHI vertex buffers to create the needed Vertex stream components in an FDataType instance, and then set it as the data of the vertex factory
			FLocalVertexFactory::FDataType Data;
			VertexBuffers->PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory, Data);
			VertexBuffers->StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactory, Data);
			VertexFactory->SetData(Data);

			//Initalize the vertex factory using the data that we just set, this will call the InitRHI() method that we implemented in out vertex factory
			InitOrUpdateResource(VertexFactory);
		});
}

/** Puzzle mesh scene proxy */
class FDeformMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	/* On construction of the Scene proxy, we'll copy all the needed data from the game thread mesh sections to create the needed render thread mesh section proxies*/
	/* We'll also create the structured buffer that will contain the deform transforms of all the sections*/
	FDeformMeshSceneProxy(UDeformMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		// Copy each section
		const uint16 NumSections = Component->DeformMeshSections.Num();
		DeformTransforms.AddZeroed(NumSections);
		Sections.AddZeroed(NumSections);
		for (uint16 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
		{
			FDeformMeshSection& SrcSection = Component->DeformMeshSections[SectionIdx];
			{
				FDeformMeshSectionProxy* NewSection = new FDeformMeshSectionProxy(GetScene().GetFeatureLevel());
				auto& LODResource = SrcSection.StaticMesh->RenderData->LODResources[0];

				FDeformMeshVertexFactory* VertexFactory= &NewSection->VertexFactory;
				//Initialize the vertex factory with the vertex data from the static mesh
				InitVertexFactoryData(VertexFactory, &(LODResource.VertexBuffers));
				//Initialize the additional data (Transform Index and pointer to this scene proxy that holds reference to the structured buffer and its SRV
				VertexFactory->SetTransformIndex(SectionIdx);
				VertexFactory->SetSceneProxy(this);

				//Copy the indices from the static mesh index buffer and use it to initialize the section proxy's index buffer
				{
					TArray<uint32> tmp_indices;
					LODResource.IndexBuffer.GetCopy(tmp_indices);
					NewSection->IndexBuffer.AppendIndices(tmp_indices.GetData(), tmp_indices.Num());
					//Initialize the render resource
					BeginInitResource(&NewSection->IndexBuffer);
				}

				//Fill the array of transforms with the transform matrix from each section
				DeformTransforms[SectionIdx] = SrcSection.DeformTransform;

				//Set the max vertex index for this mesh section
				NewSection->MaxVertexIndex = LODResource.VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

				//Get the material of this section
				NewSection->Material = Component->GetMaterial(SectionIdx);

				if (NewSection->Material == NULL)
				{
					NewSection->Material = UMaterial::GetDefaultMaterial(MD_Surface);
				}

				// Copy visibility info
				NewSection->bSectionVisible = SrcSection.bSectionVisible;

				// Save ref to new section
				Sections[SectionIdx] = NewSection;
			}
		}

		//Create the structured buffer only if we have at least one section
		if(NumSections > 0)
		{
			///////////////////////////////////////////////////////////////
			//// CREATING THE STRUCTURED BUFFER FOR THE DEFORM TRANSFORMS OF ALL THE SECTIONS
			//We'll use one structured buffer for all the transforms

			//We first create a resource array to use it in the create info for initializing the structured buffer on creation
			TResourceArray<FMatrix>* ResourceArray = new TResourceArray<FMatrix>(true);
			FRHIResourceCreateInfo CreateInfo;
			ResourceArray->Append(DeformTransforms);
			CreateInfo.ResourceArray = ResourceArray;
			CreateInfo.DebugName = TEXT("DeformMesh_TransformsSB");

			DeformTransformsSB = RHICreateStructuredBuffer(sizeof(FMatrix), NumSections * sizeof(FMatrix), BUF_ShaderResource, CreateInfo);
			bDeformTransformsDirty = false;
			///////////////////////////////////////////////////////////////
			//// CREATING AN SRV FOR THE STRUCTUED BUFFER SO WA CAN USE IT AS A SHADER RESOURCE PARAMETER AND BIND IT TO THE VERTEX FACTORY
			DeformTransformsSRV = RHICreateShaderResourceView(DeformTransformsSB);

			///////////////////////////////////////////////////////////////
		}
	}

	virtual ~FDeformMeshSceneProxy()
	{
		//For each section , release the render resources
		for (FDeformMeshSectionProxy* Section : Sections)
		{
			if (Section != nullptr)
			{
				Section->IndexBuffer.ReleaseResource();
				Section->VertexFactory.ReleaseResource();
				delete Section;
			}
		}

		//Release the structured buffer and the SRV
		DeformTransformsSB.SafeRelease();
		DeformTransformsSRV.SafeRelease();
	}


	//Updates the structured buffer using the array of deform transforms
	void UpdateDeformTransformsSB_RenderThread()
	{
		check(IsInRenderingThread());
		//Update the structured buffer only if it needs update
		if(bDeformTransformsDirty)
		{
			void* StructuredBufferData = RHILockStructuredBuffer(DeformTransformsSB, 0, DeformTransforms.Num() * sizeof(FMatrix), RLM_WriteOnly);
			FMemory::Memcpy(StructuredBufferData, DeformTransforms.GetData(), DeformTransforms.Num() * sizeof(FMatrix));
			RHIUnlockStructuredBuffer(DeformTransformsSB);
			bDeformTransformsDirty = false;
		}
	}

	//Update the deform transform that is being used to deform this section
	void UpdateDeformTransform_RenderThread(int32 SectionIndex, FMatrix Transform)
	{
		check(IsInRenderingThread());
		if (SectionIndex < Sections.Num() &&
			Sections[SectionIndex] != nullptr)
		{
			DeformTransforms[SectionIndex] = Transform;
			//Mark as dirty
			bDeformTransformsDirty = true;
		}
	}

	void SetSectionVisibility_RenderThread(int32 SectionIndex, bool bNewVisibility)
	{
		check(IsInRenderingThread());

		if (SectionIndex < Sections.Num() &&
			Sections[SectionIndex] != nullptr)
		{
			Sections[SectionIndex]->bSectionVisible = bNewVisibility;
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		// Set up wireframe material (if needed)
		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		FColoredMaterialRenderProxy* WireframeMaterialInstance = NULL;
		if (bWireframe)
		{
			WireframeMaterialInstance = new FColoredMaterialRenderProxy(
				GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL,
				FLinearColor(0, 0.5f, 1.f)
			);

			Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
		}

		// Iterate over sections
		for (const FDeformMeshSectionProxy* Section : Sections)
		{
			if (Section != nullptr && Section->bSectionVisible)
			{
				FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : Section->Material->GetRenderProxy();

				// For each view..
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (VisibilityMap & (1 << ViewIndex))
					{
						const FSceneView* View = Views[ViewIndex];
						// Draw the mesh.
						FMeshBatch& Mesh = Collector.AllocateMesh();
						FMeshBatchElement& BatchElement = Mesh.Elements[0];
						BatchElement.IndexBuffer = &Section->IndexBuffer;
						Mesh.bWireframe = bWireframe;
						Mesh.VertexFactory = &Section->VertexFactory;
						Mesh.MaterialRenderProxy = MaterialProxy;

						bool bHasPrecomputedVolumetricLightmap;
						FMatrix PreviousLocalToWorld;
						int32 SingleCaptureIndex;
						bool bOutputVelocity;
						GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

						FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
						DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), bOutputVelocity);
						BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;
						BatchElement.PrimitiveIdMode = PrimID_DynamicPrimitiveShaderData;

						BatchElement.FirstIndex = 0;
						BatchElement.NumPrimitives = Section->IndexBuffer.GetNumIndices() / 3;
						BatchElement.MinVertexIndex = 0;
						BatchElement.MaxVertexIndex = Section->MaxVertexIndex;
						Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
						Mesh.Type = PT_TriangleList;
						Mesh.DepthPriorityGroup = SDPG_World;
						Mesh.bCanApplyViewModeOverrides = false;
						Collector.AddMesh(ViewIndex, Mesh);
					}
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bVelocityRelevance = IsMovable() && Result.bOpaque && Result.bRenderInMainPass;
		return Result;
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint(void) const
	{
		return(sizeof(*this) + GetAllocatedSize());
	}

	uint32 GetAllocatedSize(void) const
	{
		return(FPrimitiveSceneProxy::GetAllocatedSize());
	}

	//Get the SRV of the transforms structured buffer
	inline FShaderResourceViewRHIRef& GetDeformTransformsSRV() { return DeformTransformsSRV; }

private:
	/** Array of sections */
	TArray<FDeformMeshSectionProxy*> Sections;

	FMaterialRelevance MaterialRelevance;

	//The render thread array of transforms of all the sections
	//Individual updates of each section's deform transform will just update the entry in this array
	//Before binding the SRV, we update the content of the structured buffer with this updated array
	TArray<FMatrix> DeformTransforms;
	//The structured buffer that will contain all the deform transoform and going to be used as a shader resource
	FStructuredBufferRHIRef DeformTransformsSB;
	//The shader resource view of the structured buffer, this is what we bind to the vertex factory
	FShaderResourceViewRHIRef DeformTransformsSRV;

	//Whether the structured buffer needs to be updated or not
	bool bDeformTransformsDirty;

	
};

//////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// The DeformMesh Vertex factory shader parameters
/*
 * We can bind shader parameters here
 * There's two types of shader parameters: FShaderPrameter and FShaderResourcePramater
 * We can use the first to pass parameters like floats, integers, arrays
 * W can use the second to pass SRVs (Shader resource View) of certains resources that we can create
 * Actually that's how manual fetch is implmented; for each of the Vertex Buffers of the stream components, an SRV is created
 * That SRV can bound as a shader resource parameter and you can fetch the buffers using the SV_VertexID
*/
///////////////////////////////////////////////////////////////////////
class FDeformMeshVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FDeformMeshVertexFactoryShaderParameters, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		/* We bind our shader paramters to the paramtermap that will be used with it, the SPF_Optional flags tells the compiler that this paramter is optional*/
		/* Otherwise, the shader compiler will complain when this parameter is not present in the shader file*/
		TransformIndex.Bind(ParameterMap, TEXT("DMTransformIndex"), SPF_Optional);
		TransformsSRV.Bind(ParameterMap, TEXT("DMTransforms"), SPF_Optional);
	};

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const FSceneView* View,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
	) const
	{
		if (BatchElement.bUserDataIsColorVertexBuffer)
		{
			const auto* LocalVertexFactory = static_cast<const FLocalVertexFactory*>(VertexFactory);
			FColorVertexBuffer* OverrideColorVertexBuffer = (FColorVertexBuffer*)BatchElement.UserData;
			check(OverrideColorVertexBuffer);

			if (!LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel))
			{
				LocalVertexFactory->GetColorOverrideStream(OverrideColorVertexBuffer, VertexStreams);
			}
		}
		const FDeformMeshVertexFactory* DeformMeshVertexFactory = ((FDeformMeshVertexFactory*)VertexFactory);

		const uint32 Index = DeformMeshVertexFactory->TransformIndex;
		ShaderBindings.Add(TransformIndex, Index);
		ShaderBindings.Add(TransformsSRV, DeformMeshVertexFactory->SceneProxy->GetDeformTransformsSRV());
	};
private:
	LAYOUT_FIELD(FShaderParameter, TransformIndex);
	LAYOUT_FIELD(FShaderResourceParameter, TransformsSRV);

};

IMPLEMENT_TYPE_LAYOUT(FDeformMeshVertexFactoryShaderParameters);

///////////////////////////////////////////////////////////////////////

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FDeformMeshVertexFactory, SF_Vertex, FDeformMeshVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(FDeformMeshVertexFactory, "/CustomShaders/LocalVertexFactory.ush", true, true, true, true, true);

///////////////////////////////////////////////////////////////////////
// The DeformMesh class methods
///////////////////////////////////////////////////////////////////////

void UDeformMeshComponent::CreateMeshSection(int32 SectionIndex, UStaticMesh* Mesh, const FTransform& Transform)
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
	// I'm assuming that the StaticMesh has only one section and I'm inly using that
	// But the StaticMeshComponent is also a MeshComponent and has multiple mesh sections
	// If you're interested in all the Sections of the StaticMesh, you can apply the same logic for each section
	NewSection.StaticMesh = Mesh;
	NewSection.DeformTransform = Transform.ToMatrixWithScale().GetTransposed();

	NewSection.StaticMesh->CalculateExtendedBounds();
	NewSection.SectionLocalBox += NewSection.StaticMesh->GetBoundingBox();
	NewSection.SectionLocalBox += NewSection.StaticMesh->GetBoundingBox().TransformBy(Transform);
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
void UDeformMeshComponent::UpdateMeshSectionTransform(int32 SectionIndex, const FTransform& Transform)
{
	if (SectionIndex < DeformMeshSections.Num())
	{
		//Set game thread state
		const FMatrix TransformMatrix = Transform.ToMatrixWithScale().GetTransposed();
		DeformMeshSections[SectionIndex].DeformTransform = TransformMatrix;

		DeformMeshSections[SectionIndex].SectionLocalBox += DeformMeshSections[SectionIndex].StaticMesh->GetBoundingBox().TransformBy(Transform);


		if (SceneProxy)
		{
			// Enqueue command to modify render thread info
			FDeformMeshSceneProxy* DeformMeshSceneProxy = (FDeformMeshSceneProxy*)SceneProxy;
			ENQUEUE_RENDER_COMMAND(FDeformMeshTransformsUpdate)(
				[DeformMeshSceneProxy, SectionIndex, TransformMatrix](FRHICommandListImmediate& RHICmdList)
				{
					DeformMeshSceneProxy->UpdateDeformTransform_RenderThread(SectionIndex, TransformMatrix);
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

/// <summary>
/// This method is called after we finished updating all the section transforms that we want to update
/// What will this do, is updating the structured buffer with the new transforms
/// </summary>
void UDeformMeshComponent::FinishTransformsUpdate()
{
	if (SceneProxy)
	{
		// Enqueue command to modify render thread info
		FDeformMeshSceneProxy* DeformMeshSceneProxy = (FDeformMeshSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(FDeformMeshAllTransformsSBUpdate)(
			[DeformMeshSceneProxy](FRHICommandListImmediate& RHICmdList)
			{
				DeformMeshSceneProxy->UpdateDeformTransformsSB_RenderThread();
			});
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

