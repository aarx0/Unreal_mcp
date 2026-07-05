// =============================================================================
// McpAutomationBridge_LevelHandlers.cpp
// =============================================================================
// manage_level member handlers. Dispatch lives in the FMcpCall classes
// (Private/MCP/Calls/McpCalls_ManageLevel.cpp); each HandleLevel* member here
// implements one advertised action. stream/unload share
// HandleLevelStreamInternal; create_light delegates to HandleLightingAction
// (McpAutomationBridge_LightingHandlers.cpp).
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0-5.7: All handlers supported
// - Level streaming APIs stable across versions
// - World Partition conditional via __has_include
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first
#include "McpHandlerUtils.h"

#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "LevelEditor.h"
#include "RenderingThread.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameMapsSettings.h"
#include "Engine/LevelBounds.h"
#include "LevelUtils.h"
#include "EditorBuildUtils.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "WorldPartition/WorldPartition.h"  // Required for World Partition detection

// ScopedTransaction header location varies by UE version
#if __has_include("ScopedTransaction.h")
#include "ScopedTransaction.h"
#elif __has_include("Misc/ScopedTransaction.h")
#include "Misc/ScopedTransaction.h"
#else
#define MCP_NO_SCOPED_TRANSACTION 1
#endif

// Check for LevelEditorSubsystem
#if defined(__has_include)
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#else
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 0
#endif
#else
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 0
#endif

namespace {
bool IsSafeLevelConsoleToken(const FString& Value) {
  const FString Trimmed = Value.TrimStartAndEnd();
  return !Trimmed.IsEmpty() && !Trimmed.Contains(TEXT("\n")) &&
         !Trimmed.Contains(TEXT("\r")) && !Trimmed.Contains(TEXT("&&")) &&
         !Trimmed.Contains(TEXT("||")) && !Trimmed.Contains(TEXT(";")) &&
         !Trimmed.Contains(TEXT("|")) && !Trimmed.Contains(TEXT("`")) &&
         !Trimmed.Contains(TEXT(" ")) && !Trimmed.Contains(TEXT("\t"));
}

FString NormalizeLevelPackagePath(const FString& InPath) {
  FString PackagePath = InPath;
  int32 ObjectDelimiter = INDEX_NONE;
  if (PackagePath.FindChar(TEXT('.'), ObjectDelimiter)) {
    PackagePath = PackagePath.Left(ObjectDelimiter);
  }
  return PackagePath;
}

bool TryGetAbsoluteMapFilename(const FString& PackagePath, FString& OutFilename) {
  FString RelativeFilename;
  if (!FPackageName::TryConvertLongPackageNameToFilename(
          PackagePath, RelativeFilename, FPackageName::GetMapPackageExtension())) {
    return false;
  }
  OutFilename = FPaths::ConvertRelativePathToFull(RelativeFilename);
  FPaths::NormalizeFilename(OutFilename);
  return true;
}

bool IsGameLevelPackagePath(const FString& PackagePath) {
  return PackagePath.StartsWith(TEXT("/Game/")) &&
         !PackagePath.Contains(TEXT(".."));
}

bool IsUnderProjectContentDir(const FString& AbsolutePath) {
  FString ProjectContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
  FPaths::NormalizeDirectoryName(ProjectContentDir);
  if (!ProjectContentDir.EndsWith(TEXT("/"))) {
    ProjectContentDir += TEXT("/");
  }

  FString NormalizedPath = AbsolutePath;
  FPaths::NormalizeFilename(NormalizedPath);
  return NormalizedPath.StartsWith(ProjectContentDir, ESearchCase::IgnoreCase);
}

bool ValidateWritableGameMapPath(const FString& PackagePath,
                                 const FString& AbsoluteMapFilename,
                                 const TCHAR* Label,
                                 FString& ErrorMessage,
                                 FString& ErrorCode) {
  if (!IsGameLevelPackagePath(PackagePath)) {
    ErrorMessage = FString::Printf(
        TEXT("%s level path must be under /Game: %s"), Label, *PackagePath);
    ErrorCode = TEXT("SECURITY_VIOLATION");
    return false;
  }
  if (!IsUnderProjectContentDir(AbsoluteMapFilename)) {
    ErrorMessage = FString::Printf(
        TEXT("%s level file must be inside the project Content directory: %s"),
        Label, *PackagePath);
    ErrorCode = TEXT("SECURITY_VIOLATION");
    return false;
  }
  return true;
}

bool TryResolveWritableGameMapFilename(const FString& PackagePath,
                                       FString& OutFilename,
                                       FString& ErrorMessage,
                                       FString& ErrorCode,
                                       const TCHAR* Label) {
  if (!TryGetAbsoluteMapFilename(PackagePath, OutFilename)) {
    ErrorMessage = FString::Printf(
        TEXT("Could not convert %s level to filename: %s"), Label, *PackagePath);
    ErrorCode = TEXT("INVALID_LEVEL_PATH");
    return false;
  }
  return ValidateWritableGameMapPath(PackagePath, OutFilename, Label,
                                     ErrorMessage, ErrorCode);
}

void ScanLevelPackagePath(const FString& PackagePath, const FString& AbsoluteMapFilename) {
  IAssetRegistry& AssetRegistry =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
  if (!AbsoluteMapFilename.IsEmpty() && IFileManager::Get().FileExists(*AbsoluteMapFilename)) {
    TArray<FString> FilesToScan;
    FilesToScan.Add(AbsoluteMapFilename);
    AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
  }
  const FString PackageDir = FPaths::GetPath(PackagePath);
  if (!PackageDir.IsEmpty()) {
    TArray<FString> PathsToScan;
    PathsToScan.Add(PackageDir);
    AssetRegistry.ScanPathsSynchronous(PathsToScan, false);
  }
}

bool IsCurrentEditorWorldPackage(const FString& PackagePath) {
  if (!GEditor) {
    return false;
  }
  UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
  return EditorWorld && EditorWorld->GetOutermost() &&
         EditorWorld->GetOutermost()->GetName() == PackagePath;
}

bool GetExternalPackageDirectory(const FString& PackagePath,
                                 const FString& RootDirectoryName,
                                 FString& OutDirectory) {
  if (!IsGameLevelPackagePath(PackagePath)) {
    return false;
  }
  const FString RelativePackagePath = PackagePath.RightChop(6);
  if (RelativePackagePath.IsEmpty()) {
    return false;
  }
  OutDirectory = FPaths::ConvertRelativePathToFull(
      FPaths::ProjectContentDir() / RootDirectoryName / RelativePackagePath);
  FPaths::NormalizeDirectoryName(OutDirectory);
  return IsUnderProjectContentDir(OutDirectory);
}

struct FExternalPackageDirectoryCopyPlan {
  FString SourceDirectory;
  FString DestinationDirectory;
  bool bSourceExists = false;
  bool bDestinationExists = false;
  bool bDeletedDestination = false;
  bool bCopied = false;
};

bool BuildExternalPackageDirectoryCopyPlan(
    const FString& SourcePackagePath,
    const FString& DestinationPackagePath,
    const FString& RootDirectoryName,
    bool bOverwrite,
    FExternalPackageDirectoryCopyPlan& Plan,
    FString& ErrorMessage,
    FString& ErrorCode) {
  if (!GetExternalPackageDirectory(SourcePackagePath, RootDirectoryName,
                                   Plan.SourceDirectory) ||
      !GetExternalPackageDirectory(DestinationPackagePath, RootDirectoryName,
                                   Plan.DestinationDirectory)) {
    return true;
  }

  IFileManager& FileManager = IFileManager::Get();
  Plan.bSourceExists = FileManager.DirectoryExists(*Plan.SourceDirectory);
  Plan.bDestinationExists = FileManager.DirectoryExists(*Plan.DestinationDirectory);
  if (Plan.SourceDirectory.Equals(Plan.DestinationDirectory,
                                  ESearchCase::IgnoreCase)) {
    ErrorMessage = FString::Printf(
        TEXT("Source and destination external package directories are identical: %s"),
        *Plan.SourceDirectory);
    ErrorCode = TEXT("SAME_PATH");
    return false;
  }

  if (Plan.bDestinationExists && !bOverwrite) {
    ErrorMessage = FString::Printf(
        TEXT("Destination external package directory already exists: %s"),
        *Plan.DestinationDirectory);
    ErrorCode = TEXT("DESTINATION_EXISTS");
    return false;
  }
  return true;
}

bool CopyExternalPackageDirectory(FExternalPackageDirectoryCopyPlan& Plan,
                                  FString& ErrorMessage,
                                  FString& ErrorCode) {
  IFileManager& FileManager = IFileManager::Get();
  if (!Plan.bSourceExists) {
    if (Plan.bDestinationExists) {
      Plan.bDeletedDestination = FileManager.DeleteDirectory(
          *Plan.DestinationDirectory, false, true);
      if (!Plan.bDeletedDestination) {
        ErrorMessage = FString::Printf(
            TEXT("Failed to remove stale destination external package directory: %s"),
            *Plan.DestinationDirectory);
        ErrorCode = TEXT("DESTINATION_DELETE_FAILED");
        return false;
      }
    }
    return true;
  }

  if (Plan.bDestinationExists) {
    Plan.bDeletedDestination = FileManager.DeleteDirectory(
        *Plan.DestinationDirectory, false, true);
    if (!Plan.bDeletedDestination) {
      ErrorMessage = FString::Printf(
          TEXT("Failed to overwrite destination external package directory: %s"),
          *Plan.DestinationDirectory);
      ErrorCode = TEXT("DESTINATION_DELETE_FAILED");
      return false;
    }
  }

  FileManager.MakeDirectory(*FPaths::GetPath(Plan.DestinationDirectory), true);
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  Plan.bCopied = PlatformFile.CopyDirectoryTree(
      *Plan.DestinationDirectory, *Plan.SourceDirectory, false);
  if (!Plan.bCopied) {
    ErrorMessage = FString::Printf(
        TEXT("Failed to copy external package directory from %s to %s"),
        *Plan.SourceDirectory, *Plan.DestinationDirectory);
    ErrorCode = TEXT("COPY_FAILED");
    return false;
  }
  return true;
}

bool DeleteExternalPackageDirectory(const FString& PackagePath,
                                    const FString& RootDirectoryName,
                                    bool& bSourceExists,
                                    bool& bDeleted,
                                    FString& ErrorMessage,
                                    FString& ErrorCode) {
  FString Directory;
  if (!GetExternalPackageDirectory(PackagePath, RootDirectoryName, Directory)) {
    bSourceExists = false;
    bDeleted = false;
    return true;
  }

  IFileManager& FileManager = IFileManager::Get();
  bSourceExists = FileManager.DirectoryExists(*Directory);
  if (!bSourceExists) {
    bDeleted = false;
    return true;
  }

  bDeleted = FileManager.DeleteDirectory(*Directory, false, true);
  if (!bDeleted) {
    ErrorMessage = FString::Printf(
        TEXT("Failed to delete source external package directory: %s"),
        *Directory);
    ErrorCode = TEXT("SOURCE_EXTERNAL_DELETE_FAILED");
    return false;
  }
  return true;
}


bool BackupFileForOverwrite(const FString& Filename,
                            const TCHAR* Label,
                            bool& bExisted,
                            FString& BackupFilename,
                            FString& ErrorMessage,
                            FString& ErrorCode) {
  IFileManager& FileManager = IFileManager::Get();
  bExisted = FileManager.FileExists(*Filename);
  BackupFilename.Reset();
  if (!bExisted) {
    return true;
  }

  BackupFilename = FString::Printf(TEXT("%s.mcp_backup_%s"), *Filename,
                                   *FGuid::NewGuid().ToString(EGuidFormats::Digits));
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  if (!PlatformFile.CopyFile(*BackupFilename, *Filename)) {
    ErrorMessage = FString::Printf(TEXT("Failed to back up %s before overwrite: %s"),
                                   Label, *Filename);
    ErrorCode = TEXT("DESTINATION_BACKUP_FAILED");
    BackupFilename.Reset();
    return false;
  }
  if (!FileManager.Delete(*Filename, false, true, true)) {
    FileManager.Delete(*BackupFilename, false, true, true);
    ErrorMessage = FString::Printf(TEXT("Failed to prepare %s for overwrite: %s"),
                                   Label, *Filename);
    ErrorCode = TEXT("DESTINATION_DELETE_FAILED");
    BackupFilename.Reset();
    return false;
  }
  return true;
}

bool RestoreFileBackup(const FString& Filename, const FString& BackupFilename) {
  if (BackupFilename.IsEmpty() ||
      !IFileManager::Get().FileExists(*BackupFilename)) {
    return true;
  }
  IFileManager::Get().Delete(*Filename, false, true, true);
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  const bool bRestored = PlatformFile.CopyFile(*Filename, *BackupFilename);
  if (bRestored) {
    IFileManager::Get().Delete(*BackupFilename, false, true, true);
  }
  return bRestored;
}

void DeleteFileBackup(const FString& BackupFilename) {
  if (!BackupFilename.IsEmpty()) {
    IFileManager::Get().Delete(*BackupFilename, false, true, true);
  }
}

bool BackupDirectoryForOverwrite(const FString& Directory,
                                 const TCHAR* Label,
                                 bool& bExisted,
                                 FString& BackupDirectory,
                                 FString& ErrorMessage,
                                 FString& ErrorCode) {
  IFileManager& FileManager = IFileManager::Get();
  bExisted = FileManager.DirectoryExists(*Directory);
  BackupDirectory.Reset();
  if (!bExisted) {
    return true;
  }

  BackupDirectory = FString::Printf(TEXT("%s_mcp_backup_%s"), *Directory,
                                    *FGuid::NewGuid().ToString(EGuidFormats::Digits));
  FileManager.MakeDirectory(*FPaths::GetPath(BackupDirectory), true);
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  if (!PlatformFile.CopyDirectoryTree(*BackupDirectory, *Directory, false)) {
    ErrorMessage = FString::Printf(TEXT("Failed to back up %s before overwrite: %s"),
                                   Label, *Directory);
    ErrorCode = TEXT("DESTINATION_BACKUP_FAILED");
    BackupDirectory.Reset();
    return false;
  }
  if (!FileManager.DeleteDirectory(*Directory, false, true)) {
    FileManager.DeleteDirectory(*BackupDirectory, false, true);
    ErrorMessage = FString::Printf(TEXT("Failed to prepare %s for overwrite: %s"),
                                   Label, *Directory);
    ErrorCode = TEXT("DESTINATION_DELETE_FAILED");
    BackupDirectory.Reset();
    return false;
  }
  return true;
}

bool RestoreDirectoryBackup(const FString& Directory, const FString& BackupDirectory) {
  if (BackupDirectory.IsEmpty() ||
      !IFileManager::Get().DirectoryExists(*BackupDirectory)) {
    return true;
  }
  IFileManager::Get().DeleteDirectory(*Directory, false, true);
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Directory), true);
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  const bool bRestored = PlatformFile.CopyDirectoryTree(*Directory, *BackupDirectory, false);
  if (bRestored) {
    IFileManager::Get().DeleteDirectory(*BackupDirectory, false, true);
  }
  return bRestored;
}

