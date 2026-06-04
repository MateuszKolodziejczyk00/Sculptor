#pragma once

#include "ResourcePath.h"
#include "SculptorCoreTypes.h"
#include "FileSystem/DirectoryWatcher.h"
#include "Containers/MPSCQueue.h"

#include <chrono>


namespace spt::as
{

class AssetsSystem;

using OnAssetModifyDetected = lib::Delegate<void(const ResourcePath& /* pathID */)>;


class ContentFilesWatcher
{
public:

	ContentFilesWatcher(AssetsSystem& inAssetsSystem, OnAssetModifyDetected onAssetModified);
	~ContentFilesWatcher();

private:

	void OnFileModified(const lib::FileModifiedPayload& payload);

	static constexpr auto s_programmaticSaveIgnoreDuration = std::chrono::seconds(2);

	AssetsSystem& m_assetsSystem;

	OnAssetModifyDetected m_onAssetModified;

	lib::FileWatcherHandle m_fileWatcherHandle = {};
};

} // spt::as
