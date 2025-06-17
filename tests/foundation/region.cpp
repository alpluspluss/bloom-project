/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/foundation/node.hpp>
#include <gtest/gtest.h>

class RegionFixture : public ::testing::Test
{
protected:
	void SetUp() override
	{
		context = std::make_unique<blm::Context>();
		module = std::make_unique<blm::Module>(*context, "test_module");
		region = module->get_root_region();
	}

	void TearDown() override
	{
		module.reset();
		context.reset();
	}

	std::unique_ptr<blm::Context> context;
	std::unique_ptr<blm::Module> module;
	blm::Region *region = nullptr;
};

TEST_F(RegionFixture, BasicProperties)
{
	EXPECT_EQ(region->get_name(), "test_module");
	EXPECT_EQ(region->get_parent(), nullptr);
	EXPECT_EQ(&region->get_module(), module.get());
}

TEST_F(RegionFixture, NodeManagement)
{
	auto *node1 = region->create_node<blm::Node>();
	auto *node2 = region->create_node<blm::Node>();

	EXPECT_EQ(region->get_nodes().size(), 2);
	EXPECT_EQ(region->get_nodes()[0], node1);
	EXPECT_EQ(region->get_nodes()[1], node2);

	region->remove_node(node1);
	EXPECT_EQ(region->get_nodes().size(), 1);
	EXPECT_EQ(region->get_nodes()[0], node2);
}

TEST_F(RegionFixture, DuplicateNodeAddition)
{
	auto *node = region->create_node<blm::Node>();

	region->add_node(node);
	EXPECT_EQ(region->get_nodes().size(), 1);
}

TEST_F(RegionFixture, RemoveNonexistentNode)
{
	[[maybe_unused]] auto *node1 = region->create_node<blm::Node>();
	auto *node2 = new blm::Node();

	EXPECT_EQ(region->get_nodes().size(), 1);

	region->remove_node(node2);
	EXPECT_EQ(region->get_nodes().size(), 1);

	region->remove_node(nullptr);
	EXPECT_EQ(region->get_nodes().size(), 1);

	delete node2;
}

TEST_F(RegionFixture, HierarchyNavigation)
{
	auto *child1 = module->create_region("child1", region);
	auto *child2 = module->create_region("child2", region);
	auto *grandchild = module->create_region("grandchild", child1);

	EXPECT_EQ(region->get_children().size(), 2);
	EXPECT_EQ(child1->get_parent(), region);
	EXPECT_EQ(child2->get_parent(), region);
	EXPECT_EQ(grandchild->get_parent(), child1);

	EXPECT_EQ(child1->get_children().size(), 1);
	EXPECT_EQ(child1->get_children()[0], grandchild);
}

TEST_F(RegionFixture, InvalidChildAddition)
{
	auto *child1 = module->create_region("child1", region);
	auto *child2 = module->create_region("child2", region);

	child1->add_child(child2);
	EXPECT_EQ(child1->get_children().size(), 1);

	child1->add_child(nullptr);
	EXPECT_EQ(child1->get_children().size(), 1);
}

TEST_F(RegionFixture, NodeCreationInRegion)
{
	auto *node = region->create_node<blm::Node>();
	node->ir_type = blm::NodeType::ADD;
	node->type_kind = blm::DataType::INT32;

	EXPECT_EQ(region->get_nodes().size(), 1);
	EXPECT_EQ(region->get_nodes()[0]->ir_type, blm::NodeType::ADD);
	EXPECT_EQ(region->get_nodes()[0]->type_kind, blm::DataType::INT32);
}

TEST_F(RegionFixture, MultipleNodeTypesInRegion)
{
	auto *add_node = region->create_node<blm::Node>();
	add_node->ir_type = blm::NodeType::ADD;

	auto *sub_node = region->create_node<blm::Node>();
	sub_node->ir_type = blm::NodeType::SUB;

	auto *mul_node = region->create_node<blm::Node>();
	mul_node->ir_type = blm::NodeType::MUL;

	EXPECT_EQ(region->get_nodes().size(), 3);
	EXPECT_EQ(region->get_nodes()[0]->ir_type, blm::NodeType::ADD);
	EXPECT_EQ(region->get_nodes()[1]->ir_type, blm::NodeType::SUB);
	EXPECT_EQ(region->get_nodes()[2]->ir_type, blm::NodeType::MUL);
}

