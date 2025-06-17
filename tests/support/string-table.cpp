/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <thread>
#include <bloom/support/string-table.hpp>
#include <gtest/gtest.h>

class StringTableFixture : public ::testing::Test
{
protected:
	void SetUp() override {}

	void TearDown() override {}

	blm::StringTable table;
};

TEST_F(StringTableFixture, InternEmptyStringReturnsZero)
{
	EXPECT_EQ(table.intern(""), 0);
	EXPECT_EQ(table.size(), 1);
}

TEST_F(StringTableFixture, InternSameStringReturnsSameId)
{
	const auto id1 = table.intern("hello");
	const auto id2 = table.intern("hello");

	EXPECT_EQ(id1, id2);
	EXPECT_EQ(table.size(), 2);
}

TEST_F(StringTableFixture, InternDifferentStringsReturnDifferentIds)
{
	const auto id1 = table.intern("foo");
	const auto id2 = table.intern("bar");

	EXPECT_NE(id1, id2);
	EXPECT_EQ(table.size(), 3);
}

TEST_F(StringTableFixture, GetReturnsCorrectString)
{
	const auto id = table.intern("sample");
	const auto str = table.get(id);

	EXPECT_EQ(str, "sample");
}

TEST_F(StringTableFixture, ContainsWorks)
{
	table.intern("exist");
	EXPECT_TRUE(table.contains("exist"));
	EXPECT_FALSE(table.contains("not_exist"));
}

TEST_F(StringTableFixture, ClearResetsTable)
{
	table.intern("one");
	table.intern("two");

	table.clear();

	EXPECT_EQ(table.size(), 1);
	EXPECT_FALSE(table.contains("one"));
	EXPECT_EQ(table.intern(""), 0);
}