void DeleteDirectoryBackup(const FString& BackupDirectory) {
  if (!BackupDirectory.IsEmpty()) {
    IFileManager::Get().DeleteDirectory(*BackupDirectory, false, true);
  }
}

bool IsBlockingDirtyPackageForLevelLoad(UPackage* Package) {
  if (!Package || Package->HasAnyFlags(RF_Transient)) {
    return false;
  }

  const FString PackagePath = Package->GetPathName();
  return !PackagePath.StartsWith(TEXT("/Temp/")) &&
         !PackagePath.StartsWith(TEXT("/Transient/")) &&
         !PackagePath.StartsWith(TEXT("/Engine/Transient"));
}

void CountBlockingDirtyPackages(int32& OutWorldPackages,
                                int32& OutContentPackages) {
  OutWorldPackages = 0;
  OutContentPackages = 0;

  TArray<UPackage*> DirtyWorldPackages;
  TArray<UPackage*> DirtyContentPackages;
  FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
  FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);

  TSet<UPackage*> SeenPackages;
  for (UPackage* Package : DirtyWorldPackages) {
    if (IsBlockingDirtyPackageForLevelLoad(Package) && !SeenPackages.Contains(Package)) {
      SeenPackages.Add(Package);
      OutWorldPackages++;
    }
  }
  for (UPackage* Package : DirtyContentPackages) {
    if (IsBlockingDirtyPackageForLevelLoad(Package) && !SeenPackages.Contains(Package)) {
      SeenPackages.Add(Package);
      OutContentPackages++;
    }
  }
}

bool SaveBlockingDirtyPackagesForLevelLoad(int32& OutInitialWorldPackages,
                                           int32& OutInitialContentPackages,
                                           int32& OutRemainingWorldPackages,
                                           int32& OutRemainingContentPackages,
                                           int32& OutFailedPackages) {
  OutFailedPackages = 0;
  CountBlockingDirtyPackages(OutInitialWorldPackages, OutInitialContentPackages);
  if (OutInitialWorldPackages + OutInitialContentPackages == 0) {
    OutRemainingWorldPackages = 0;
    OutRemainingContentPackages = 0;
    return true;
  }

  TArray<UPackage*> DirtyWorldPackages;
  TArray<UPackage*> DirtyContentPackages;
  FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
  FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);

  TSet<UPackage*> ProcessedPackages;
  auto SavePackage = [&ProcessedPackages, &OutFailedPackages](UPackage* Package) {
    if (!IsBlockingDirtyPackageForLevelLoad(Package) || ProcessedPackages.Contains(Package)) {
      return;
    }

    ProcessedPackages.Add(Package);
    const FString PackagePath = Package->GetName();
    UWorld* PackageWorld = UWorld::FindWorldInPackage(Package);
    const bool bSaved = PackageWorld && PackageWorld->PersistentLevel
        ? McpSafeLevelSave(PackageWorld->PersistentLevel, PackagePath)
        : McpSafeAssetSave(Package);
    if (!bSaved) {
      OutFailedPackages++;
    }
  };

  for (UPackage* Package : DirtyWorldPackages) {
    SavePackage(Package);
  }
  for (UPackage* Package : DirtyContentPackages) {
    SavePackage(Package);
  }

  FlushRenderingCommands();
  CountBlockingDirtyPackages(OutRemainingWorldPackages, OutRemainingContentPackages);
  return OutFailedPackages == 0 && OutRemainingWorldPackages + OutRemainingContentPackages == 0;
}

bool CopyLevelMapPackageFile(const FString& SourcePackagePath,
                             const FString& DestinationPackagePath,
                             bool bOverwrite,
                             TSharedPtr<FJsonObject>& Result,
                             FString& ErrorMessage,
                             FString& ErrorCode) {
  FString SourceFilename;
  FString DestinationFilename;
  if (!TryGetAbsoluteMapFilename(SourcePackagePath, SourceFilename)) {
    ErrorMessage = FString::Printf(TEXT("Could not convert source level to filename: %s"), *SourcePackagePath);
    ErrorCode = TEXT("INVALID_SOURCE_PATH");
    return false;
  }
  if (!TryGetAbsoluteMapFilename(DestinationPackagePath, DestinationFilename)) {
    ErrorMessage = FString::Printf(TEXT("Could not convert destination level to filename: %s"), *DestinationPackagePath);
    ErrorCode = TEXT("INVALID_DESTINATION_PATH");
    return false;
  }

  if (!ValidateWritableGameMapPath(SourcePackagePath, SourceFilename,
                                   TEXT("Source"), ErrorMessage, ErrorCode) ||
      !ValidateWritableGameMapPath(DestinationPackagePath, DestinationFilename,
                                   TEXT("Destination"), ErrorMessage, ErrorCode)) {
    return false;
  }
  if (SourceFilename.Equals(DestinationFilename, ESearchCase::IgnoreCase)) {
    ErrorMessage = FString::Printf(
        TEXT("Source and destination levels are identical: %s"), *SourcePackagePath);
    ErrorCode = TEXT("SAME_PATH");
    return false;
  }

  IFileManager& FileManager = IFileManager::Get();
  if (!FileManager.FileExists(*SourceFilename)) {
    ErrorMessage = FString::Printf(TEXT("Source level file not found: %s"), *SourcePackagePath);
    ErrorCode = TEXT("SOURCE_NOT_FOUND");
    return false;
  }

  const FString SourceBuiltDataPackagePath = SourcePackagePath + TEXT("_BuiltData");
  const FString DestinationBuiltDataPackagePath = DestinationPackagePath + TEXT("_BuiltData");
  FString SourceBuiltDataFilename;
  FString DestinationBuiltDataFilename;
  bool bSourceBuiltDataExists = false;
  bool bDestinationBuiltDataExists = false;
  bool bDeletedDestinationBuiltData = false;
  if (FPackageName::TryConvertLongPackageNameToFilename(
          SourceBuiltDataPackagePath, SourceBuiltDataFilename, FPackageName::GetAssetPackageExtension()) &&
      FPackageName::TryConvertLongPackageNameToFilename(
          DestinationBuiltDataPackagePath, DestinationBuiltDataFilename, FPackageName::GetAssetPackageExtension())) {
    SourceBuiltDataFilename = FPaths::ConvertRelativePathToFull(SourceBuiltDataFilename);
    DestinationBuiltDataFilename = FPaths::ConvertRelativePathToFull(DestinationBuiltDataFilename);
    FPaths::NormalizeFilename(SourceBuiltDataFilename);
    FPaths::NormalizeFilename(DestinationBuiltDataFilename);
    bSourceBuiltDataExists = FileManager.FileExists(*SourceBuiltDataFilename);
    bDestinationBuiltDataExists = FileManager.FileExists(*DestinationBuiltDataFilename);
    if (bSourceBuiltDataExists &&
        SourceBuiltDataFilename.Equals(DestinationBuiltDataFilename,
                                       ESearchCase::IgnoreCase)) {
      ErrorMessage = FString::Printf(
          TEXT("Source and destination built data are identical: %s"),
          *SourceBuiltDataPackagePath);
      ErrorCode = TEXT("SAME_PATH");
      return false;
    }
    if (bDestinationBuiltDataExists && !bOverwrite) {
      ErrorMessage = FString::Printf(
          TEXT("Destination built data already exists: %s"),
          *DestinationBuiltDataPackagePath);
      ErrorCode = TEXT("DESTINATION_EXISTS");
      return false;
    }
  }

  FExternalPackageDirectoryCopyPlan ExternalActorsPlan;
  FExternalPackageDirectoryCopyPlan ExternalObjectsPlan;
  if (!BuildExternalPackageDirectoryCopyPlan(
          SourcePackagePath, DestinationPackagePath, TEXT("__ExternalActors__"),
          bOverwrite, ExternalActorsPlan, ErrorMessage, ErrorCode) ||
      !BuildExternalPackageDirectoryCopyPlan(
          SourcePackagePath, DestinationPackagePath, TEXT("__ExternalObjects__"),
          bOverwrite, ExternalObjectsPlan, ErrorMessage, ErrorCode)) {
    return false;
  }

  const bool bDestinationMapExists = FileManager.FileExists(*DestinationFilename);
  bool bDeletedDestinationMap = false;
  if (bDestinationMapExists && !bOverwrite) {
    ErrorMessage = FString::Printf(TEXT("Destination level already exists: %s"), *DestinationPackagePath);
    ErrorCode = TEXT("DESTINATION_EXISTS");
    return false;
  }
  if (bOverwrite && FindPackage(nullptr, *DestinationPackagePath) != nullptr) {
    ErrorMessage = FString::Printf(
        TEXT("Destination level package is loaded and cannot be overwritten: %s"),
        *DestinationPackagePath);
    ErrorCode = TEXT("DESTINATION_LOADED");
    return false;
  }

  FString DestinationMapBackup;
  FString DestinationBuiltDataBackup;
  FString DestinationExternalActorsBackup;
  FString DestinationExternalObjectsBackup;
  auto AppendRollbackFailure = [](FString& RollbackError, const FString& Detail) {
    if (!RollbackError.IsEmpty()) {
      RollbackError += TEXT("; ");
    }
    RollbackError += Detail;
  };
  auto RestoreDestinationBackups = [&](FString& RollbackError) {
    bool bRollbackSucceeded = true;
    if (!RestoreFileBackup(DestinationFilename, DestinationMapBackup)) {
      bRollbackSucceeded = false;
      AppendRollbackFailure(RollbackError,
                            FString::Printf(TEXT("failed to restore destination level backup: %s"),
                                            *DestinationMapBackup));
    }
    if (!RestoreFileBackup(DestinationBuiltDataFilename, DestinationBuiltDataBackup)) {
      bRollbackSucceeded = false;
      AppendRollbackFailure(RollbackError,
                            FString::Printf(TEXT("failed to restore destination built data backup: %s"),
                                            *DestinationBuiltDataBackup));
    }
    if (!RestoreDirectoryBackup(ExternalActorsPlan.DestinationDirectory, DestinationExternalActorsBackup)) {
      bRollbackSucceeded = false;
      AppendRollbackFailure(RollbackError,
                            FString::Printf(TEXT("failed to restore destination external actors backup: %s"),
                                            *DestinationExternalActorsBackup));
    }
    if (!RestoreDirectoryBackup(ExternalObjectsPlan.DestinationDirectory, DestinationExternalObjectsBackup)) {
      bRollbackSucceeded = false;
      AppendRollbackFailure(RollbackError,
                            FString::Printf(TEXT("failed to restore destination external objects backup: %s"),
                                            *DestinationExternalObjectsBackup));
    }
    return bRollbackSucceeded;
  };
  auto RecordRollbackResult = [&](bool bRollbackSucceeded, const FString& RollbackError) {
    if (!Result.IsValid()) {
      Result = McpHandlerUtils::CreateResultObject();
    }
    Result->SetBoolField(TEXT("rollbackSucceeded"), bRollbackSucceeded);
    if (!RollbackError.IsEmpty()) {
      Result->SetStringField(TEXT("rollbackError"), RollbackError);
    }
  };
  auto RollbackCopiedDestinationArtifacts = [&](FString& RollbackError) {
    if (DestinationMapBackup.IsEmpty()) {
      if (!DestinationFilename.IsEmpty()) {
        FileManager.Delete(*DestinationFilename, false, true, true);
      }
    }
    if (DestinationBuiltDataBackup.IsEmpty()) {
      if (!DestinationBuiltDataFilename.IsEmpty()) {
        FileManager.Delete(*DestinationBuiltDataFilename, false, true, true);
      }
    }
    if (DestinationExternalActorsBackup.IsEmpty()) {
      if (!ExternalActorsPlan.DestinationDirectory.IsEmpty()) {
        FileManager.DeleteDirectory(*ExternalActorsPlan.DestinationDirectory, false, true);
      }
    }
    if (DestinationExternalObjectsBackup.IsEmpty()) {
      if (!ExternalObjectsPlan.DestinationDirectory.IsEmpty()) {
        FileManager.DeleteDirectory(*ExternalObjectsPlan.DestinationDirectory, false, true);
      }
    }
    const bool bRollbackSucceeded = RestoreDestinationBackups(RollbackError);
    RecordRollbackResult(bRollbackSucceeded, RollbackError);
    return bRollbackSucceeded;
  };
  auto DeleteDestinationBackups = [&]() {
    DeleteFileBackup(DestinationMapBackup);
    DeleteFileBackup(DestinationBuiltDataBackup);
    DeleteDirectoryBackup(DestinationExternalActorsBackup);
    DeleteDirectoryBackup(DestinationExternalObjectsBackup);
  };

  if (!BackupFileForOverwrite(DestinationFilename, TEXT("destination level"),
                              bDeletedDestinationMap, DestinationMapBackup,
                              ErrorMessage, ErrorCode) ||
      (!DestinationBuiltDataFilename.IsEmpty() && !BackupFileForOverwrite(
                                  DestinationBuiltDataFilename,
                                  TEXT("destination built data"),
                                  bDeletedDestinationBuiltData,
                                  DestinationBuiltDataBackup,
                                  ErrorMessage, ErrorCode)) ||
      !BackupDirectoryForOverwrite(ExternalActorsPlan.DestinationDirectory,
                                   TEXT("destination external actors"),
                                   ExternalActorsPlan.bDeletedDestination,
                                   DestinationExternalActorsBackup,
                                   ErrorMessage, ErrorCode) ||
      !BackupDirectoryForOverwrite(ExternalObjectsPlan.DestinationDirectory,
                                   TEXT("destination external objects"),
                                   ExternalObjectsPlan.bDeletedDestination,
                                   DestinationExternalObjectsBackup,
                                   ErrorMessage, ErrorCode)) {
    FString RollbackError;
    const bool bRollbackSucceeded = RestoreDestinationBackups(RollbackError);
    RecordRollbackResult(bRollbackSucceeded, RollbackError);
    if (!bRollbackSucceeded) {
      ErrorMessage += FString::Printf(TEXT(" Rollback failed: %s"), *RollbackError);
      ErrorCode = TEXT("ROLLBACK_FAILED");
    }
    return false;
  }

  const FString DestinationDir = FPaths::GetPath(DestinationFilename);
  if (!DestinationDir.IsEmpty()) {
    FileManager.MakeDirectory(*DestinationDir, true);
  }

  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  const bool bCopiedMap = PlatformFile.CopyFile(*DestinationFilename, *SourceFilename);
  if (!bCopiedMap) {
    FString RollbackError;
    const bool bRollbackSucceeded = RollbackCopiedDestinationArtifacts(RollbackError);
    ErrorMessage = FString::Printf(TEXT("Failed to copy level file from %s to %s"),
                                   *SourcePackagePath, *DestinationPackagePath);
    if (!bRollbackSucceeded) {
      ErrorMessage += FString::Printf(TEXT(" Rollback failed: %s"), *RollbackError);
    }
    ErrorCode = bRollbackSucceeded ? TEXT("COPY_FAILED") : TEXT("ROLLBACK_FAILED");
    return false;
  }

  bool bCopiedBuiltData = false;
  if (bSourceBuiltDataExists) {
    FileManager.MakeDirectory(*FPaths::GetPath(DestinationBuiltDataFilename), true);
    bCopiedBuiltData = PlatformFile.CopyFile(*DestinationBuiltDataFilename, *SourceBuiltDataFilename);
    if (!bCopiedBuiltData) {
      FString RollbackError;
      const bool bRollbackSucceeded = RollbackCopiedDestinationArtifacts(RollbackError);
      ErrorMessage = FString::Printf(
          TEXT("Failed to copy built data from %s to %s"),
          *SourceBuiltDataPackagePath, *DestinationBuiltDataPackagePath);
      if (!bRollbackSucceeded) {
        ErrorMessage += FString::Printf(TEXT(" Rollback failed: %s"), *RollbackError);
      }
      ErrorCode = bRollbackSucceeded ? TEXT("COPY_FAILED") : TEXT("ROLLBACK_FAILED");
      return false;
    }
  }

  bool bCopiedExternalActors = false;
  if (ExternalActorsPlan.bSourceExists) {
    FileManager.MakeDirectory(*FPaths::GetPath(ExternalActorsPlan.DestinationDirectory), true);
    bCopiedExternalActors = PlatformFile.CopyDirectoryTree(
        *ExternalActorsPlan.DestinationDirectory,
        *ExternalActorsPlan.SourceDirectory, false);
    if (!bCopiedExternalActors) {
      FString RollbackError;
      const bool bRollbackSucceeded = RollbackCopiedDestinationArtifacts(RollbackError);
      ErrorMessage = FString::Printf(
          TEXT("Failed to copy external actors from %s to %s"),
          *ExternalActorsPlan.SourceDirectory,
          *ExternalActorsPlan.DestinationDirectory);
      if (!bRollbackSucceeded) {
        ErrorMessage += FString::Printf(TEXT(" Rollback failed: %s"), *RollbackError);
      }
      ErrorCode = bRollbackSucceeded ? TEXT("COPY_FAILED") : TEXT("ROLLBACK_FAILED");
      return false;
    }
  }
  ExternalActorsPlan.bCopied = bCopiedExternalActors;

  bool bCopiedExternalObjects = false;
  if (ExternalObjectsPlan.bSourceExists) {
    FileManager.MakeDirectory(*FPaths::GetPath(ExternalObjectsPlan.DestinationDirectory), true);
    bCopiedExternalObjects = PlatformFile.CopyDirectoryTree(
        *ExternalObjectsPlan.DestinationDirectory,
        *ExternalObjectsPlan.SourceDirectory, false);
    if (!bCopiedExternalObjects) {
      FString RollbackError;
      const bool bRollbackSucceeded = RollbackCopiedDestinationArtifacts(RollbackError);
      ErrorMessage = FString::Printf(
          TEXT("Failed to copy external objects from %s to %s"),
          *ExternalObjectsPlan.SourceDirectory,
          *ExternalObjectsPlan.DestinationDirectory);
      if (!bRollbackSucceeded) {
        ErrorMessage += FString::Printf(TEXT(" Rollback failed: %s"), *RollbackError);
      }
      ErrorCode = bRollbackSucceeded ? TEXT("COPY_FAILED") : TEXT("ROLLBACK_FAILED");
      return false;
    }
  }
  ExternalObjectsPlan.bCopied = bCopiedExternalObjects;
  DeleteDestinationBackups();

  Result = McpHandlerUtils::CreateResultObject();

  ScanLevelPackagePath(DestinationPackagePath, DestinationFilename);
  const bool bDestinationFileExists = FileManager.FileExists(*DestinationFilename);
  const bool bDestinationPackageExists = FPackageName::DoesPackageExist(DestinationPackagePath);

  if (!Result.IsValid()) {
    Result = McpHandlerUtils::CreateResultObject();
  }
  Result->SetStringField(TEXT("sourcePath"), SourcePackagePath);
  Result->SetStringField(TEXT("destinationPath"), DestinationPackagePath);
  Result->SetStringField(TEXT("sourceFilename"), SourceFilename);
  Result->SetStringField(TEXT("destinationFilename"), DestinationFilename);
  Result->SetBoolField(TEXT("overwrite"), bOverwrite);
  Result->SetBoolField(TEXT("copiedMapFile"), bCopiedMap);
  Result->SetBoolField(TEXT("destinationMapExisted"), bDestinationMapExists);
  Result->SetBoolField(TEXT("deletedDestinationMap"), bDeletedDestinationMap);
  Result->SetBoolField(TEXT("sourceBuiltDataExists"), bSourceBuiltDataExists);
  Result->SetBoolField(TEXT("destinationBuiltDataExisted"), bDestinationBuiltDataExists);
  Result->SetBoolField(TEXT("deletedDestinationBuiltData"), bDeletedDestinationBuiltData);
  Result->SetBoolField(TEXT("copiedBuiltData"), bCopiedBuiltData);
  Result->SetBoolField(TEXT("sourceExternalActorsExists"), ExternalActorsPlan.bSourceExists);
  Result->SetBoolField(TEXT("destinationExternalActorsExisted"), ExternalActorsPlan.bDestinationExists);
  Result->SetBoolField(TEXT("deletedDestinationExternalActors"), ExternalActorsPlan.bDeletedDestination);
  Result->SetBoolField(TEXT("copiedExternalActors"), ExternalActorsPlan.bCopied);
  Result->SetBoolField(TEXT("sourceExternalObjectsExists"), ExternalObjectsPlan.bSourceExists);
  Result->SetBoolField(TEXT("destinationExternalObjectsExisted"), ExternalObjectsPlan.bDestinationExists);
  Result->SetBoolField(TEXT("deletedDestinationExternalObjects"), ExternalObjectsPlan.bDeletedDestination);
  Result->SetBoolField(TEXT("copiedExternalObjects"), ExternalObjectsPlan.bCopied);
  Result->SetBoolField(TEXT("destinationFileExists"), bDestinationFileExists);
  Result->SetBoolField(TEXT("destinationPackageExists"), bDestinationPackageExists);

  if (!bCopiedMap || !bDestinationFileExists) {
    ErrorMessage = FString::Printf(TEXT("Failed to copy level file from %s to %s"),
                                   *SourcePackagePath, *DestinationPackagePath);
    ErrorCode = TEXT("COPY_FAILED");
    return false;
  }

  return true;
}
} // namespace

