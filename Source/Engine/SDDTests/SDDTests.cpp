#include "gtest/gtest.h"
#include "SerializationHelper.h"
#include "Generated/SDDTests.h"


namespace spt::sdd::tests
{

static_assert(srl::detail::CSerializableIntrusive<SomeData1>);
static_assert(srl::detail::CSerializableExternal<SomeData3>);
static_assert(std::is_same_v<std::underlying_type_t<ESomeEnum>, Uint32>);
static_assert(std::is_same_v<std::underlying_type_t<ESomeSignedEnum>, Int32>);
static_assert(static_cast<Uint32>(ESomeEnum::A) == 0u && static_cast<Uint32>(ESomeEnum::B) == 4u && static_cast<Uint32>(ESomeEnum::C) == 5u);
static_assert(static_cast<Int32>(ESomeSignedEnum::Negative) == -1);


TEST(SDD, Defaults)
{
	const SomeData1 data1;
	EXPECT_EQ(data1.data0.x, 0u);
	EXPECT_EQ(data1.data0.y, 0);
	EXPECT_EQ(data1.z, 8);
	EXPECT_EQ(data1.position, math::Vector3f(1.f, 2.f, 3.f));
	EXPECT_EQ(data1.someEnum, ESomeEnum::B);
	EXPECT_EQ(static_cast<Int32>(data1.signedEnum), 0);

	EXPECT_EQ(SomeData2{}.w, 9);

	const SomeData3 data3;
	EXPECT_EQ(data3.a, 1);
	EXPECT_EQ(data3.b, 2.f);
	EXPECT_EQ(data3.c[0], 0u);
	EXPECT_EQ(data3.d, ESomeSignedEnum::Negative);
}


TEST(SDD, StructRoundTrip)
{
	SomeData1 original;
	original.data0.x = 3u;
	original.data0.y = -4;
	original.z = 5;
	original.values = { 1u, 2u, 3u };
	original.position = math::Vector3f(4.f, 5.f, 6.f);
	original.someEnum = ESomeEnum::C;
	original.signedEnum = ESomeSignedEnum::Negative;

	SomeData1 loaded;
	srl::SerializationHelper::DeserializeStruct(loaded, srl::SerializationHelper::SerializeStruct(original));

	EXPECT_EQ(loaded.data0.x, original.data0.x);
	EXPECT_EQ(loaded.data0.y, original.data0.y);
	EXPECT_EQ(loaded.z, original.z);
	EXPECT_EQ(loaded.values, original.values);
	EXPECT_EQ(loaded.position, original.position);
	EXPECT_EQ(loaded.someEnum, original.someEnum);
	EXPECT_EQ(loaded.signedEnum, original.signedEnum);
}


TEST(SDD, ShaderStructRoundTrip)
{
	SomeData3 original;
	original.a = 7;
	original.b = 0.5f;
	original.c = { 8u, 9u };
	original.d = ESomeSignedEnum::Positive;

	SomeData3 loaded;
	srl::SerializationHelper::DeserializeStruct(loaded, srl::SerializationHelper::SerializeStruct(original));

	EXPECT_EQ(loaded, original);
	EXPECT_EQ(rdr::shader_translator::GetTypeName<ESomeSignedEnum>(), "int");
}


TEST(SDD, MissingKeysKeepValues)
{
	SomeData1 data1;
	data1.data0.x = 3u;
	data1.values = { 1u, 2u };
	srl::SerializationHelper::DeserializeStruct(data1, "{}");

	EXPECT_EQ(data1.data0.x, 3u);
	EXPECT_EQ(data1.z, 8);
	EXPECT_EQ(data1.values, lib::DynamicArray<Uint32>({ 1u, 2u }));
	EXPECT_EQ(data1.position, math::Vector3f(1.f, 2.f, 3.f));

	SomeData3 data3;
	data3.c = { 4u, 5u };
	srl::SerializationHelper::DeserializeStruct(data3, R"({ "a": 2 })");

	EXPECT_EQ(data3.a, 2);
	EXPECT_EQ(data3.b, 2.f);
	EXPECT_EQ(data3.c[1], 5u);
}


TEST(SDD, TestDefaults)
{
	SomeData1 data1;
	ApplyTestDefaults(data1);

	EXPECT_EQ(data1.data0.x, 0u);
	EXPECT_EQ(data1.data0.y, -2);
	EXPECT_EQ(data1.z, 8);
	EXPECT_EQ(data1.position, math::Vector3f(4.f, 5.f, 6.f));
	EXPECT_EQ(data1.someEnum, ESomeEnum::C);

	SomeData2 data2;
	ApplyTestDefaults(data2);
	EXPECT_EQ(data2.w, 9);

	SomeData3 data3;
	ApplyTestDefaults(data3);
	srl::SerializationHelper::DeserializeStruct(data3, R"({ "a": 7 })");

	EXPECT_EQ(data3.a, 7);
	EXPECT_EQ(data3.b, 0.5f);
	EXPECT_EQ(data3.c[0], 0u);
}


TEST(SDD, MemberMetaData)
{
	const rtti::RTTIStruct* type = rtti::GetType(lib::TypeInfo<SomeData2>{})->AsStruct();
	ASSERT_NE(type, nullptr);
	ASSERT_EQ(type->members.size(), 1u);

	const lib::Span<const rtti::RTTIMemberMetaData> metaData = type->members[0].metaData;
	ASSERT_EQ(metaData.size(), 2u);
	EXPECT_STREQ(metaData[0].name, "meta1");
	EXPECT_STREQ(metaData[1].name, "meta2");
}

} // spt::sdd::tests


int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);

	return RUN_ALL_TESTS();
}