TEST_F(RegionFixture, DebugInfoBasics)
{
	auto &debug_info = region->get_debug_info();
	EXPECT_EQ(&debug_info.get_region(), region);
}

TEST_F(RegionFixture, DebugInfoSourceFiles)
{
	auto &debug_info = region->get_debug_info();

	auto file_id = debug_info.add_source_file("test.cpp");
	EXPECT_NE(file_id, 0);

	auto file_path = debug_info.get_source_file(file_id);
	EXPECT_EQ(file_path, "test.cpp");
}

TEST_F(RegionFixture, DebugInfoLocationTracking)
{
	auto &debug_info = region->get_debug_info();
	auto file_id = debug_info.add_source_file("source.cpp");

	auto *node = region->create_node<blm::Node>();
	debug_info.set_node_location(node, file_id, 42, 10);

	auto loc_opt = debug_info.get_node_location(node);
	ASSERT_TRUE(loc_opt.has_value());

	auto &loc = loc_opt.value();
	EXPECT_EQ(loc.file_id, file_id);
	EXPECT_EQ(loc.line, 42);
	EXPECT_EQ(loc.column, 10);
}

TEST_F(RegionFixture, CreateNodeWithLocation)
{
	auto &debug_info = region->get_debug_info();
	auto file_id = debug_info.add_source_file("source.cpp");

	auto *node = region->create_node_with_location<blm::Node>(file_id, 100, 5);

	auto loc_opt = debug_info.get_node_location(node);
	ASSERT_TRUE(loc_opt.has_value());
	EXPECT_EQ(loc_opt->line, 100);
	EXPECT_EQ(loc_opt->column, 5);
}

TEST_F(RegionFixture, FindNodesByLocation)
{
	auto &debug_info = region->get_debug_info();
	auto file_id = debug_info.add_source_file("source.cpp");

	auto *node1 = region->create_node_with_location<blm::Node>(file_id, 15, 8);
	auto *node2 = region->create_node_with_location<blm::Node>(file_id, 15, 8);

	auto nodes = debug_info.find_nodes_at_location(file_id, 15, 8);

	ASSERT_EQ(nodes.size(), 2);
	EXPECT_EQ(nodes[0], node1);
	EXPECT_EQ(nodes[1], node2);
}

TEST_F(RegionFixture, DebugInfoVariables)
{
	auto &debug_info = region->get_debug_info();
	auto file_id = debug_info.add_source_file("source.cpp");

	auto *var_node = region->create_node_with_location<blm::Node>(file_id, 5, 10);
	var_node->ir_type = blm::NodeType::PARAM;

	debug_info.add_variable(var_node, "counter", "int32", true, -12);

	auto [name, type, is_param, offset] = debug_info.get_variable_info(var_node);

	EXPECT_EQ(name, "counter");
	EXPECT_EQ(type, "int32");
	EXPECT_TRUE(is_param);
	EXPECT_EQ(offset, -12);
}

TEST_F(RegionFixture, DebugInfoFunctions)
{
	auto &debug_info = region->get_debug_info();
	auto file_id = debug_info.add_source_file("source.cpp");

	auto *func_node = region->create_node_with_location<blm::Node>(file_id, 1, 1);
	func_node->ir_type = blm::NodeType::FUNCTION;

	auto *param_node = region->create_node_with_location<blm::Node>(file_id, 2, 5);
	param_node->ir_type = blm::NodeType::PARAM;

	auto *local_var = region->create_node_with_location<blm::Node>(file_id, 3, 10);
	local_var->ir_type = blm::NodeType::LIT;

	debug_info.add_function(func_node, "calculate");
	debug_info.add_parameter_to_function(func_node, param_node);
	debug_info.add_local_var_to_function(func_node, local_var);

	auto params = debug_info.get_function_parameters(func_node);
	auto locals = debug_info.get_function_local_vars(func_node);

	ASSERT_EQ(params.size(), 1);
	EXPECT_EQ(params[0], param_node);

	ASSERT_EQ(locals.size(), 1);
	EXPECT_EQ(locals[0], local_var);
}