// UEditorLevelUtils::GetLevels has linker issues; gather levels manually.
static TArray<ULevel*> GetAllLevelsFromWorld(UWorld* World) {
  TArray<ULevel*> Levels;
  if (!World) return Levels;

  // Add persistent level
  if (World->PersistentLevel) {
    Levels.Add(World->PersistentLevel);
  }

  // Add streaming levels
  for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels()) {
    if (StreamingLevel) {
      ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
      if (LoadedLevel) {
        Levels.Add(LoadedLevel);
      }
    }
  }

  return Levels;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelLoad(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  // Map to Open command
  FString LevelPath;
  Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
  bool bSaveDirtyPackages = false;
  Payload->TryGetBoolField(TEXT("saveDirtyPackages"), bSaveDirtyPackages);

  // Determine invalid characters for checks
  if (LevelPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("levelPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // SECURITY: Sanitize LevelPath to prevent path traversal attacks
  FString SanitizedLevelPath = SanitizeProjectRelativePath(LevelPath);
  if (SanitizedLevelPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid levelPath: contains path traversal (..) or invalid characters"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }
  LevelPath = SanitizedLevelPath;

  // Auto-resolve short names
  if (!LevelPath.StartsWith(TEXT("/")) && !FPaths::FileExists(LevelPath)) {
    FString TryPath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelPath);
    if (FPackageName::DoesPackageExist(TryPath)) {
      LevelPath = TryPath;
    }
  }

  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Try to resolve package path to filename
  FString Filename;
  bool bGotFilename = false;
  if (FPackageName::IsPackageFilename(LevelPath)) {
    Filename = LevelPath;
    bGotFilename = true;
  } else {
    // Assume package path
    if (FPackageName::TryConvertLongPackageNameToFilename(
            LevelPath, Filename, FPackageName::GetMapPackageExtension())) {
      bGotFilename = true;
    }
  }

  // If conversion failed, it might be a short name? But LoadMap usually
  // needs full path. Let's try to load what we have if conversion returned
  // something, else fallback to input.
  const FString FileToLoad = bGotFilename ? Filename : LevelPath;
  FString ResolvedFileToLoad = FileToLoad;
  FString ExpectedLoadedPath = LevelPath;

  // Verify file exists before attempting load to avoid false positives
  // CRITICAL: Unreal stores levels in TWO possible path patterns:
  // 1. Folder-based (standard UE 5.x): /Game/Path/LevelName/LevelName.umap
  // 2. Flat (legacy): /Game/Path/LevelName.umap
  // We must check BOTH paths before returning FILE_NOT_FOUND to prevent
  // the "Pure virtual not implemented" crash when LoadMap fails.
  
  FString FilenameToCheck;
  bool bFileExists = false;
  
  // Build both possible paths
  FString FlatMapPath, FullFlatMapPath, FolderMapPath, FullFolderMapPath;
  if (FPackageName::TryConvertLongPackageNameToFilename(
          LevelPath, FlatMapPath, FPackageName::GetMapPackageExtension())) {
    FullFlatMapPath = FPaths::ConvertRelativePathToFull(FlatMapPath);
    
    // Also build folder-based path: /Game/Path/LevelName -> /Game/Path/LevelName/LevelName.umap
    FString LevelName = FPaths::GetBaseFilename(LevelPath);
    FolderMapPath = FPaths::GetPath(FlatMapPath) / LevelName / (LevelName + FPackageName::GetMapPackageExtension());
    FullFolderMapPath = FPaths::ConvertRelativePathToFull(FolderMapPath);
  }
  
  // Check both paths - prefer folder-based (UE 5.x standard)
  if (!FullFolderMapPath.IsEmpty() && IFileManager::Get().FileExists(*FullFolderMapPath)) {
    bFileExists = true;
    ResolvedFileToLoad = FolderMapPath;
    const FString LevelName = FPaths::GetBaseFilename(LevelPath);
    ExpectedLoadedPath = FPaths::GetPath(LevelPath) / LevelName / LevelName;
    UE_LOG(LogTemp, Log, TEXT("load: Found level at folder-based path: %s"), *FullFolderMapPath);
  } else if (!FullFlatMapPath.IsEmpty() && IFileManager::Get().FileExists(*FullFlatMapPath)) {
    bFileExists = true;
    ResolvedFileToLoad = FlatMapPath;
    ExpectedLoadedPath = LevelPath;
    UE_LOG(LogTemp, Log, TEXT("load: Found level at flat path: %s"), *FullFlatMapPath);
  }
  
  // Also check if it's a valid package path (for levels in memory but not on disk yet)
  if (!bFileExists && !FPackageName::DoesPackageExist(LevelPath)) {
    TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
    ErrorDetails->SetStringField(TEXT("levelPath"), LevelPath);
    if (!FullFolderMapPath.IsEmpty()) {
      ErrorDetails->SetStringField(TEXT("checkedFolderBased"), FullFolderMapPath);
    }
    if (!FullFlatMapPath.IsEmpty()) {
      ErrorDetails->SetStringField(TEXT("checkedFlat"), FullFlatMapPath);
    }
    ErrorDetails->SetStringField(TEXT("hint"), TEXT("Unreal levels are typically stored as /Game/Path/LevelName/LevelName.umap"));
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Level file not found. Checked:\n  Folder: %s\n  Flat: %s"), 
                      *FullFolderMapPath, *FullFlatMapPath),
        ErrorDetails, TEXT("FILE_NOT_FOUND"));
    return true;
  }

  // Force any pending work to complete
  FlushRenderingCommands();

  bool bSavedDirtyPackagesBeforeLoad = false;
  int32 DirtyWorldPackagesBeforeLoad = 0;
  int32 DirtyContentPackagesBeforeLoad = 0;
  int32 DirtyWorldPackagesAfterSave = 0;
  int32 DirtyContentPackagesAfterSave = 0;
  int32 FailedDirtyPackageSaves = 0;
  if (FApp::IsUnattended() || IsRunningCommandlet() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) {
    CountBlockingDirtyPackages(DirtyWorldPackagesBeforeLoad, DirtyContentPackagesBeforeLoad);
    if (DirtyWorldPackagesBeforeLoad + DirtyContentPackagesBeforeLoad > 0 && !bSaveDirtyPackages) {
      TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
      ErrorDetails->SetNumberField(TEXT("dirtyWorldPackages"), DirtyWorldPackagesBeforeLoad);
      ErrorDetails->SetNumberField(TEXT("dirtyContentPackages"), DirtyContentPackagesBeforeLoad);
      ErrorDetails->SetBoolField(TEXT("saveDirtyPackages"), bSaveDirtyPackages);
      ErrorDetails->SetStringField(TEXT("levelPath"), LevelPath);
      SendAutomationResponse(
          Socket, RequestId, false,
          TEXT("Cannot load a level in unattended/headless mode while packages are dirty. Pass saveDirtyPackages=true to save them before loading."),
          ErrorDetails, TEXT("DIRTY_PACKAGES"));
      return true;
    }

    if (bSaveDirtyPackages) {
    bSavedDirtyPackagesBeforeLoad = SaveBlockingDirtyPackagesForLevelLoad(
        DirtyWorldPackagesBeforeLoad, DirtyContentPackagesBeforeLoad,
        DirtyWorldPackagesAfterSave, DirtyContentPackagesAfterSave,
        FailedDirtyPackageSaves);
    if (!bSavedDirtyPackagesBeforeLoad) {
      TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
      ErrorDetails->SetNumberField(TEXT("dirtyWorldPackagesBeforeSave"), DirtyWorldPackagesBeforeLoad);
      ErrorDetails->SetNumberField(TEXT("dirtyContentPackagesBeforeSave"), DirtyContentPackagesBeforeLoad);
      ErrorDetails->SetNumberField(TEXT("dirtyWorldPackages"), DirtyWorldPackagesAfterSave);
      ErrorDetails->SetNumberField(TEXT("dirtyContentPackages"), DirtyContentPackagesAfterSave);
      ErrorDetails->SetNumberField(TEXT("failedPackageSaves"), FailedDirtyPackageSaves);
      ErrorDetails->SetBoolField(TEXT("saveDirtyPackagesSucceeded"), bSavedDirtyPackagesBeforeLoad);
      ErrorDetails->SetStringField(TEXT("levelPath"), LevelPath);
      SendAutomationResponse(
          Socket, RequestId, false,
          TEXT("Cannot load a level in unattended/headless mode while packages remain dirty after non-interactive save."),
          ErrorDetails, TEXT("DIRTY_PACKAGES"));
      return true;
    }
    }
  }

  const bool bLoaded = McpSafeLoadMap(ResolvedFileToLoad);

  // Post-load verification: check that the loaded world matches the requested path
  if (bLoaded) {
    UWorld* LoadedWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (LoadedWorld) {
      FString LoadedPath = LoadedWorld->GetOutermost()->GetName();
      // Normalize paths for comparison (handle case differences)
      if (LoadedPath.ToLower() != ExpectedLoadedPath.ToLower()) {
        // The requested level was not actually loaded - engine fell back to default
        SendAutomationResponse(
            Socket, RequestId, false,
            FString::Printf(TEXT("Level path mismatch: requested %s but loaded %s"), *ExpectedLoadedPath, *LoadedPath),
            nullptr, TEXT("LOAD_MISMATCH"));
        return true;
      }
    }
    
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("requestedPath"), LevelPath);
    Resp->SetStringField(TEXT("loadedPath"), ExpectedLoadedPath);
    Resp->SetBoolField(TEXT("saveDirtyPackages"), bSaveDirtyPackages);
    Resp->SetBoolField(TEXT("savedDirtyPackagesBeforeLoad"), bSavedDirtyPackagesBeforeLoad);
    Resp->SetNumberField(TEXT("dirtyWorldPackagesBeforeLoad"), DirtyWorldPackagesBeforeLoad);
    Resp->SetNumberField(TEXT("dirtyContentPackagesBeforeLoad"), DirtyContentPackagesBeforeLoad);
    Resp->SetNumberField(TEXT("dirtyWorldPackagesAfterSave"), DirtyWorldPackagesAfterSave);
    Resp->SetNumberField(TEXT("dirtyContentPackagesAfterSave"), DirtyContentPackagesAfterSave);
    Resp->SetNumberField(TEXT("failedDirtyPackageSaves"), FailedDirtyPackageSaves);
    VerifyAssetExists(Resp, ExpectedLoadedPath);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Level loaded"), Resp, FString());
    return true;
  } else {
    // Fallback to ExecuteConsoleCommand "Open" if LoadMap failed (e.g.
    // maybe it was a raw asset path or something) But actually if LoadMap
    // fails, Open likely fails too.
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Failed to load map: %s"), *LevelPath),
        nullptr, TEXT("LOAD_FAILED"));
    return true;
  }
}

