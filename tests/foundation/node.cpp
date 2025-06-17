/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/node.hpp>
#include <gtest/gtest.h>

class NodeFixture : public ::testing::Test
{
protected:
	void SetUp() override {}

	void TearDown() override {}

	blm::Node node;
};

TEST_F(NodeFixture, BasicProperties)
{
	node.ir_type = blm::NodeType::ADD;
	node.type_kind = blm::DataType::INT32;
	node.str_id = 42;

	EXPECT_EQ(node.ir_type, blm::NodeType::ADD);
	EXPECT_EQ(node.type_kind, blm::DataType::INT32);
	EXPECT_EQ(node.str_id, 42);
}

TEST_F(NodeFixture, NodeProperties)
{
	node.props = blm::NodeProps::STATIC | blm::NodeProps::EXPORT;

	EXPECT_TRUE((node.props & blm::NodeProps::STATIC) == blm::NodeProps::STATIC);
	EXPECT_TRUE((node.props & blm::NodeProps::EXPORT) == blm::NodeProps::EXPORT);
	EXPECT_FALSE((node.props & blm::NodeProps::EXTERN) == blm::NodeProps::EXTERN);

	node.props |= blm::NodeProps::EXTERN;
	EXPECT_TRUE((node.props & blm::NodeProps::EXTERN) == blm::NodeProps::EXTERN);

	node.props &= ~blm::NodeProps::STATIC;
	EXPECT_FALSE((node.props & blm::NodeProps::STATIC) == blm::NodeProps::STATIC);

	node.props = blm::NodeProps::CONSTEXPR;
	EXPECT_TRUE((node.props & blm::NodeProps::CONSTEXPR) == blm::NodeProps::CONSTEXPR);

	node.props = blm::NodeProps::DRIVER;
	EXPECT_TRUE((node.props & blm::NodeProps::DRIVER) == blm::NodeProps::DRIVER);

	node.props = blm::NodeProps::NO_OPTIMIZE;
	EXPECT_TRUE((node.props & blm::NodeProps::NO_OPTIMIZE) == blm::NodeProps::NO_OPTIMIZE);
}

TEST_F(NodeFixture, NodeRelationships)
{
	blm::Node input1;
	blm::Node input2;
	blm::Node user1;
	blm::Node user2;

	node.inputs.push_back(&input1);
	node.inputs.push_back(&input2);
	node.users.push_back(&user1);
	node.users.push_back(&user2);

	EXPECT_EQ(node.inputs.size(), 2);
	EXPECT_EQ(node.users.size(), 2);
	EXPECT_EQ(node.inputs[0], &input1);
	EXPECT_EQ(node.inputs[1], &input2);
	EXPECT_EQ(node.users[0], &user1);
	EXPECT_EQ(node.users[1], &user2);
}

TEST_F(NodeFixture, TypedDataAccess)
{
	node.ir_type = blm::NodeType::LIT;
	node.type_kind = blm::DataType::INT32;
	node.data.set<std::int32_t, blm::DataType::INT32>(42);

	EXPECT_EQ(node.as<blm::DataType::INT32>(), 42);
}

TEST_F(NodeFixture, StructData)
{
	node.ir_type = blm::NodeType::LIT;
	node.type_kind = blm::DataType::STRUCT;

	blm::DataTypeTraits<blm::DataType::STRUCT>::type struct_data;
	struct_data.size = 16;
	struct_data.alignment = 8;
	struct_data.fields = {
		{ "x", blm::DataType::INT32 },
		{ "y", blm::DataType::FLOAT64 }
	};

	node.data.set<decltype(struct_data), blm::DataType::STRUCT>(std::move(struct_data));

	const auto &[size, alignment, fields] = node.as<blm::DataType::STRUCT>();
	EXPECT_EQ(size, 16);
	EXPECT_EQ(alignment, 8);
	ASSERT_EQ(fields.size(), 2);
	EXPECT_EQ(fields[0].first, "x");
	EXPECT_EQ(fields[0].second, blm::DataType::INT32);
	EXPECT_EQ(fields[1].first, "y");
	EXPECT_EQ(fields[1].second, blm::DataType::FLOAT64);
}

TEST_F(NodeFixture, PointerData)
{
	node.ir_type = blm::NodeType::PTR_LOAD;
	node.type_kind = blm::DataType::POINTER;

	blm::DataTypeTraits<blm::DataType::POINTER>::type ptr_data = {};
	ptr_data.pointee_type = blm::DataType::UINT64;
	ptr_data.addr_space = 1;

	node.data.set<decltype(ptr_data), blm::DataType::POINTER>(std::move(ptr_data));

	const auto &[pointee_type, addr_space] = node.as<blm::DataType::POINTER>();
	EXPECT_EQ(pointee_type, blm::DataType::UINT64);
	EXPECT_EQ(addr_space, 1);
}

TEST_F(NodeFixture, NodeRefAccess)
{
	blm::Node target;
	node.ir_type = blm::NodeType::LOAD;

	auto ptr_data = blm::DataTypeTraits<blm::DataType::POINTER>::type {};
	*reinterpret_cast<blm::Node **>(&ptr_data) = &target;
	node.data.set<decltype(ptr_data), blm::DataType::POINTER>(std::move(ptr_data));

	EXPECT_EQ(node.as_node_ref(), &target);

	node.ir_type = blm::NodeType::CALL;
	EXPECT_EQ(node.as_node_ref(), &target);
}

TEST_F(NodeFixture, NodeTypeOperations)
{
	node.ir_type = blm::NodeType::ADD;
	EXPECT_EQ(node.ir_type, blm::NodeType::ADD);

	node.ir_type = blm::NodeType::CALL;
	EXPECT_EQ(node.ir_type, blm::NodeType::CALL);

	node.ir_type = blm::NodeType::HEAP_ALLOC;
	EXPECT_EQ(node.ir_type, blm::NodeType::HEAP_ALLOC);

	const std::vector cf_nodes = {
		blm::NodeType::ENTRY, blm::NodeType::EXIT
	};

	const std::vector val_nodes = {
		blm::NodeType::PARAM, blm::NodeType::LIT
	};

	const std::vector op_nodes = {
		blm::NodeType::ADD, blm::NodeType::SUB, blm::NodeType::MUL, blm::NodeType::DIV,
		blm::NodeType::GT, blm::NodeType::GTE, blm::NodeType::LT, blm::NodeType::LTE,
		blm::NodeType::EQ, blm::NodeType::NEQ, blm::NodeType::BAND, blm::NodeType::BOR,
		blm::NodeType::BXOR, blm::NodeType::BNOT, blm::NodeType::BSHL, blm::NodeType::BSHR,
		blm::NodeType::RET
	};

	node.ir_type = cf_nodes[0];
	EXPECT_EQ(node.ir_type, blm::NodeType::ENTRY);

	node.ir_type = val_nodes[0];
	EXPECT_EQ(node.ir_type, blm::NodeType::PARAM);

	node.ir_type = op_nodes[0];
	EXPECT_EQ(node.ir_type, blm::NodeType::ADD);
}
