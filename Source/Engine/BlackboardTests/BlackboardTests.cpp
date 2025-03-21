#include "gtest/gtest.h"
#include "Blackboard.h"
#include "BlackboardSerialization.h"
#include "SerializationHelper.h"

namespace spt::lib::tests
{

struct BBDataType1
{
	Uint32 value = 0u;
};

struct BBDataType2
{
	Uint32 value1 = 1u;
	Uint32 value2 = 2u;
};

} // spt::lib::tests

namespace spt::srl
{

template<>
struct TypeSerializer<lib::tests::BBDataType1>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("value", data.value);
	}
};

template<>
struct TypeSerializer<lib::tests::BBDataType2>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("value1", data.value1);
		serializer.Serialize("value2", data.value2);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::lib::tests::BBDataType1);
SPT_YAML_SERIALIZATION_TEMPLATES(spt::lib::tests::BBDataType2);

SPT_REGISTER_TYPE_FOR_BLACKBOARD_SERIALIZATION(spt::lib::tests::BBDataType1);
SPT_REGISTER_TYPE_FOR_BLACKBOARD_SERIALIZATION(spt::lib::tests::BBDataType2);


namespace spt::lib::tests
{

TEST(BlackboardSerialization, BasicSerializationSingleComponent)
{
	lib::Blackboard originalBlackboard;
	BBDataType1& originalData1 = originalBlackboard.Create<BBDataType1>(BBDataType1{ .value = 42u });

	const lib::String data = srl::SerializationHelper::SerializeStruct(originalBlackboard);

	EXPECT_TRUE(!data.empty());

	lib::Blackboard copiedBlackboard;

	srl::SerializationHelper::DeserializeStruct(copiedBlackboard, data);

	const BBDataType1* copyData1 = copiedBlackboard.Find<BBDataType1>();
	const BBDataType2* copyData2 = copiedBlackboard.Find<BBDataType2>();

	EXPECT_TRUE(copyData1 != nullptr);
	EXPECT_EQ(copyData1->value, originalData1.value);

	EXPECT_TRUE(copyData2 == nullptr);
}

TEST(BlackboardSerialization, BasicSerializationMultipleComponents)
{
	lib::Blackboard originalBlackboard;
	BBDataType1& originalData1 = originalBlackboard.Create<BBDataType1>(BBDataType1{ .value = 42u });
	BBDataType2& originalData2 = originalBlackboard.Create<BBDataType2>(BBDataType2{ .value2 = 42u });

	const lib::String data = srl::SerializationHelper::SerializeStruct(originalBlackboard);

	EXPECT_TRUE(!data.empty());

	lib::Blackboard copiedBlackboard;

	srl::SerializationHelper::DeserializeStruct(copiedBlackboard, data);

	const BBDataType1* copyData1 = copiedBlackboard.Find<BBDataType1>();
	const BBDataType2* copyData2 = copiedBlackboard.Find<BBDataType2>();

	EXPECT_TRUE(copyData1 != nullptr);
	EXPECT_EQ(copyData1->value, originalData1.value);

	EXPECT_TRUE(copyData2 != nullptr);
	EXPECT_EQ(copyData2->value1, originalData2.value1);
	EXPECT_EQ(copyData2->value2, originalData2.value2);
}

} // spt::lib::tests


int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);

	using namespace spt;

	const auto testsResult = RUN_ALL_TESTS();

	return testsResult;
}
