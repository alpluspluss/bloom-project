/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass.hpp>
#include <gtest/gtest.h>

class PassFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        context = std::make_unique<blm::Context>();
        module = context->create_module("test_module");
    }

    void TearDown() override
    {
        context.reset();
    }

    std::unique_ptr<blm::Context> context;
    blm::Module *module = nullptr;
};

class TestPass final : public blm::Pass
{
public:
    explicit TestPass(int min_opt_level = 0) : min_opt_lvl(min_opt_level) {}

    [[nodiscard]] const std::type_info& blm_id() const override
    {
        return typeid(TestPass);
    }

    [[nodiscard]] std::string_view name() const override
    {
        return "test-pass";
    }

    [[nodiscard]] std::string_view description() const override
    {
        return "A test pass for unit testing";
    }

    [[nodiscard]] std::vector<const std::type_info*> required_passes() const override
    {
        return required;
    }

    [[nodiscard]] std::vector<const std::type_info*> invalidated_passes() const override
    {
        return invalidated;
    }

    bool run(blm::Module&, blm::PassContext&) override
    {
        run_c++;
        return succ;
    }

    [[nodiscard]] bool run_at_opt_level(int level) const override
    {
        return level >= min_opt_lvl;
    }

    void set_required(std::vector<const std::type_info*> required)
    {
        this->required = std::move(required);
    }

    void set_invalidated(std::vector<const std::type_info*> invalidated)
    {
        this->invalidated = std::move(invalidated);
    }

    void set_success(bool success)
    {
        succ = success;
    }

    [[nodiscard]] int run_count() const
    {
        return run_c;
    }

private:
    int min_opt_lvl = 0;
    int run_c = 0;
    bool succ = true;
    std::vector<const std::type_info*> required;
    std::vector<const std::type_info*> invalidated;
};

TEST_F(PassFixture, BasicProperties)
{
    TestPass pass;

    EXPECT_EQ(pass.name(), "test-pass");
    EXPECT_EQ(pass.description(), "A test pass for unit testing");
    EXPECT_EQ(pass.blm_id(), typeid(TestPass));
    EXPECT_TRUE(pass.required_passes().empty());
    EXPECT_TRUE(pass.invalidated_passes().empty());
}

TEST_F(PassFixture, OptimizationLevelHandling)
{
    TestPass pass_no_min;
    EXPECT_TRUE(pass_no_min.run_at_opt_level(0));
    EXPECT_TRUE(pass_no_min.run_at_opt_level(1));
    EXPECT_TRUE(pass_no_min.run_at_opt_level(2));
    EXPECT_TRUE(pass_no_min.run_at_opt_level(3));

    TestPass pass_level_2(2);
    EXPECT_FALSE(pass_level_2.run_at_opt_level(0));
    EXPECT_FALSE(pass_level_2.run_at_opt_level(1));
    EXPECT_TRUE(pass_level_2.run_at_opt_level(2));
    EXPECT_TRUE(pass_level_2.run_at_opt_level(3));
}

TEST_F(PassFixture, PassTypesHelper)
{
    class TestPassA : public blm::Pass { /* ... */ };
    class TestPassB : public blm::Pass { /* ... */ };
    class TestPassC : public blm::Pass { /* ... */ };

    auto types = blm::Pass::get_pass_types<TestPassA, TestPassB, TestPassC>();

    EXPECT_EQ(types.size(), 3);
    EXPECT_EQ(*types[0], typeid(TestPassA));
    EXPECT_EQ(*types[1], typeid(TestPassB));
    EXPECT_EQ(*types[2], typeid(TestPassC));
}
