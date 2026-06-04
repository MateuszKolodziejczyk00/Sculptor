#include "ContentFilesWatcher.h"
#include "AssetsSystem.h"
#include "Utility/String/StringUtils.h"


namespace spt::as
{

ContentFilesWatcher:: ContentFilesWatcher(AssetsSystem& inAssetsSystem, OnAssetModifyDetected onAssetModified)
	: m_assetsSystem(inAssetsSystem)
	, m_onAssetModified(std::move(onAssetModified))
{
	lib::WatchParams watchParams;
	watchParams.directory           = m_assetsSystem.GetContentPath();
	watchParams.callback            = lib::FileModifiedCallback::CreateRawMember(this, &ContentFilesWatcher::OnFileModified);
	watchParams.watchSubdirectories = true;

	m_fileWatcherHandle = lib::StartWatchingDirectory(std::move(watchParams));
}

ContentFilesWatcher::~ContentFilesWatcher()
{
	lib::StopWatchingDirectory(m_fileWatcherHandle);
}

void ContentFilesWatcher::OnFileModified(const lib::FileModifiedPayload& payload)
{
	const lib::StringView fileExtension = lib::File::GetExtension(payload.fileName);
	if (fileExtension == "sptasset")
	{
		const ResourcePath resourcePath = ResourcePath(payload.relativePath);
		m_onAssetModified.ExecuteIfBound(resourcePath);
	}
}

} // spt::as