TEST_F(RegionFixture, DebugInfoTypes)
{
	auto &debug_info = region->get_debug_info();

	auto type_id = debug_info.add_type("Matrix4x4", 64, 16);

	auto [name, size, alignment] = debug_info.get_type_info(type_id);

	EXPECT_EQ(name, "Matrix4x4");
	EXPECT_EQ(size, 64);
	EXPECT_EQ(alignment, 16);
}

TEST_F(RegionFixture, DebugInfoWithChildRegions)
{
	auto *child_region = module->create_region("child", region);

	auto &parent_debug = region->get_debug_info();
	auto &child_debug = child_region->get_debug_info();

	auto parent_file_id = parent_debug.add_source_file("parent.cpp");
	auto child_file_id = child_debug.add_source_file("child.cpp");

	auto *parent_node = region->create_node_with_location<blm::Node>(parent_file_id, 1, 1);
	auto *child_node = child_region->create_node_with_location<blm::Node>(child_file_id, 2, 2);

	auto parent_loc = parent_debug.get_node_location(parent_node);
	auto child_loc_in_parent = parent_debug.get_node_location(child_node);

	ASSERT_TRUE(parent_loc.has_value());
	EXPECT_FALSE(child_loc_in_parent.has_value());

	auto parent_loc_in_child = child_debug.get_node_location(parent_node);
	auto child_loc = child_debug.get_node_location(child_node);

	EXPECT_FALSE(parent_loc_in_child.has_value());
	ASSERT_TRUE(child_loc.has_value());
}

TEST_F(RegionFixture, DebugInfoAfterNodeRemoval)
{
	auto &debug_info = region->get_debug_info();
	auto file_id = debug_info.add_source_file("source.cpp");

	auto *node = region->create_node_with_location<blm::Node>(file_id, 42, 10);

	auto loc_before = debug_info.get_node_location(node);
	ASSERT_TRUE(loc_before.has_value());

	region->remove_node(node);

	auto loc_after = debug_info.get_node_location(node);
	ASSERT_TRUE(loc_after.has_value());
	EXPECT_EQ(loc_after->line, 42);
}

TEST_F(RegionFixture, InsertNodeBefore)
{
	auto *node1 = region->create_node<blm::Node>();
	auto *node2 = region->create_node<blm::Node>();
	auto *node3 = context->create<blm::Node>();

	/* [node1, node2] */
	EXPECT_EQ(region->get_nodes().size(), 2);
	EXPECT_EQ(region->get_nodes()[0], node1);
	EXPECT_EQ(region->get_nodes()[1], node2);

	/* insert node3 before node2 */
	region->insert_node_before(node2, node3);

	/* [node1, node3, node2] */
	EXPECT_EQ(region->get_nodes().size(), 3);
	EXPECT_EQ(region->get_nodes()[0], node1);
	EXPECT_EQ(region->get_nodes()[1], node3);
	EXPECT_EQ(region->get_nodes()[2], node2);

	auto *nonexistent = context->create<blm::Node>();
	auto *node4 = context->create<blm::Node>();

	region->insert_node_before(nonexistent, node4);

	/* `node4` should be added at the end since nonexistent isn't in the region */
	EXPECT_EQ(region->get_nodes().size(), 4);
	EXPECT_EQ(region->get_nodes()[3], node4);
}

