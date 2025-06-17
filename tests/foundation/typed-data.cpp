/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/typed-data.hpp>
#include <bloom/foundation/types.hpp>
#include <gtest/gtest.h>

class TypedDataFixture : public ::testing::Test
{
protected:
	void SetUp() override {}

	void TearDown() override {}

	blm::TypedData data;
};

TEST_F(TypedDataFixture, SetTrivialData)
{
	data.set<int, blm::DataType::INT32>(42);

	EXPECT_EQ(data.get<blm::DataType::INT32>(), 42);
}

TEST_F(TypedDataFixture, SetNonTrivialData)
{
	blm::DataTypeTraits<blm::DataType::STRUCT>::type struct_data;
	struct_data.size = 16;
	struct_data.alignment = 8;
	struct_data.fields = {
		{ "x", blm::DataType::INT32 },
		{ "y", blm::DataType::FLOAT64 }
	};

	data.set<decltype(struct_data), blm::DataType::STRUCT>(std::move(struct_data));

	const auto&[size, alignment, fields] = data.get<blm::DataType::STRUCT>();
	EXPECT_EQ(size, 16);
	EXPECT_EQ(alignment, 8);
	ASSERT_EQ(fields.size(), 2);
	EXPECT_EQ(fields[0].first, "x");
	EXPECT_EQ(fields[0].second, blm::DataType::INT32);
	EXPECT_EQ(fields[1].first, "y");
	EXPECT_EQ(fields[1].second, blm::DataType::FLOAT64);
}

TEST_F(TypedDataFixture, SetPointer)
{
	blm::DataTypeTraits<blm::DataType::POINTER>::type ptr_data = {};
	ptr_data.pointee_type = blm::DataType::UINT64;
	ptr_data.addr_space = 1;

	data.set<decltype(ptr_data), blm::DataType::POINTER>(std::move(ptr_data));

	const auto& result = data.get<blm::DataType::POINTER>();
	EXPECT_EQ(result.pointee_type, blm::DataType::UINT64);
	EXPECT_EQ(result.addr_space, 1);
}

TEST_F(TypedDataFixture, TypeCheck)
{
	data.set<std::int8_t, blm::DataType::INT8>(127);

	EXPECT_TRUE(data.is_type<std::int8_t>());
	EXPECT_FALSE(data.is_type<std::uint64_t>());
	EXPECT_FALSE(data.is_type<float>());
}