bool UMcpAutomationBridgeSubsystem::HandleLevelSave(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!World) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No world loaded"), nullptr,
                           TEXT("NO_WORLD"));
    return true;
  }

  // CRITICAL: Check if the current level is transient (unsaved/Untitled)
  // Saving a transient level causes fatal error: "Attempted to create a package 
  // with name containing double slashes" when HLOD/Instancing generates paths
  // like /Game//Temp/Untitled_1_HLOD0_Instancing
  FString PackageName = World->GetOutermost()->GetName();
  bool bIsTransient = PackageName.StartsWith(TEXT("/Temp/")) ||
                      PackageName.StartsWith(TEXT("/Engine/Transient")) ||
                      PackageName.Contains(TEXT("Untitled"));
  
  if (bIsTransient) {
    TSharedPtr<FJsonObject> ErrorDetail = McpHandlerUtils::CreateResultObject();
    ErrorDetail->SetStringField(TEXT("attemptedPath"), PackageName);
    ErrorDetail->SetStringField(TEXT("reason"), 
        TEXT("Level is unsaved/temporary. Use save_as with a valid path first."));
    ErrorDetail->SetStringField(TEXT("hint"), 
        TEXT("Use manage_level with action='save_as' and provide savePath parameter"));
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Cannot save transient level: Level must be saved with 'save_as' first"),
        ErrorDetail, TEXT("TRANSIENT_LEVEL"));
    return true;
  }

  // Use McpSafeLevelSave to prevent Intel GPU driver crashes during save
  // FlushRenderingCommands prevents MONZA DdiThreadingContext exceptions
  // Explicitly use 5 retries for Intel GPU resilience (max 7.75s total retry time)
  bool bSaved = McpSafeLevelSave(World->PersistentLevel, PackageName);
  if (bSaved) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    FString LevelPath = World->GetOutermost()->GetName();
    VerifyAssetExists(Resp, LevelPath);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Level saved"), Resp, FString());
  } else {
    // Provide detailed error information
    TSharedPtr<FJsonObject> ErrorDetail = McpHandlerUtils::CreateResultObject();
    ErrorDetail->SetStringField(TEXT("attemptedPath"), PackageName);

    FString Filename;
    FString ErrorReason = TEXT("Unknown save failure");

    // Transient level check already handled above, so this is for other save failures
    if (FPackageName::TryConvertLongPackageNameToFilename(
                   PackageName, Filename,
                   FPackageName::GetMapPackageExtension())) {
      if (IFileManager::Get().IsReadOnly(*Filename)) {
        ErrorReason = TEXT("File is read-only or locked by another process");
        ErrorDetail->SetStringField(TEXT("filename"), Filename);
      } else if (!IFileManager::Get().DirectoryExists(
                     *FPaths::GetPath(Filename))) {
        ErrorReason = TEXT("Target directory does not exist");
        ErrorDetail->SetStringField(TEXT("directory"),
                                    FPaths::GetPath(Filename));
      } else {
        ErrorReason =
            TEXT("Save operation failed - check Output Log for details");
        ErrorDetail->SetStringField(TEXT("filename"), Filename);
      }
    } else {
      ErrorReason = TEXT("Invalid package path");
    }

    ErrorDetail->SetStringField(TEXT("reason"), ErrorReason);
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Failed to save level: %s"), *ErrorReason),
        ErrorDetail, TEXT("SAVE_FAILED"));
  }
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelSaveAs(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  // Force cleanup to prevent potential deadlocks with HLODs/WorldPartition
  // during save
  if (GEditor) {
    FlushRenderingCommands();
    GEditor->ForceGarbageCollection(true);
    FlushRenderingCommands();
  }

  FString SavePath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
  if (SavePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("savePath required for save_as"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  SavePath = SanitizeProjectRelativePath(SavePath);
  if (SavePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid savePath: contains path traversal (..) or invalid characters"),
                           nullptr, TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // CRITICAL: Validate path length BEFORE attempting save to prevent silent hangs
  // McpSafeLevelSave validates internally but may not send error response in all code paths
  {
    FString AbsoluteFilePath;
    if (FPackageName::TryConvertLongPackageNameToFilename(SavePath, AbsoluteFilePath, FPackageName::GetMapPackageExtension()))
    {
      AbsoluteFilePath = FPaths::ConvertRelativePathToFull(AbsoluteFilePath);
      const int32 SafePathLength = 240;
      if (AbsoluteFilePath.Len() > SafePathLength)
      {
        TSharedPtr<FJsonObject> ErrorDetail = McpHandlerUtils::CreateResultObject();
        ErrorDetail->SetStringField(TEXT("attemptedPath"), SavePath);
        ErrorDetail->SetStringField(TEXT("absolutePath"), AbsoluteFilePath);
        ErrorDetail->SetNumberField(TEXT("pathLength"), AbsoluteFilePath.Len());
        ErrorDetail->SetNumberField(TEXT("maxLength"), SafePathLength);
        SendAutomationResponse(
            Socket, RequestId, false,
            FString::Printf(TEXT("Path too long (%d chars, max %d): %s"), 
                AbsoluteFilePath.Len(), SafePathLength, *SavePath),
            ErrorDetail, TEXT("PATH_TOO_LONG"));
        return true;
      }
    }
  }

#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
  if (ULevelEditorSubsystem *LevelEditorSS =
          GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) {
    bool bSaved = false;
#if __has_include("FileHelpers.h")
    if (UWorld *World = GEditor->GetEditorWorldContext().World()) {
      // Use McpSafeLevelSave to prevent Intel GPU driver crashes
      // Explicitly use 5 retries for Intel GPU resilience (max 7.75s total retry time)
      bSaved = McpSafeLevelSave(World->PersistentLevel, SavePath);
    }
#endif
    if (bSaved) {
      // Refresh Asset Registry so the saved level is immediately visible for rename/duplicate operations
      IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
      FString SavedFilename;
      if (FPackageName::TryConvertLongPackageNameToFilename(SavePath, SavedFilename, FPackageName::GetMapPackageExtension())) {
        TArray<FString> FilesToScan;
        FilesToScan.Add(SavedFilename);
        AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
      }

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("levelPath"), SavePath);
      SendAutomationResponse(
          Socket, RequestId, true,
          FString::Printf(TEXT("Level saved as %s"), *SavePath), Resp,
          FString());
    } else {
      // CRITICAL FIX: Send error response when save fails (was missing before, causing silent hangs)
      TSharedPtr<FJsonObject> ErrorDetail = McpHandlerUtils::CreateResultObject();
      ErrorDetail->SetStringField(TEXT("attemptedPath"), SavePath);
      ErrorDetail->SetStringField(TEXT("reason"), TEXT("Save operation failed - check Output Log for details"));
      SendAutomationResponse(
          Socket, RequestId, false,
          FString::Printf(TEXT("Failed to save level as: %s"), *SavePath),
          ErrorDetail, TEXT("SAVE_FAILED"));
    }
    return true;
  }
#endif
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("LevelEditorSubsystem not available"), nullptr,
                         TEXT("SUBSYSTEM_MISSING"));
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelCreate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString LevelName;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("levelName"), LevelName);

  // SECURITY: Sanitize LevelName to prevent path injection
  // Remove any path separators (only allow the final name component)
  // and reject traversal sequences
  if (!LevelName.IsEmpty()) {
    int32 LastSlash = -1;
    LevelName.FindLastChar(TEXT('/'), LastSlash);
    if (LastSlash >= 0) {
      LevelName = LevelName.RightChop(LastSlash + 1);
    }
    LevelName.FindLastChar(TEXT('\\'), LastSlash);
    if (LastSlash >= 0) {
      LevelName = LevelName.RightChop(LastSlash + 1);
    }
    if (LevelName.Contains(TEXT(".."))) {
      SendAutomationResponse(
          Socket, RequestId, false,
          TEXT("Invalid levelName: contains path traversal (..)"),
          nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
  }

  FString LevelPath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("levelPath"), LevelPath);

  // Parse useWorldPartition - default to false for faster level creation
  // World Partition levels take 20+ seconds to unload in UE 5.7
  bool bUseWorldPartition = false;
  if (Payload.IsValid()) {
    Payload->TryGetBoolField(TEXT("useWorldPartition"), bUseWorldPartition);
  }

  // SECURITY: Sanitize LevelPath to prevent path traversal attacks
  // Rejects paths containing "..", double slashes, or invalid characters
  // that could cause engine crashes or security violations
  FString SanitizedLevelPath = SanitizeProjectRelativePath(LevelPath);
  if (!LevelPath.IsEmpty() && SanitizedLevelPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Invalid levelPath: contains path traversal (..), double slashes, or invalid characters"),
        nullptr, TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // CRITICAL FIX: Properly combine levelPath (parent directory) and levelName
  // If both are provided, levelPath is the parent directory and levelName is the level name
  // If only levelName is provided and it starts with '/', it's treated as a full path
  // If only levelPath is provided, it's treated as a full path (backwards compatibility)
  FString SavePath;
  
  if (!SanitizedLevelPath.IsEmpty() && !LevelName.IsEmpty()) {
    // Both provided: levelPath is parent directory, levelName is the level name
    // Combine them: /Game/MCPTest + TestLevel = /Game/MCPTest/TestLevel
    SavePath = SanitizedLevelPath;
    if (!SavePath.EndsWith(TEXT("/"))) {
      SavePath += TEXT("/");
    }
    SavePath += LevelName;
  } else if (!LevelName.IsEmpty()) {
    // Only levelName provided
    if (LevelName.StartsWith(TEXT("/"))) {
      // levelName is actually a full path
      SavePath = LevelName;
    } else {
      // Just the name - save to default location
      SavePath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelName);
    }
  } else if (!SanitizedLevelPath.IsEmpty()) {
    // Only levelPath provided - treat as full path (backwards compatibility)
    SavePath = SanitizedLevelPath;
  }

  if (SavePath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("levelName or levelPath required for create_level"), nullptr,
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Check if map already exists
  if (FPackageName::DoesPackageExist(SavePath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("levelPath"), SavePath);
    Resp->SetStringField(TEXT("packagePath"), SavePath);
    Resp->SetBoolField(TEXT("alreadyExists"), true);
    const bool bLoaded = McpSafeLoadMap(SavePath, true);
    Resp->SetBoolField(TEXT("loaded"), bLoaded);
    if (bLoaded && GEditor && GEditor->GetEditorWorldContext().World()) {
      UWorld* LoadedWorld = GEditor->GetEditorWorldContext().World();
      if (LoadedWorld && LoadedWorld->GetOutermost()) {
        Resp->SetStringField(TEXT("currentLevelPath"), LoadedWorld->GetOutermost()->GetName());
      }
    }
    SendAutomationResponse(
        Socket, RequestId, bLoaded,
        bLoaded ? FString::Printf(TEXT("Level already exists and was loaded: %s"), *SavePath)
                : FString::Printf(TEXT("Level already exists but could not be loaded: %s"), *SavePath),
        Resp, bLoaded ? FString() : TEXT("LOAD_FAILED"));
    return true;
  }

  // UE 5.7: GEditor->NewMap can assert while destroying the current editor
  // world if TickTaskManager still tracks a level from a previous automation
  // map transition. The manage_level_structure create_level path creates and
  // saves an inactive UWorld package without switching the editor world, so it
  // avoids EditorDestroyWorld/NewMap entirely while still producing a real
  // level asset that manage_level load/stream/export actions can use.
  TSharedPtr<FJsonObject> CreatePayload = MakeShared<FJsonObject>();
  CreatePayload->SetStringField(TEXT("subAction"), TEXT("create_level"));
  CreatePayload->SetStringField(TEXT("levelName"), FPaths::GetBaseFilename(SavePath));
  CreatePayload->SetStringField(TEXT("levelPath"), FPaths::GetPath(SavePath));
  CreatePayload->SetBoolField(TEXT("bCreateWorldPartition"), bUseWorldPartition);
  CreatePayload->SetBoolField(TEXT("save"), true);
  CreatePayload->SetBoolField(TEXT("loadAfterCreate"), true);
  return HandleManageLevelStructureAction(RequestId, TEXT("manage_level_structure"), CreatePayload, Socket);
}

bool UMcpAutomationBridgeSubsystem::HandleLevelStreamInternal(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket, bool bForceUnload) {
  FString LevelName;
  bool bLoad = bForceUnload ? false : true;
  bool bVis = bForceUnload ? false : true;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("levelName"), LevelName);
    Payload->TryGetBoolField(TEXT("shouldBeLoaded"), bLoad);
    Payload->TryGetBoolField(TEXT("shouldBeVisible"), bVis);
    if (LevelName.IsEmpty())
      Payload->TryGetStringField(TEXT("levelPath"), LevelName);
  }
  if (bForceUnload) {
    bLoad = false;
    bVis = false;
  }
  if (LevelName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("levelName or levelPath required"), nullptr,
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // CRITICAL FIX: Use UEditorLevelUtils for streaming instead of console command
  // Console command StreamLevel is unreliable and returns EXEC_FAILED in many cases
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld* World = GEditor->GetEditorWorldContext().World();
  if (!World) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No world loaded"), nullptr,
                           TEXT("NO_WORLD"));
    return true;
  }

  // Find the streaming level by name/path
  ULevelStreaming* TargetStreamingLevel = nullptr;
  FString NormalizedLevelName = LevelName;
  
  // Normalize the path - remove .umap extension if present
  if (NormalizedLevelName.EndsWith(TEXT(".umap"))) {
    NormalizedLevelName = NormalizedLevelName.LeftChop(5);
  }
  
  // Search for the streaming level
  for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels()) {
    if (StreamingLevel) {
      FString StreamingName = StreamingLevel->GetWorldAssetPackageName();
      if (StreamingName.Equals(NormalizedLevelName, ESearchCase::IgnoreCase) ||
          StreamingName.EndsWith(NormalizedLevelName, ESearchCase::IgnoreCase) ||
          FPaths::GetBaseFilename(StreamingName).Equals(NormalizedLevelName, ESearchCase::IgnoreCase)) {
        TargetStreamingLevel = StreamingLevel;
        break;
      }
    }
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("levelName"), NormalizedLevelName);
  Result->SetBoolField(TEXT("shouldBeLoaded"), bLoad);
  Result->SetBoolField(TEXT("shouldBeVisible"), bVis);

  if (TargetStreamingLevel) {
    // Use the streaming level API directly
    TargetStreamingLevel->SetShouldBeLoaded(bLoad);
    TargetStreamingLevel->SetShouldBeVisible(bVis);
    
    Result->SetStringField(TEXT("streamingState"), 
        TargetStreamingLevel->IsStreamingStatePending() ? TEXT("Pending") :
        TargetStreamingLevel->IsLevelLoaded() ? TEXT("Loaded") : TEXT("Unloaded"));
    
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Streaming level state updated: %s (Loaded=%s, Visible=%s)"),
                               *NormalizedLevelName, 
                               bLoad ? TEXT("true") : TEXT("false"),
                               bVis ? TEXT("true") : TEXT("false")),
                           Result);
  } else {
    // Streaming level not found - try console command as fallback
    if (!IsSafeLevelConsoleToken(NormalizedLevelName)) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Invalid streaming level name"), Result,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    const FString Cmd =
        FString::Printf(TEXT("StreamLevel %s %s %s"), *NormalizedLevelName,
                        bLoad ? TEXT("Load") : TEXT("Unload"),
                        bVis ? TEXT("Show") : TEXT("Hide"));
    
    // Execute console command and check result
    bool bCmdSuccess = false;
    if (World) {
      bCmdSuccess = GEditor->Exec(World, *Cmd);
    }
    
    if (bCmdSuccess) {
      Result->SetStringField(TEXT("method"), TEXT("console_command"));
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Streaming command executed"), Result);
    } else {
      // No ULevelStreaming was located in the world and the StreamLevel Exec
      // did not handle the command — nothing was actually streamed. Fail loudly
      // rather than reporting a phantom success.
      Result->SetStringField(TEXT("method"), TEXT("console_command_fallback"));
      Result->SetStringField(TEXT("command"), Cmd);
      SendAutomationResponse(
          Socket, RequestId, false,
          FString::Printf(
              TEXT("Streaming level not found in world and StreamLevel command failed: %s"),
              *NormalizedLevelName),
          Result, TEXT("LEVEL_NOT_FOUND"));
    }
  }
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelStream(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  return HandleLevelStreamInternal(RequestId, Payload, Socket, false);
}

