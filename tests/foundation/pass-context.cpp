/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <gtest/gtest.h>

class PassContextFixture : public ::testing::Test
{
protected:
    class MockResult final : public blm::AnalysisResult
    {
    public:
        explicit MockResult(const int id) : i(id) {}

        [[nodiscard]] bool invalidated_by(const std::type_info& type) const override
        {
            return type == typeid(int);
        }

        [[nodiscard]] int id() const { return i; }

    private:
        int i;
    };

    void SetUp() override
    {
        context = std::make_unique<blm::Context>();
        module = context->create_module("test_module");
        pass_context = std::make_unique<blm::PassContext>(*module, 2, true);
    }

    void TearDown() override
    {
        pass_context.reset();
        context.reset();
    }

    std::unique_ptr<blm::Context> context;
    blm::Module* module = nullptr;
    std::unique_ptr<blm::PassContext> pass_context;
};

TEST_F(PassContextFixture, ConstructorAndBasicProperties)
{
    EXPECT_EQ(&pass_context->module(), module);
    EXPECT_EQ(pass_context->opt_level(), 2);
    EXPECT_TRUE(pass_context->debug_mode());

    blm::PassContext ctx2(*module, 0, false);
    EXPECT_EQ(&ctx2.module(), module);
    EXPECT_EQ(ctx2.opt_level(), 0);
    EXPECT_FALSE(ctx2.debug_mode());
}

TEST_F(PassContextFixture, ResultManagement)
{
    EXPECT_FALSE(pass_context->has_result(typeid(int)));
    EXPECT_EQ(pass_context->get_result<MockResult>(), nullptr);

    auto result1 = std::make_unique<MockResult>(1);
    pass_context->store_result(typeid(int), std::move(result1));

    EXPECT_TRUE(pass_context->has_result(typeid(int)));
    const auto* retrieved1 = pass_context->get_result<MockResult>(typeid(int));
    EXPECT_NE(retrieved1, nullptr);
    EXPECT_EQ(retrieved1->id(), 1);

    auto result2 = std::make_unique<MockResult>(2);
    pass_context->store_result(typeid(float), std::move(result2));

    EXPECT_TRUE(pass_context->has_result(typeid(int)));
    EXPECT_TRUE(pass_context->has_result(typeid(float)));

    pass_context->invalidate(typeid(int));
    EXPECT_FALSE(pass_context->has_result(typeid(int)));
    EXPECT_TRUE(pass_context->has_result(typeid(float)));

    const auto* retrieved2 = pass_context->get_result<MockResult>(typeid(float));
    EXPECT_NE(retrieved2, nullptr);
    EXPECT_EQ(retrieved2->id(), 2);
}

TEST_F(PassContextFixture, InvalidateByTransform)
{
    auto result1 = std::make_unique<MockResult>(1);
    auto result2 = std::make_unique<MockResult>(2);
    auto result3 = std::make_unique<MockResult>(3);

    pass_context->store_result(typeid(int), std::move(result1));
    pass_context->store_result(typeid(float), std::move(result2));
    pass_context->store_result(typeid(double), std::move(result3));

    EXPECT_TRUE(pass_context->has_result(typeid(int)));
    EXPECT_TRUE(pass_context->has_result(typeid(float)));
    EXPECT_TRUE(pass_context->has_result(typeid(double)));

    pass_context->invalidate_by(typeid(int));

    EXPECT_FALSE(pass_context->has_result(typeid(int)));
    EXPECT_FALSE(pass_context->has_result(typeid(float)));
    EXPECT_FALSE(pass_context->has_result(typeid(double)));
}

TEST_F(PassContextFixture, Statistics)
{
    EXPECT_EQ(pass_context->get_stat("test"), 0);

    pass_context->update_stat("test", 5);
    EXPECT_EQ(pass_context->get_stat("test"), 5);

    pass_context->update_stat("test", 3);
    EXPECT_EQ(pass_context->get_stat("test"), 8);

    pass_context->update_stat("another", 10);
    EXPECT_EQ(pass_context->get_stat("test"), 8);
    EXPECT_EQ(pass_context->get_stat("another"), 10);

    pass_context->update_stat("test", -2);
    EXPECT_EQ(pass_context->get_stat("test"), 6);
}