TEST_F(RegionFixture, InsertAtBeginning)
{
	auto *node1 = region->create_node<blm::Node>();
	auto *node2 = region->create_node<blm::Node>();

	/* [node1, node2] */
	EXPECT_EQ(region->get_nodes().size(), 2);
	EXPECT_EQ(region->get_nodes()[0], node1);
	EXPECT_EQ(region->get_nodes()[1], node2);

	auto *node3 = context->create<blm::Node>();
	region->insert_at_beginning(node3);

	/* [node3, node1, node2] */
	EXPECT_EQ(region->get_nodes().size(), 3);
	EXPECT_EQ(region->get_nodes()[0], node3);
	EXPECT_EQ(region->get_nodes()[1], node1);
	EXPECT_EQ(region->get_nodes()[2], node2);

	region->insert_at_beginning(nullptr);
	EXPECT_EQ(region->get_nodes().size(), 3);
	EXPECT_EQ(region->get_nodes()[0], node3);
}

TEST_F(RegionFixture, IsTerminated)
{
	EXPECT_FALSE(region->is_terminated());

	auto *node1 = region->create_node<blm::Node>();
	node1->ir_type = blm::NodeType::ADD;
	EXPECT_FALSE(region->is_terminated());

	auto *ret_node = region->create_node<blm::Node>();
	ret_node->ir_type = blm::NodeType::RET;
	EXPECT_TRUE(region->is_terminated());

	auto *exit_region = module->create_region("exit_region");
	auto *exit_node = exit_region->create_node<blm::Node>();
	exit_node->ir_type = blm::NodeType::EXIT;

	EXPECT_FALSE(exit_region->is_terminated());

	auto *mixed_region = module->create_region("mixed_region");
	auto *exit_in_middle = mixed_region->create_node<blm::Node>();
	exit_in_middle->ir_type = blm::NodeType::EXIT;

	auto *node_after_exit = mixed_region->create_node<blm::Node>();
	node_after_exit->ir_type = blm::NodeType::ADD;
	EXPECT_FALSE(mixed_region->is_terminated());

	auto *final_ret = mixed_region->create_node<blm::Node>();
	final_ret->ir_type = blm::NodeType::RET;
	EXPECT_TRUE(mixed_region->is_terminated());
}

TEST_F(RegionFixture, SSAPropertyPreservation)
{
	auto *param_node = region->create_node<blm::Node>();
	param_node->ir_type = blm::NodeType::PARAM;
	param_node->type_kind = blm::DataType::INT32;

	auto *add_node = region->create_node<blm::Node>();
	add_node->ir_type = blm::NodeType::ADD;
	add_node->type_kind = blm::DataType::INT32;

	add_node->inputs.push_back(param_node);
	param_node->users.push_back(add_node);

	auto *ret_node = region->create_node<blm::Node>();
	ret_node->ir_type = blm::NodeType::RET;

	ret_node->inputs.push_back(add_node);
	add_node->users.push_back(ret_node);

	EXPECT_EQ(region->get_nodes().size(), 3);

	auto *mul_node = context->create<blm::Node>();
	mul_node->ir_type = blm::NodeType::MUL;
	mul_node->type_kind = blm::DataType::INT32;

	region->insert_node_before(ret_node, mul_node);

	ret_node->inputs.clear();
	ret_node->inputs.push_back(mul_node);
	mul_node->users.push_back(ret_node);

	/* expected order: [param_node, add_node, mul_node, ret_node] */
	EXPECT_EQ(region->get_nodes().size(), 4);
	EXPECT_EQ(region->get_nodes()[0], param_node);
	EXPECT_EQ(region->get_nodes()[1], add_node);
	EXPECT_EQ(region->get_nodes()[2], mul_node);
	EXPECT_EQ(region->get_nodes()[3], ret_node);

	EXPECT_TRUE(region->is_terminated());

	int param_pos = -1;
	int add_pos = -1;
	int mul_pos = -1;
	int ret_pos = -1;

	for (size_t i = 0; i < region->get_nodes().size(); i++)
	{
		if (region->get_nodes()[i] == param_node)
			param_pos = i;
		if (region->get_nodes()[i] == add_node)
			add_pos = i;
		if (region->get_nodes()[i] == mul_node)
			mul_pos = i;
		if (region->get_nodes()[i] == ret_node)
			ret_pos = i;
	}

	EXPECT_LT(param_pos, add_pos);
	EXPECT_LT(mul_pos, ret_pos);
}