bool UMcpAutomationBridgeSubsystem::HandleLevelUnload(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  return HandleLevelStreamInternal(RequestId, Payload, Socket, true);
}

bool UMcpAutomationBridgeSubsystem::HandleLevelCreateLight(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  return HandleLightingAction(RequestId, TEXT("create_light"), Payload, Socket);
}

bool UMcpAutomationBridgeSubsystem::HandleLevelBuildLighting(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  // FEditorBuildUtils::EditorBuild is async — returns immediately while
  // the lighting build runs in the background, avoiding 30s timeout.
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("buildStarted"), true);
  if (Payload.IsValid()) {
    FString Q;
    if (Payload->TryGetStringField(TEXT("quality"), Q) && !Q.IsEmpty())
      Result->SetStringField(TEXT("quality"), Q);
  }
  FlushRenderingCommands();
  if (GEditor && GEditor->GetEditorWorldContext().World()) {
    FEditorBuildUtils::EditorBuild(GEditor->GetEditorWorldContext().World(), FBuildOptions::BuildLighting);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Lighting build started (runs in background)"), Result);
  } else {
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Lighting build requested (no active world)"), Result);
  }
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelSetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  TSharedPtr<FJsonObject> AssetPayload = MakeShared<FJsonObject>();
  FString AssetPath;
  if (Payload.IsValid()) {
    AssetPayload->Values = Payload->Values;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("levelPath"), AssetPath);
    }
  }

  AssetPath = NormalizeLevelPackagePath(AssetPath);
  FString MapFilename;
  FString ErrorMessage;
  FString ErrorCode;
  if (AssetPath.IsEmpty() || !TryGetAbsoluteMapFilename(AssetPath, MapFilename) ||
      !ValidateWritableGameMapPath(AssetPath, MapFilename, TEXT("Metadata"),
                                   ErrorMessage, ErrorCode)) {
    if (ErrorMessage.IsEmpty()) {
      ErrorMessage = FString::Printf(TEXT("metadata target must be a /Game level path: %s"), *AssetPath);
      ErrorCode = TEXT("SECURITY_VIOLATION");
    }
    SendAutomationResponse(Socket, RequestId, false, ErrorMessage,
                           nullptr, ErrorCode);
    return true;
  }
  if (!IFileManager::Get().FileExists(*MapFilename) &&
      !FPackageName::DoesPackageExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Level not found: %s"), *AssetPath),
                           nullptr, TEXT("NOT_FOUND"));
    return true;
  }

  AssetPayload->SetStringField(TEXT("assetPath"), AssetPath);
  return HandleSetMetadata(RequestId, AssetPayload, Socket);
}

