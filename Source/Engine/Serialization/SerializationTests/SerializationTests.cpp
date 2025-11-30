#include "gtest/gtest.h"
#include "Serialization.h"
#include "Utility/Random.h"

#pragma optimize("", off)

namespace spt::srl::tests
{


struct CustomType
{
	Real32 floatValue = 0.f;
	Int32 intValue = 0;
	lib::String stringValue;

	void Serialize(Serializer& serializer)
	{
		serializer.Serialize("FloatValue", floatValue);
		serializer.Serialize("IntValue", intValue);
		serializer.Serialize("StringValue", stringValue);
	}
};


struct CustomType2
{
	Real32 floatValue = 0.f;
	CustomType nestedType;

	void Serialize(Serializer& serializer)
	{
		serializer.Serialize("FloatValue", floatValue);
		serializer.Serialize("NestedType", nestedType);
	}
};


static_assert(detail::CSerializableIntrusive<CustomType>);


TEST(BasicTypesSerialization, Serialization)
{
	Real32  value = 3.14f;

	Serializer writer = Serializer::CreateWriter();
	writer.Serialize("TestValue", value);

	const lib::String serializedData = writer.ToString();

	Real32 readValue = 0.0f;

	Serializer reader = Serializer::CreateReader(serializedData);
	reader.Serialize("TestValue", readValue);

	EXPECT_EQ(value, readValue);
}

TEST(CustomTypeSerialization, Serialization)
{
	CustomType originalData;
	originalData.floatValue  = 6.28f;
	originalData.intValue    = 42;
	originalData.stringValue = "Hello, Serialization!";

	Serializer writer = Serializer::CreateWriter();
	writer.Serialize("CustomData", originalData);

	const lib::String serializedData = writer.ToString();

	CustomType loadedData;
	Serializer reader = Serializer::CreateReader(serializedData);
	reader.Serialize("CustomData", loadedData);

	EXPECT_EQ(originalData.floatValue, loadedData.floatValue);
	EXPECT_EQ(originalData.intValue, loadedData.intValue);
	EXPECT_EQ(originalData.stringValue, loadedData.stringValue);
}

TEST(CustomTypeSerialization2, Serialization)
{
	CustomType2 originalData;
	originalData.floatValue = 1.618f;
	originalData.nestedType.floatValue  = 6.28f;
	originalData.nestedType.intValue    = 42;
	originalData.nestedType.stringValue = "Hello, Serialization!";

	Serializer writer = Serializer::CreateWriter();
	writer.Serialize("Data", originalData);

	const lib::String serializedData = writer.ToString();

	CustomType2 loadedData;
	Serializer reader = Serializer::CreateReader(serializedData);
	reader.Serialize("Data", loadedData);

	EXPECT_EQ(originalData.floatValue, loadedData.floatValue);
	EXPECT_EQ(originalData.nestedType.floatValue, loadedData.nestedType.floatValue);
	EXPECT_EQ(originalData.nestedType.intValue, loadedData.nestedType.intValue);
	EXPECT_EQ(originalData.nestedType.stringValue, loadedData.nestedType.stringValue);
}

TEST(Array1, Serialization)
{
	lib::DynamicArray<Int32> originalData = { 1, 2, 3, 4, 5 };

	Serializer writer = Serializer::CreateWriter();
	writer.Serialize("DataArray", originalData);

	const lib::String serializedData = writer.ToString();

	lib::DynamicArray<Int32> loadedData;
	Serializer reader = Serializer::CreateReader(serializedData);
	reader.Serialize("DataArray", loadedData);

	EXPECT_EQ(originalData.size(), loadedData.size());

	for (SizeType i = 0; i < originalData.size(); ++i)
	{
		EXPECT_EQ(originalData[i], loadedData[i]);
	}
}

TEST(Array2, Serialization)
{
	lib::DynamicArray<CustomType> originalData = {
		CustomType{ 1.1f, 10, "First"  },
		CustomType{ 2.2f, 20, "Second" },
		CustomType{ 3.3f, 30, "Third"  }
	};

	Serializer writer = Serializer::CreateWriter();
	writer.Serialize("DataArray", originalData);

	const lib::String serializedData = writer.ToString();

	lib::DynamicArray<CustomType> loadedData;
	Serializer reader = Serializer::CreateReader(serializedData);
	reader.Serialize("DataArray", loadedData);

	EXPECT_EQ(originalData.size(), loadedData.size());

	for (SizeType i = 0; i < originalData.size(); ++i)
	{
		EXPECT_EQ(originalData[i].floatValue, loadedData[i].floatValue);
		EXPECT_EQ(originalData[i].intValue, loadedData[i].intValue);
		EXPECT_EQ(originalData[i].stringValue, loadedData[i].stringValue);
	}
}

TEST(BinaryData, Serialization)
{
	lib::DynamicArray<Byte> originalData;
	originalData.resize(256);
	for (SizeType idx = 0u; idx < originalData.size(); ++idx)
	{
		originalData[idx] = Byte(lib::rnd::Random(0, 255));
	}

	Serializer writer = Serializer::CreateWriter();
	writer.Serialize("BinaryData", originalData);

	const lib::String serializedData = writer.ToString();

	lib::DynamicArray<Byte> loadedData;
	Serializer reader = Serializer::CreateReader(serializedData);
	reader.Serialize("BinaryData", loadedData);

	EXPECT_EQ(originalData.size(), loadedData.size());
	for (SizeType i = 0; i < originalData.size(); ++i)
	{
		EXPECT_EQ(originalData[i], loadedData[i]);
	}
}

TEST(HashMap, Serialization)
{
	lib::HashMap<lib::String, Int32> originalData = {
		{"One", 1},
		{"Two", 2},
		{"Three", 3}
	};

	Serializer writer = Serializer::CreateWriter();
	writer.Serialize("DataMap", originalData);

	const lib::String serializedData = writer.ToString();

	lib::HashMap<lib::String, Int32> loadedData;
	Serializer reader = Serializer::CreateReader(serializedData);
	reader.Serialize("DataMap", loadedData);

	EXPECT_EQ(originalData.size(), loadedData.size());
	for (const auto& [key, value] : originalData)
	{
		EXPECT_EQ(value, loadedData[key]);
	}
}

TEST(EmptyArray, Serialization)
{
	lib::DynamicArray<Int32> originalData;

	Serializer writer = Serializer::CreateWriter();
	writer.Serialize("EmptyArray", originalData);

	const lib::String serializedData = writer.ToString();

	lib::DynamicArray<Int32> loadedData;
	Serializer reader = Serializer::CreateReader(serializedData);
	reader.Serialize("EmptyArray", loadedData);

	EXPECT_EQ(originalData.size(), loadedData.size());
}

} // spt::srl::tests


int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);

	using namespace spt;

	const auto testsResult = RUN_ALL_TESTS();

	return testsResult;
}