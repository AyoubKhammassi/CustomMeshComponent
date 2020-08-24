// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DeformMesh.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "GlobalShader.h"

IMPLEMENT_GAME_MODULE( FDeformMeshModule, DeformMesh);


void FDeformMeshModule::StartupModule()
{
	// Maps virtual shader source directory to actual shaders directory on disk.
	FString ShaderDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders/Private"));
	AddShaderSourceDirectoryMapping("/CustomShaders", ShaderDirectory);
}

void FDeformMeshModule::ShutdownModule()
{
}