bool UMcpAutomationBridgeSubsystem::HandleLevelValidate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString LevelPath;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
    if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("assetPath"), LevelPath);
  }
  if (LevelPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("levelPath required for validate_level"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  LevelPath = NormalizeLevelPackagePath(LevelPath);
  LevelPath = SanitizeProjectRelativePath(LevelPath);
  if (LevelPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid levelPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  FString MapFilename;
  const bool bHasFilename = TryGetAbsoluteMapFilename(LevelPath, MapFilename);
  const bool bFileExists = bHasFilename && IFileManager::Get().FileExists(*MapFilename);
  if (bHasFilename) {
    ScanLevelPackagePath(LevelPath, MapFilename);
  }
  const bool bPackageExists = FPackageName::DoesPackageExist(LevelPath);
  const bool bExists = bPackageExists || bFileExists;

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bExists);
  Result->SetBoolField(TEXT("exists"), bExists);
  Result->SetBoolField(TEXT("isValid"), bExists);
  Result->SetStringField(TEXT("levelPath"), LevelPath);
  if (!MapFilename.IsEmpty()) {
    Result->SetStringField(TEXT("mapFilename"), MapFilename);
  }
  Result->SetBoolField(TEXT("packageExists"), bPackageExists);
  Result->SetBoolField(TEXT("fileExists"), bFileExists);

  SendAutomationResponse(Socket, RequestId, bExists,
                         bExists ? TEXT("Level validated") : TEXT("Level not found"),
                         Result, bExists ? FString() : TEXT("NOT_FOUND"));
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelList(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> LevelsArray;

  UWorld *World =
      GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

  // Add current persistent level
  if (World) {
    TSharedPtr<FJsonObject> CurrentLevel = McpHandlerUtils::CreateResultObject();
    CurrentLevel->SetStringField(TEXT("name"), World->GetMapName());
    CurrentLevel->SetStringField(TEXT("path"),
                                 World->GetOutermost()->GetName());
    CurrentLevel->SetBoolField(TEXT("isPersistent"), true);
    CurrentLevel->SetBoolField(TEXT("isLoaded"), true);
    CurrentLevel->SetBoolField(TEXT("isVisible"), true);
    LevelsArray.Add(MakeShared<FJsonValueObject>(CurrentLevel));

    // Add streaming levels
    for (const ULevelStreaming *StreamingLevel :
         World->GetStreamingLevels()) {
      if (!StreamingLevel)
        continue;

      TSharedPtr<FJsonObject> LevelEntry = McpHandlerUtils::CreateResultObject();
      LevelEntry->SetStringField(TEXT("name"),
                                 StreamingLevel->GetWorldAssetPackageName());
      LevelEntry->SetStringField(
          TEXT("path"),
          StreamingLevel->GetWorldAssetPackageFName().ToString());
      LevelEntry->SetBoolField(TEXT("isPersistent"), false);
      LevelEntry->SetBoolField(TEXT("isLoaded"),
                               StreamingLevel->IsLevelLoaded());
      LevelEntry->SetBoolField(TEXT("isVisible"),
                               StreamingLevel->IsLevelVisible());
      LevelEntry->SetStringField(
          TEXT("streamingState"),
          StreamingLevel->IsStreamingStatePending() ? TEXT("Pending")
          : StreamingLevel->IsLevelLoaded()         ? TEXT("Loaded")
                                                    : TEXT("Unloaded"));
      LevelsArray.Add(MakeShared<FJsonValueObject>(LevelEntry));
    }
  }

  // Also query Asset Registry for all map assets
  IAssetRegistry &AssetRegistry =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry")
          .Get();
  TArray<FAssetData> MapAssets;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  AssetRegistry.GetAssetsByClass(
      FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("World")), MapAssets,
      false);
#else
  // UE 5.0: Use FName for class path
  AssetRegistry.GetAssetsByClass(
      FName(TEXT("World")), MapAssets,
      false);
#endif

  TArray<TSharedPtr<FJsonValue>> AllMapsArray;
  for (const FAssetData &MapAsset : MapAssets) {
    TSharedPtr<FJsonObject> MapEntry = McpHandlerUtils::CreateResultObject();
    MapEntry->SetStringField(TEXT("name"), MapAsset.AssetName.ToString());
    MapEntry->SetStringField(TEXT("path"), MapAsset.PackageName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    MapEntry->SetStringField(TEXT("objectPath"),
                             MapAsset.GetObjectPathString());
#else
    // UE 5.0: Construct object path from package and asset name
    MapEntry->SetStringField(TEXT("objectPath"),
                             FString::Printf(TEXT("%s.%s"), *MapAsset.PackageName.ToString(), *MapAsset.AssetName.ToString()));
#endif
    AllMapsArray.Add(MakeShared<FJsonValueObject>(MapEntry));
  }

  Resp->SetArrayField(TEXT("currentWorldLevels"), LevelsArray);
  Resp->SetNumberField(TEXT("currentWorldLevelCount"), LevelsArray.Num());
  Resp->SetArrayField(TEXT("allMaps"), AllMapsArray);
  Resp->SetNumberField(TEXT("allMapsCount"), AllMapsArray.Num());

  if (World) {
    Resp->SetStringField(TEXT("currentMap"), World->GetMapName());
    Resp->SetStringField(TEXT("currentMapPath"),
                         World->GetOutermost()->GetName());
  }

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Levels listed"), Resp, FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelGetCurrent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!EditorWorld) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
    return true;
  }

  ULevel* CurrentLevel = EditorWorld->GetCurrentLevel();
  if (!CurrentLevel) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No current level available"), nullptr, TEXT("NO_LEVEL"));
    return true;
  }

  UPackage* WorldPackage = EditorWorld->GetOutermost();
  UPackage* LevelPackage = CurrentLevel->GetOutermost();

  auto WorldTypeToString = [](EWorldType::Type WorldType) -> FString {
    switch (WorldType) {
    case EWorldType::Game:
      return TEXT("Game");
    case EWorldType::Editor:
      return TEXT("Editor");
    case EWorldType::PIE:
      return TEXT("PIE");
    case EWorldType::EditorPreview:
      return TEXT("EditorPreview");
    case EWorldType::GamePreview:
      return TEXT("GamePreview");
    case EWorldType::GameRPC:
      return TEXT("GameRPC");
    case EWorldType::Inactive:
      return TEXT("Inactive");
    case EWorldType::None:
    default:
      return TEXT("None");
    }
  };

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("mapName"), EditorWorld->GetMapName());
  Result->SetStringField(TEXT("mapPath"), WorldPackage ? WorldPackage->GetName() : TEXT(""));
  Result->SetStringField(TEXT("levelName"), CurrentLevel->GetName());
  Result->SetStringField(TEXT("levelPath"), LevelPackage ? LevelPackage->GetName() : TEXT(""));
  // Include editor-world identity separately from the map package so agents
  // can distinguish persistent map state from transient PIE/editor worlds.
  Result->SetStringField(TEXT("editorWorldName"), EditorWorld->GetName());
  Result->SetStringField(TEXT("editorWorldPath"), WorldPackage ? WorldPackage->GetPathName() : TEXT(""));
  Result->SetStringField(TEXT("worldType"), WorldTypeToString(EditorWorld->WorldType));
  Result->SetNumberField(TEXT("actorCount"), CurrentLevel->Actors.Num());
  Result->SetBoolField(TEXT("isPersistentLevel"), CurrentLevel == EditorWorld->PersistentLevel);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Current level retrieved"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelExport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString LevelPath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
  FString ExportPath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("exportPath"), ExportPath);
  if (ExportPath.IsEmpty())
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("destinationPath"), ExportPath);

  if (ExportPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("exportPath required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // SECURITY: Sanitize export path as an asset path
  FString SafeExportPath = NormalizeLevelPackagePath(SanitizeProjectRelativePath(ExportPath));
  if (SafeExportPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid or unsafe exportPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *WorldToExport = nullptr;
  if (!LevelPath.IsEmpty()) {
    // CRITICAL FIX: Validate source level exists before export
    if (!FPackageName::DoesPackageExist(LevelPath)) {
      // Also check file on disk as fallback
      FString Filename;
      bool bFileFound = false;
      if (FPackageName::TryConvertLongPackageNameToFilename(
              LevelPath, Filename, FPackageName::GetMapPackageExtension())) {
        bFileFound = IFileManager::Get().FileExists(*Filename);
      }
      if (!bFileFound) {
        SendAutomationResponse(Socket, RequestId, false,
                               FString::Printf(TEXT("Source level not found: %s"), *LevelPath),
                               nullptr, TEXT("LEVEL_NOT_FOUND"));
        return true;
      }
    }
    UWorld *Current = GEditor->GetEditorWorldContext().World();
    if (Current && (Current->GetOutermost()->GetName() == LevelPath ||
                    Current->GetPathName() == LevelPath)) {
      WorldToExport = Current;
    } else {
      SendAutomationResponse(
          Socket, RequestId, false,
          FString::Printf(
              TEXT("Requested level is not loaded: %s. Load the level before exporting it."),
              *LevelPath),
          nullptr, TEXT("LEVEL_NOT_LOADED"));
      return true;
    }
  }
  if (!WorldToExport)
    WorldToExport = GEditor->GetEditorWorldContext().World();

  if (!WorldToExport) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No world loaded"), nullptr,
                           TEXT("NO_WORLD"));
    return true;
  }

  FString AbsoluteFilePath;
  FString ExportErrorMessage;
  FString ExportErrorCode;
  if (!TryResolveWritableGameMapFilename(SafeExportPath, AbsoluteFilePath,
                                         ExportErrorMessage,
                                         ExportErrorCode,
                                         TEXT("Export destination"))) {
    SendAutomationResponse(Socket, RequestId, false,
                           ExportErrorMessage, nullptr,
                           ExportErrorCode.IsEmpty() ? TEXT("INVALID_ARGUMENT") : ExportErrorCode);
    return true;
  }

  // Ensure directory only after the export destination is validated as a
  // writable /Game map inside the project Content directory.
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteFilePath), true);

  // CRITICAL: Use McpSafeLevelSave instead of FEditorFileUtils::SaveMap
  // to prevent Intel GPU driver crashes (MONZA DdiThreadingContext)
  bool bExported = McpSafeLevelSave(WorldToExport->PersistentLevel, SafeExportPath);
  if (bExported) {
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Level exported"), nullptr);
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to export level after 5 retries (check GPU driver stability)"), nullptr,
                           TEXT("EXPORT_FAILED"));
  }
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelImport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString DestinationPath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  FString SourcePath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
  if (SourcePath.IsEmpty())
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("packagePath"), SourcePath); // Mapping

  if (SourcePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath/packagePath required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // If SourcePath is a package (starts with /Game), handle as Duplicate/Copy
  if (SourcePath.StartsWith(TEXT("/"))) {
    if (DestinationPath.IsEmpty()) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("destinationPath required for asset copy"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    SourcePath = NormalizeLevelPackagePath(SanitizeProjectRelativePath(SourcePath));
    DestinationPath = NormalizeLevelPackagePath(SanitizeProjectRelativePath(DestinationPath));
    if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Invalid sourcePath or destinationPath"),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }

    bool bOverwrite = false;
    if (Payload.IsValid()) {
      Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);
    }

    FString DestinationFilename;
    const bool bDestinationFileExists =
        TryGetAbsoluteMapFilename(DestinationPath, DestinationFilename) &&
        IFileManager::Get().FileExists(*DestinationFilename);
    if (!bOverwrite && (bDestinationFileExists || FPackageName::DoesPackageExist(DestinationPath))) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("sourcePath"), SourcePath);
      Result->SetStringField(TEXT("destinationPath"), DestinationPath);
      Result->SetBoolField(TEXT("alreadyExists"), true);
      SendAutomationResponse(Socket, RequestId, true,
                             FString::Printf(TEXT("Destination already exists: %s"), *DestinationPath), Result);
      return true;
    }

    TSharedPtr<FJsonObject> Result;
    FString ErrorMessage;
    FString ErrorCode;
    const bool bCopied = CopyLevelMapPackageFile(SourcePath, DestinationPath,
                                                 bOverwrite, Result,
                                                 ErrorMessage, ErrorCode);
    if (bCopied) {
      Result->SetBoolField(TEXT("imported"), true);
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Level imported (copied)"), Result);
    } else {
      SendAutomationResponse(Socket, RequestId, false, ErrorMessage,
                             Result,
                             ErrorCode.IsEmpty() ? TEXT("IMPORT_FAILED") : ErrorCode);
    }
    return true;
  }

  // If SourcePath is file, try Import
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  FString DestPath = DestinationPath.IsEmpty()
                         ? TEXT("/Game/Maps")
                         : FPaths::GetPath(DestinationPath);
  FString DestName = FPaths::GetBaseFilename(
      DestinationPath.IsEmpty() ? SourcePath : DestinationPath);

  TArray<FString> Files;
  Files.Add(SourcePath);
  // FEditorFileUtils::Import(DestPath, DestName); // Ambiguous/Removed
  // Use GEditor->ImportMap or handle via AssetTools
  // Simple fallback:
  if (GEditor) {
    // ImportMap is usually for T3D. If SourcePath is .umap, we should
    // Copy/Load. Assuming T3D import or similar:
    // GEditor->ImportMap(*DestPath, *DestName, *SourcePath);
    // ImportMap is deprecated/removed. For .umap files, manual import or Copy
    // is preferred.
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Direct map file import not supported. Use "
                                "import_level with a package path to copy."),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
  }
  // Automation of Import is tricky without a factory wrapper.
  // Use AssetTools Import.

  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("File-based level import not fully automatic yet"), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelAddSublevel(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString SubLevelPath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("subLevelPath"), SubLevelPath);
  if (SubLevelPath.IsEmpty() && Payload.IsValid())
    Payload->TryGetStringField(TEXT("levelPath"), SubLevelPath);

  if (SubLevelPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("subLevelPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  SubLevelPath = SanitizeProjectRelativePath(SubLevelPath);
  if (SubLevelPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid subLevelPath"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Robustness: Cleanup before adding
  if (GEditor) {
    GEditor->ForceGarbageCollection(true);
  }

  // Verify file existence (more robust than DoesPackageExist for new files)
  FString Filename;
  bool bFileFound = false;
  if (FPackageName::TryConvertLongPackageNameToFilename(
          SubLevelPath, Filename, FPackageName::GetMapPackageExtension())) {
    if (IFileManager::Get().FileExists(*Filename)) {
      bFileFound = true;
    }
  }

  // Fallback: Check without conversion if it's already a file path?
  if (!bFileFound && IFileManager::Get().FileExists(*SubLevelPath)) {
    bFileFound = true;
  }

  if (!bFileFound) {
    // Try checking DoesPackageExist as last resort
    if (!FPackageName::DoesPackageExist(SubLevelPath)) {
      SendAutomationResponse(
          Socket, RequestId, false,
          FString::Printf(TEXT("Level file not found: %s"), *SubLevelPath),
          nullptr, TEXT("PACKAGE_NOT_FOUND"));
      return true;
    }
  }

  FString StreamingMethod = TEXT("Blueprint");
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("streamingMethod"), StreamingMethod);

  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor unavailable"), nullptr,
                           TEXT("NO_EDITOR"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!World) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No world loaded"), nullptr,
                           TEXT("NO_WORLD"));
    return true;
  }

  // CRITICAL FIX: Check if sublevel is already in the world BEFORE trying to add it
  // This prevents "A level with that name already exists in the world" modal dialog
  // which blocks execution and causes test timeouts
  // BUT: Also check if the existing level is actually loaded/valid
  for (ULevelStreaming* ExistingStreamingLevel : World->GetStreamingLevels()) {
    if (ExistingStreamingLevel) {
      FString ExistingPath = ExistingStreamingLevel->GetWorldAssetPackageName();
      // Compare normalized paths (without .umap extension)
      FString NormalizedExisting = ExistingPath;
      FString NormalizedNew = SubLevelPath;
      if (NormalizedExisting.EndsWith(TEXT(".umap"))) {
        NormalizedExisting = NormalizedExisting.LeftChop(5);
      }
      if (NormalizedNew.EndsWith(TEXT(".umap"))) {
        NormalizedNew = NormalizedNew.LeftChop(5);
      }
      if (NormalizedExisting.Equals(NormalizedNew, ESearchCase::IgnoreCase)) {
        // Check if the existing streaming level is actually valid/loaded
        // If it failed to load (file doesn't exist), it's a broken reference
        ULevel* ExistingLevel = ExistingStreamingLevel->GetLoadedLevel();
        bool bIsValidStreaming = ExistingLevel != nullptr || 
                                 ExistingStreamingLevel->IsStreamingStatePending();
        
        if (bIsValidStreaming) {
          // Sublevel already exists and is valid - return success with info
          TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
          Result->SetStringField(TEXT("sublevelPath"), SubLevelPath);
          Result->SetStringField(TEXT("world"), World->GetName());
          Result->SetBoolField(TEXT("alreadyExists"), true);
          SendAutomationResponse(Socket, RequestId, true,
                                 FString::Printf(TEXT("Sublevel already in world: %s"), *SubLevelPath), Result);
          return true;
        } else {
          // Existing streaming level is broken (failed to load)
          // Remove it and continue to add the new one
          UE_LOG(LogTemp, Warning, TEXT("add_sublevel: Removing broken streaming level reference: %s"), *SubLevelPath);
          World->RemoveStreamingLevel(ExistingStreamingLevel);
          break;  // Exit the loop to continue adding
        }
      }
    }
  }

  // Determine streaming class
  UClass *StreamingClass = ULevelStreamingDynamic::StaticClass();
  if (StreamingMethod.Equals(TEXT("AlwaysLoaded"), ESearchCase::IgnoreCase)) {
    StreamingClass = ULevelStreamingAlwaysLoaded::StaticClass();
  }

  ULevelStreaming *NewLevel = UEditorLevelUtils::AddLevelToWorld(
      World, *SubLevelPath, StreamingClass);
  if (NewLevel) {
    // CRITICAL FIX: Verify the streaming level can actually be loaded.
    // AddLevelToWorld() creates the streaming level object but doesn't verify
    // the level file exists. Level streaming advances on world tick — which a
    // game-thread sleep never runs — so pump it to completion synchronously.
    World->FlushLevelStreaming();

    // Check if the level is actually loaded or pending load
    // If the level file doesn't exist, GetLoadedLevel() will be null and
    // the streaming state will not be pending
    ULevel* LoadedLevel = NewLevel->GetLoadedLevel();
    bool bIsPendingLoad = NewLevel->IsStreamingStatePending();
    
    // If level is loaded or pending, it's a valid streaming level
    if (LoadedLevel || bIsPendingLoad) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("sublevelPath"), SubLevelPath);
      Result->SetStringField(TEXT("world"), World->GetName());
      Result->SetStringField(TEXT("streamingMethod"), StreamingMethod);
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Sublevel added successfully"), Result);
    } else {
      // CRITICAL FIX: Level file doesn't exist - return ERROR not success with warning
      // The streaming level was added to the world but the level file doesn't exist
      // This is an error condition, not a warning
      SendAutomationResponse(
          Socket, RequestId, false,
          FString::Printf(TEXT("Sublevel file not found: %s"), *SubLevelPath),
          nullptr, TEXT("FILE_NOT_FOUND"));
    }
  } else {
    // Did we fail because it's already there?
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Failed to add sublevel %s (Check logs)"),
                        *SubLevelPath),
        nullptr, TEXT("ADD_FAILED"));
  }
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelDelete(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString LevelPath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
  if (LevelPath.IsEmpty() && Payload.IsValid())
    Payload->TryGetStringField(TEXT("path"), LevelPath);

  if (LevelPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("levelPath required for delete"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Issue #8: Sanitize path to prevent traversal attacks
  FString SanitizedPath = SanitizeProjectRelativePath(LevelPath);
  if (SanitizedPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *LevelPath),
                           nullptr, TEXT("SECURITY_VIOLATION"));
    return true;
  }
  LevelPath = SanitizedPath;

  FString LongPackageName = LevelPath;
  int32 ObjectPathDelimiter = INDEX_NONE;
  if (LongPackageName.FindChar(TEXT('.'), ObjectPathDelimiter)) {
    LongPackageName = LongPackageName.Left(ObjectPathDelimiter);
  }

  FString DeleteMapFilename;
  FString DeleteErrorMessage;
  FString DeleteErrorCode;
  if (!TryGetAbsoluteMapFilename(LongPackageName, DeleteMapFilename) ||
      !ValidateWritableGameMapPath(LongPackageName, DeleteMapFilename,
                                   TEXT("Delete target"),
                                   DeleteErrorMessage, DeleteErrorCode)) {
    if (DeleteErrorMessage.IsEmpty()) {
      DeleteErrorMessage = FString::Printf(
          TEXT("Could not convert delete target level to filename: %s"),
          *LongPackageName);
      DeleteErrorCode = TEXT("INVALID_LEVEL_PATH");
    }
    SendAutomationResponse(Socket, RequestId, false,
                           DeleteErrorMessage, nullptr, DeleteErrorCode);
    return true;
  }

  const FString AssetName = FPaths::GetBaseFilename(LongPackageName);
  const FString ObjectPath = AssetName.IsEmpty()
                                 ? LongPackageName
                                 : FString::Printf(TEXT("%s.%s"), *LongPackageName, *AssetName);
  const FString PackageDir = FPaths::GetPath(LongPackageName);

  FString MapFilename;
  FString AbsoluteMapFilename;
  const bool bHasMapFilename = FPackageName::TryConvertLongPackageNameToFilename(
      LongPackageName, MapFilename, FPackageName::GetMapPackageExtension());
  if (bHasMapFilename) {
    AbsoluteMapFilename = FPaths::ConvertRelativePathToFull(MapFilename);
    FPaths::NormalizeFilename(AbsoluteMapFilename);
  }

  IFileManager& FileManager = IFileManager::Get();
  IAssetRegistry& AssetRegistry =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

  auto RescanLevelPackage = [&]() {
    if (bHasMapFilename && FileManager.FileExists(*AbsoluteMapFilename)) {
      TArray<FString> FilesToScan;
      FilesToScan.Add(AbsoluteMapFilename);
      AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
    }
    if (!PackageDir.IsEmpty()) {
      TArray<FString> PathsToScan;
      PathsToScan.Add(PackageDir);
      AssetRegistry.ScanPathsSynchronous(PathsToScan, true);
    }
  };

  RescanLevelPackage();

  int32 RemovedStreamingRefs = 0;
  bool bCurrentWorldMatchesTarget = false;
  if (GEditor) {
    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (EditorWorld) {
      bCurrentWorldMatchesTarget = EditorWorld->GetOutermost() &&
                                   EditorWorld->GetOutermost()->GetName() == LongPackageName;
      if (!bCurrentWorldMatchesTarget) {
        TArray<ULevelStreaming*> StreamingLevels = EditorWorld->GetStreamingLevels();
        for (ULevelStreaming* StreamingLevel : StreamingLevels) {
          if (!StreamingLevel) {
            continue;
          }

          const FString StreamingPackage = StreamingLevel->GetWorldAssetPackageFName().ToString();
          if (StreamingPackage == LongPackageName || StreamingPackage == ObjectPath) {
            StreamingLevel->SetShouldBeLoaded(false);
            StreamingLevel->SetShouldBeVisible(false);
            if (ULevel* LoadedStreamingLevel = StreamingLevel->GetLoadedLevel()) {
              if (UEditorLevelUtils::RemoveLevelFromWorld(LoadedStreamingLevel)) {
                ++RemovedStreamingRefs;
              }
            } else {
              EditorWorld->RemoveStreamingLevel(StreamingLevel);
              ++RemovedStreamingRefs;
            }
          }
        }
      }
    }
  }

  if (RemovedStreamingRefs > 0) {
    FlushRenderingCommands();
    if (GEditor) {
      GEditor->ForceGarbageCollection(true);
    }
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    FlushRenderingCommands();
  }

  UPackage* LoadedPackage = FindPackage(nullptr, *LongPackageName);
  const bool bWasLoaded = LoadedPackage != nullptr;
  bool bPackageUnloadAttempted = false;
  bool bPackageUnloadSucceeded = false;
  if (LoadedPackage && !bCurrentWorldMatchesTarget) {
    FlushRenderingCommands();
    if (GEditor) {
      GEditor->ForceGarbageCollection(true);
    }
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    FlushRenderingCommands();
    LoadedPackage = FindPackage(nullptr, *LongPackageName);

    if (LoadedPackage) {
#if MCP_HAS_PACKAGE_TOOLS
      TArray<UPackage*> PackagesToUnload;
      PackagesToUnload.Add(LoadedPackage);
      TWeakObjectPtr<UPackage> WeakLoadedPackage = LoadedPackage;
      FText UnloadError;
      bPackageUnloadAttempted = true;
      bPackageUnloadSucceeded = UPackageTools::UnloadPackages(PackagesToUnload, UnloadError, true);
      if (!UnloadError.IsEmpty()) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("delete: UnloadPackages reported for %s: %s"),
               *LongPackageName, *UnloadError.ToString());
      }
      FlushRenderingCommands();
      if (GEditor) {
        GEditor->ForceGarbageCollection(true);
      }
      CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
      FlushRenderingCommands();
      LoadedPackage = FindPackage(nullptr, *LongPackageName);
      bPackageUnloadSucceeded = bPackageUnloadSucceeded && !WeakLoadedPackage.IsValid() && LoadedPackage == nullptr;
#else
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("delete: PackageTools unavailable; cannot unload loaded map package %s"),
             *LongPackageName);
