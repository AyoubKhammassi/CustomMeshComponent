// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"



class DEFORMMESH_API FDeformMeshModule : public IModuleInterface
{
public:
	static inline FDeformMeshModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FDeformMeshModule>("DeformMesh");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("DeformMesh");
	}

public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
