#pragma once

#include "CloudscapeAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "Cloudscape/CloudscapeDefinition.h"
#include "DDC.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::as
{

struct CloudscapeAssetDefinition
{
	lib::Path weatherMapTex;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("WeatherMapTex", weatherMapTex);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(CloudscapeAssetDefinition);


class CLOUDSCAPE_ASSET_API CloudscapeAssetInitializer : public AssetDataInitializer
{
public:

	explicit CloudscapeAssetInitializer(CloudscapeAssetDefinition definition)
		: m_definition(std::move(definition))
	{ }

	virtual void InitializeNewAsset(AssetInstance& asset) override;

private:

	CloudscapeAssetDefinition m_definition;
};


class CLOUDSCAPE_ASSET_API CloudscapeAsset : public AssetInstance
{
	ASSET_TYPE_GENERATED_BODY(CloudscapeAsset, AssetInstance)

public:

	using AssetInstance::AssetInstance;

	const CloudscapeAssetDefinition& GetCloudscapeAssetDefinition() const;

	const lib::SharedPtr<rdr::TextureView>& GetWeatherMap() const { return m_weatherMap; }

	rsc::CloudscapeDefinition GetCloudscapeDefinition() const;

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void OnInitialize() override;
	// End AssetInstance overrides

private:

	lib::SharedPtr<rdr::TextureView> m_weatherMap;
};
SPT_REGISTER_ASSET_TYPE(CloudscapeAsset);

} // spt::as