#endif
    }
  }
  const bool bPackageStillLoaded = LoadedPackage != nullptr;

  const bool bMapFileExisted = bHasMapFilename && FileManager.FileExists(*AbsoluteMapFilename);
  const bool bPackageExisted = FPackageName::DoesPackageExist(LongPackageName);
  bool bDeletedViaFileFallback = false;
  bool bDeletedBuiltData = false;
  bool bBuiltDataExists = false;
  bool bExternalSidecarDeleteAttempted = false;
  bool bExternalSidecarDeleteFailed = false;
  bool bExternalActorsExists = false;
  bool bDeletedExternalActors = false;
  bool bExternalObjectsExists = false;
  bool bDeletedExternalObjects = false;
  FString ExternalDeleteErrorMessage;
  FString ExternalDeleteErrorCode;

  const FString BuiltDataPackagePath = LongPackageName + TEXT("_BuiltData");
  FString BuiltDataFilename;
  FString AbsoluteBuiltDataFilename;
  if (FPackageName::TryConvertLongPackageNameToFilename(
          BuiltDataPackagePath, BuiltDataFilename, FPackageName::GetAssetPackageExtension())) {
    AbsoluteBuiltDataFilename = FPaths::ConvertRelativePathToFull(BuiltDataFilename);
    FPaths::NormalizeFilename(AbsoluteBuiltDataFilename);
    bBuiltDataExists = FileManager.FileExists(*AbsoluteBuiltDataFilename);
  }

  if (!bCurrentWorldMatchesTarget && !bPackageStillLoaded && bMapFileExisted) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("delete: Deleting map file directly after registry/editor cleanup: %s"),
           *AbsoluteMapFilename);
    bDeletedViaFileFallback = FileManager.Delete(*AbsoluteMapFilename, false, true, true);

    if (bBuiltDataExists) {
      bDeletedBuiltData = FileManager.Delete(*AbsoluteBuiltDataFilename, false, true, true);
    }
  }

  if (!bCurrentWorldMatchesTarget && !bPackageStillLoaded &&
      (!bMapFileExisted || bDeletedViaFileFallback)) {
    bExternalSidecarDeleteAttempted = true;

    FString ActorsErrorMessage;
    FString ActorsErrorCode;
    if (!DeleteExternalPackageDirectory(LongPackageName, TEXT("__ExternalActors__"),
                                        bExternalActorsExists,
                                        bDeletedExternalActors,
                                        ActorsErrorMessage, ActorsErrorCode)) {
      bExternalSidecarDeleteFailed = true;
      ExternalDeleteErrorMessage = ActorsErrorMessage;
      ExternalDeleteErrorCode = ActorsErrorCode;
    }

    FString ObjectsErrorMessage;
    FString ObjectsErrorCode;
    if (!DeleteExternalPackageDirectory(LongPackageName, TEXT("__ExternalObjects__"),
                                        bExternalObjectsExists,
                                        bDeletedExternalObjects,
                                        ObjectsErrorMessage, ObjectsErrorCode)) {
      bExternalSidecarDeleteFailed = true;
      if (ExternalDeleteErrorMessage.IsEmpty()) {
        ExternalDeleteErrorMessage = ObjectsErrorMessage;
        ExternalDeleteErrorCode = ObjectsErrorCode;
      }
    }
  }

  RescanLevelPackage();

  const bool bMapFileStillExists = bHasMapFilename && FileManager.FileExists(*AbsoluteMapFilename);
  const bool bPackageStillExists = FPackageName::DoesPackageExist(LongPackageName);
  const bool bRemovedBuiltData = !bBuiltDataExists || bDeletedBuiltData;
  const bool bRemovedExternalActors = !bExternalActorsExists || bDeletedExternalActors;
  const bool bRemovedExternalObjects = !bExternalObjectsExists || bDeletedExternalObjects;
  const bool bDeleted = (bDeletedViaFileFallback && !bMapFileStillExists) ||
                        (!bMapFileExisted && !bPackageExisted && !bPackageStillLoaded);
  const bool bDeletedWithSidecars = bDeleted && !bExternalSidecarDeleteFailed &&
                                     bRemovedBuiltData &&
                                     bRemovedExternalActors && bRemovedExternalObjects;

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("levelPath"), LongPackageName);
  Result->SetStringField(TEXT("objectPath"), ObjectPath);
  Result->SetStringField(TEXT("mapFilename"), AbsoluteMapFilename);
  Result->SetBoolField(TEXT("deleted"), bDeletedWithSidecars);
  Result->SetBoolField(TEXT("deletedMapFile"), bDeletedViaFileFallback);
  Result->SetBoolField(TEXT("mapDeletedOrAlreadyAbsent"), bDeleted);
  Result->SetBoolField(TEXT("deletedViaFileFallback"), bDeletedViaFileFallback);
  Result->SetBoolField(TEXT("builtDataExists"), bBuiltDataExists);
  Result->SetBoolField(TEXT("deletedBuiltData"), bDeletedBuiltData);
  Result->SetBoolField(TEXT("externalSidecarDeleteAttempted"), bExternalSidecarDeleteAttempted);
  Result->SetBoolField(TEXT("externalSidecarDeleteFailed"), bExternalSidecarDeleteFailed);
  Result->SetBoolField(TEXT("externalActorsExists"), bExternalActorsExists);
  Result->SetBoolField(TEXT("deletedExternalActors"), bDeletedExternalActors);
  Result->SetBoolField(TEXT("externalObjectsExists"), bExternalObjectsExists);
  Result->SetBoolField(TEXT("deletedExternalObjects"), bDeletedExternalObjects);
  if (!ExternalDeleteErrorMessage.IsEmpty()) {
    Result->SetStringField(TEXT("externalDeleteError"), ExternalDeleteErrorMessage);
  }
  if (!ExternalDeleteErrorCode.IsEmpty()) {
    Result->SetStringField(TEXT("externalDeleteErrorCode"), ExternalDeleteErrorCode);
  }
  Result->SetBoolField(TEXT("editorDeletionSkippedForMap"), true);
  Result->SetBoolField(TEXT("deleteAssetFailed"), false);
  Result->SetBoolField(TEXT("wasLoaded"), bWasLoaded);
  Result->SetBoolField(TEXT("packageUnloadAttempted"), bPackageUnloadAttempted);
  Result->SetBoolField(TEXT("packageUnloadSucceeded"), bPackageUnloadSucceeded);
  Result->SetBoolField(TEXT("packageStillLoaded"), bPackageStillLoaded);
  Result->SetBoolField(TEXT("currentWorldMatchesTarget"), bCurrentWorldMatchesTarget);
  Result->SetNumberField(TEXT("removedStreamingRefs"), RemovedStreamingRefs);
  Result->SetBoolField(TEXT("fileExistsAfter"), bMapFileStillExists);
  Result->SetBoolField(TEXT("packageExistsAfter"), bPackageStillExists);

  if (bExternalSidecarDeleteFailed) {
    SendAutomationResponse(Socket, RequestId, false,
                           ExternalDeleteErrorMessage, Result,
                           ExternalDeleteErrorCode.IsEmpty()
                               ? TEXT("SOURCE_EXTERNAL_DELETE_FAILED")
                               : ExternalDeleteErrorCode);
  } else if (bDeletedWithSidecars) {
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Level file deleted: %s"), *LongPackageName), Result);
  } else if (bCurrentWorldMatchesTarget || bPackageStillLoaded) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Level is still loaded and cannot be deleted safely: %s"), *LongPackageName),
                           Result, TEXT("LEVEL_LOADED"));
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Failed to delete level: %s"), *LongPackageName),
                           Result, TEXT("DELETE_FAILED"));
  }
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelRename(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString SourcePath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("levelPath"), SourcePath);
  if (SourcePath.IsEmpty() && Payload.IsValid())
    Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);

  FString DestinationPath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  if (DestinationPath.IsEmpty() && Payload.IsValid()) {
    FString NewName;
    Payload->TryGetStringField(TEXT("newName"), NewName);
    if (!NewName.IsEmpty()) {
      DestinationPath = FPaths::GetPath(NormalizeLevelPackagePath(SourcePath)) / NewName;
    }
  }

  if (SourcePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("levelPath or sourcePath required for rename_level"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("destinationPath required for rename_level"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Issue #8: Sanitize paths to prevent traversal attacks
  FString SanitizedSource = SanitizeProjectRelativePath(SourcePath);
  if (SanitizedSource.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Invalid source path (traversal/security violation): %s"), *SourcePath),
                           nullptr, TEXT("SECURITY_VIOLATION"));
    return true;
  }
  FString SanitizedDest = SanitizeProjectRelativePath(DestinationPath);
  if (SanitizedDest.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Invalid destination path (traversal/security violation): %s"), *DestinationPath),
                           nullptr, TEXT("SECURITY_VIOLATION"));
    return true;
  }
  SourcePath = NormalizeLevelPackagePath(SanitizedSource);
  DestinationPath = NormalizeLevelPackagePath(SanitizedDest);

  if (IsCurrentEditorWorldPackage(SourcePath)) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Cannot rename the current loaded level: %s"), *SourcePath),
                           nullptr, TEXT("LEVEL_LOADED"));
    return true;
  }

  bool bOverwrite = false;
  if (Payload.IsValid()) {
    Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);
  }

  TSharedPtr<FJsonObject> Result;
  FString ErrorMessage;
  FString ErrorCode;
  const bool bCopied = CopyLevelMapPackageFile(SourcePath, DestinationPath, bOverwrite, Result, ErrorMessage, ErrorCode);
  if (!bCopied) {
    SendAutomationResponse(Socket, RequestId, false, ErrorMessage, Result, ErrorCode);
    return true;
  }

  FString SourceFilename;
  TryGetAbsoluteMapFilename(SourcePath, SourceFilename);
  bool bDeletedSource = false;
  if (!SourceFilename.IsEmpty()) {
    bDeletedSource = IFileManager::Get().Delete(*SourceFilename, false, true, true);
  }

  const FString SourceBuiltDataPackagePath = SourcePath + TEXT("_BuiltData");
  FString SourceBuiltDataFilename;
  bool bSourceBuiltDataExists = false;
  bool bDeletedBuiltData = false;
  if (FPackageName::TryConvertLongPackageNameToFilename(
          SourceBuiltDataPackagePath, SourceBuiltDataFilename, FPackageName::GetAssetPackageExtension())) {
    SourceBuiltDataFilename = FPaths::ConvertRelativePathToFull(SourceBuiltDataFilename);
    FPaths::NormalizeFilename(SourceBuiltDataFilename);
    bSourceBuiltDataExists = IFileManager::Get().FileExists(*SourceBuiltDataFilename);
    if (bSourceBuiltDataExists) {
      bDeletedBuiltData = IFileManager::Get().Delete(*SourceBuiltDataFilename, false, true, true);
    }
  }

  bool bSourceExternalActorsExists = false;
  bool bDeletedSourceExternalActors = false;
  bool bSourceExternalObjectsExists = false;
  bool bDeletedSourceExternalObjects = false;
  if (bDeletedSource) {
    if (!DeleteExternalPackageDirectory(SourcePath, TEXT("__ExternalActors__"),
                                        bSourceExternalActorsExists,
                                        bDeletedSourceExternalActors,
                                        ErrorMessage, ErrorCode) ||
        !DeleteExternalPackageDirectory(SourcePath, TEXT("__ExternalObjects__"),
                                        bSourceExternalObjectsExists,
                                        bDeletedSourceExternalObjects,
                                        ErrorMessage, ErrorCode)) {
      Result->SetStringField(TEXT("externalDeleteError"), ErrorMessage);
      SendAutomationResponse(Socket, RequestId, false, ErrorMessage,
                             Result, ErrorCode);
      return true;
    }
  }

  ScanLevelPackagePath(SourcePath, SourceFilename);
  const bool bSourceFileExistsAfter = !SourceFilename.IsEmpty() && IFileManager::Get().FileExists(*SourceFilename);
  const bool bRemovedSourceBuiltData = !bSourceBuiltDataExists || bDeletedBuiltData;
  const bool bRemovedSourceExternalActors = !bSourceExternalActorsExists || bDeletedSourceExternalActors;
  const bool bRemovedSourceExternalObjects = !bSourceExternalObjectsExists || bDeletedSourceExternalObjects;
  const bool bRenamed = bDeletedSource && !bSourceFileExistsAfter &&
      bRemovedSourceBuiltData && bRemovedSourceExternalActors &&
      bRemovedSourceExternalObjects;
  Result->SetBoolField(TEXT("renamed"), bRenamed);
  Result->SetBoolField(TEXT("deletedSourceFile"), bDeletedSource);
  Result->SetBoolField(TEXT("sourceBuiltDataExists"), bSourceBuiltDataExists);
  Result->SetBoolField(TEXT("deletedSourceBuiltData"), bDeletedBuiltData);
  Result->SetBoolField(TEXT("sourceExternalActorsExists"), bSourceExternalActorsExists);
  Result->SetBoolField(TEXT("deletedSourceExternalActors"), bDeletedSourceExternalActors);
  Result->SetBoolField(TEXT("sourceExternalObjectsExists"), bSourceExternalObjectsExists);
  Result->SetBoolField(TEXT("deletedSourceExternalObjects"), bDeletedSourceExternalObjects);
  Result->SetBoolField(TEXT("sourceFileExistsAfter"), bSourceFileExistsAfter);

  if (bRenamed) {
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Level renamed to: %s"), *DestinationPath), Result);
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Failed to delete source level after copy: %s"), *SourcePath),
                           Result, TEXT("SOURCE_DELETE_FAILED"));
  }
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelDuplicate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString SourcePath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
  if (SourcePath.IsEmpty() && Payload.IsValid())
    Payload->TryGetStringField(TEXT("levelPath"), SourcePath);

  FString DestinationPath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);

  if (SourcePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath or levelPath required for duplicate_level"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("destinationPath required for duplicate_level"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Issue #8: Sanitize paths to prevent traversal attacks
  FString SanitizedSource = SanitizeProjectRelativePath(SourcePath);
  if (SanitizedSource.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Invalid source path (traversal/security violation): %s"), *SourcePath),
                           nullptr, TEXT("SECURITY_VIOLATION"));
    return true;
  }
  FString SanitizedDest = SanitizeProjectRelativePath(DestinationPath);
  if (SanitizedDest.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Invalid destination path (traversal/security violation): %s"), *DestinationPath),
                           nullptr, TEXT("SECURITY_VIOLATION"));
    return true;
  }
  SourcePath = NormalizeLevelPackagePath(SanitizedSource);
  DestinationPath = NormalizeLevelPackagePath(SanitizedDest);

  bool bOverwrite = false;
  if (Payload.IsValid()) {
    Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);
  }

  TSharedPtr<FJsonObject> Result;
  FString ErrorMessage;
  FString ErrorCode;
  const bool bDuplicated = CopyLevelMapPackageFile(SourcePath, DestinationPath, bOverwrite, Result, ErrorMessage, ErrorCode);
  if (bDuplicated) {
    Result->SetBoolField(TEXT("duplicated"), true);
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Level duplicated to: %s"), *DestinationPath), Result);
  } else {
    SendAutomationResponse(Socket, RequestId, false, ErrorMessage, Result,
                           ErrorCode.IsEmpty() ? TEXT("DUPLICATE_FAILED") : ErrorCode);
  }
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelGetSummary(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    FMcpResponseHandle Socket) {
  FString LevelPath;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
    if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), LevelPath);
  }

  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!World) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
    return true;
  }

  ULevel* TargetLevel = nullptr;
  if (!LevelPath.IsEmpty()) {
    LevelPath = SanitizeProjectRelativePath(LevelPath);
    if (LevelPath.IsEmpty()) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Invalid levelPath"), nullptr,
                             TEXT("SECURITY_VIOLATION"));
      return true;
    }

    TArray<ULevel*> Levels = GetAllLevelsFromWorld(World);
    for (ULevel* Level : Levels) {
      if (Level && Level->GetOutermost() && Level->GetOutermost()->GetName() == LevelPath) {
        TargetLevel = Level;
        break;
      }
    }
  } else {
    TargetLevel = World->GetCurrentLevel();
  }

  if (TargetLevel) {
    // Loaded path: preserve existing JSON shape, only ADD `loaded: true`.
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("levelPath"), TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : TEXT(""));
    Result->SetStringField(TEXT("levelName"), TargetLevel->GetName());
    Result->SetNumberField(TEXT("actorCount"), TargetLevel->Actors.Num());
    Result->SetBoolField(TEXT("loaded"), true);

    // GameMode / default pawn, so "what pawn does map X spawn" doesn't need python.
    // World Settings' "GameMode Override" is AWorldSettings::DefaultGameMode; when it's
    // null the project-wide UGameMapsSettings::GlobalDefaultGameMode applies. The default
    // pawn is read off the effective game mode's CDO.
    if (AWorldSettings* WorldSettings = TargetLevel->GetWorldSettings()) {
      TSubclassOf<AGameModeBase> OverrideGameMode = WorldSettings->DefaultGameMode;
      Result->SetStringField(TEXT("gameModeOverride"),
          OverrideGameMode ? OverrideGameMode->GetPathName() : TEXT(""));

      TSubclassOf<AGameModeBase> EffectiveGameMode = OverrideGameMode;
      if (!EffectiveGameMode) {
        // GlobalDefaultGameMode is private; the static accessor returns its path string.
        const FSoftClassPath GlobalGameModePath(
            UGameMapsSettings::GetGlobalDefaultGameMode());
        if (GlobalGameModePath.IsValid()) {
          EffectiveGameMode = GlobalGameModePath.TryLoadClass<AGameModeBase>();
        }
      }
      Result->SetStringField(TEXT("effectiveGameMode"),
          EffectiveGameMode ? EffectiveGameMode->GetPathName() : TEXT(""));

      if (EffectiveGameMode) {
        if (const AGameModeBase* GameModeCDO =
                EffectiveGameMode->GetDefaultObject<AGameModeBase>()) {
          Result->SetStringField(TEXT("defaultPawnClass"),
              GameModeCDO->DefaultPawnClass ? GameModeCDO->DefaultPawnClass->GetPathName()
                                            : TEXT(""));
        }
      }
    }

    SendAutomationResponse(Socket, RequestId, true, TEXT("Level info retrieved"), Result);
    return true;
  }

  // Not loaded as a UWorld — fall back to AssetRegistry lookup so callers can
  // query metadata for any map asset without forcing a load. We do NOT auto-load.
  if (!LevelPath.IsEmpty()) {
    IAssetRegistry& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    // Accept either a package path ("/Game/Maps/Foo") or a full object path
    // ("/Game/Maps/Foo.Foo"). FSoftObjectPath handles both forms.
    FAssetData AssetData;
    #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(LevelPath));
    if (!AssetData.IsValid()) {
      // Try appending `.<ShortName>` to a bare package path.
      const FString ShortName = FPackageName::GetShortName(LevelPath);
      if (!ShortName.IsEmpty() && !LevelPath.Contains(TEXT("."))) {
        const FString ObjectPath = LevelPath + TEXT(".") + ShortName;
        AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
      }
    }
    #else
    AssetData = AssetRegistry.GetAssetByObjectPath(FName(*LevelPath));
    if (!AssetData.IsValid()) {
      const FString ShortName = FPackageName::GetShortName(LevelPath);
      if (!ShortName.IsEmpty() && !LevelPath.Contains(TEXT("."))) {
        const FString ObjectPath = LevelPath + TEXT(".") + ShortName;
        AssetData = AssetRegistry.GetAssetByObjectPath(FName(*ObjectPath));
      }
    }
    #endif

    if (AssetData.IsValid()) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetBoolField(TEXT("loaded"), false);
      Result->SetStringField(TEXT("levelPath"), AssetData.PackageName.ToString());
      Result->SetStringField(TEXT("levelName"), AssetData.AssetName.ToString());
      Result->SetStringField(TEXT("packageName"), AssetData.PackageName.ToString());
      Result->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
      Result->SetStringField(TEXT("objectPath"), MCP_ASSET_DATA_GET_OBJECT_PATH(AssetData));
      Result->SetStringField(TEXT("assetClass"), MCP_ASSET_DATA_GET_CLASS_PATH(AssetData));

      TSharedPtr<FJsonObject> TagsObj = McpHandlerUtils::CreateResultObject();
      for (const auto& Kvp : AssetData.TagsAndValues) {
        TagsObj->SetStringField(Kvp.Key.ToString(), Kvp.Value.AsString());
      }
      Result->SetObjectField(TEXT("tagsAndValues"), TagsObj);

      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Level info retrieved (asset registry, not loaded)"), Result);
      return true;
    }
  }

  SendAutomationResponse(Socket, RequestId, false,
                         FString::Printf(TEXT("Level not found: %s"), *LevelPath),
                         nullptr, TEXT("LEVEL_NOT_FOUND"));
  return true;
}

#else

#define MCP_LEVEL_HANDLER_STUB(Fn)                                            \
  bool UMcpAutomationBridgeSubsystem::Fn(const FString &,                     \
                                         const TSharedPtr<FJsonObject> &,     \
                                         FMcpResponseHandle) {                \
    return false;                                                             \
  }

MCP_LEVEL_HANDLER_STUB(HandleLevelLoad)
MCP_LEVEL_HANDLER_STUB(HandleLevelSave)
MCP_LEVEL_HANDLER_STUB(HandleLevelSaveAs)
MCP_LEVEL_HANDLER_STUB(HandleLevelCreate)
MCP_LEVEL_HANDLER_STUB(HandleLevelStream)
MCP_LEVEL_HANDLER_STUB(HandleLevelUnload)
MCP_LEVEL_HANDLER_STUB(HandleLevelCreateLight)
MCP_LEVEL_HANDLER_STUB(HandleLevelBuildLighting)
MCP_LEVEL_HANDLER_STUB(HandleLevelSetMetadata)
MCP_LEVEL_HANDLER_STUB(HandleLevelValidate)
MCP_LEVEL_HANDLER_STUB(HandleLevelList)
MCP_LEVEL_HANDLER_STUB(HandleLevelGetCurrent)
MCP_LEVEL_HANDLER_STUB(HandleLevelExport)
MCP_LEVEL_HANDLER_STUB(HandleLevelImport)
MCP_LEVEL_HANDLER_STUB(HandleLevelAddSublevel)
MCP_LEVEL_HANDLER_STUB(HandleLevelDelete)
MCP_LEVEL_HANDLER_STUB(HandleLevelRename)
MCP_LEVEL_HANDLER_STUB(HandleLevelDuplicate)
MCP_LEVEL_HANDLER_STUB(HandleLevelGetSummary)
#undef MCP_LEVEL_HANDLER_STUB

#endif // WITH_EDITOR
