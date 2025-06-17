/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <sstream>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-manager.hpp>
#include <bloom/foundation/pass.hpp>
#include <bloom/foundation/region.hpp>
#include <gtest/gtest.h>

class PassManagerFixture : public ::testing::Test
{
protected:
    class TestPassA final : public blm::Pass
    {
    public:
        [[nodiscard]] const std::type_info& blm_id() const override { return typeid(TestPassA); }
        [[nodiscard]] std::string_view name() const override { return "test-pass-a"; }
        [[nodiscard]] std::string_view description() const override { return "Test Pass A"; }

        bool run(blm::Module& mod, blm::PassContext&) override
        {
            rc++;

            auto* node = mod.get_root_region()->create_node<blm::Node>();
            node->ir_type = blm::NodeType::LIT;

            return succ;
        }

        void set_success(const bool success) { succ = success; }
        [[nodiscard]] int run_count() const { return rc; }
        void reset_run_count() { rc = 0; }

    private:
        bool succ = true;
        int rc = 0;
    };

    class TestPassB final : public blm::Pass
    {
    public:
        [[nodiscard]] const std::type_info& blm_id() const override { return typeid(TestPassB); }
        [[nodiscard]] std::string_view name() const override { return "test-pass-b"; }
        [[nodiscard]] std::string_view description() const override { return "Test Pass B"; }

        [[nodiscard]] std::vector<const std::type_info*> required_passes() const override
        {
            return {&typeid(TestPassA)};
        }

        bool run(blm::Module&, blm::PassContext&) override
        {
            runc++;
            return sec;
        }

        void set_success(const bool success) { sec = success; }
        [[nodiscard]] int run_count() const { return runc; }
        void reset_run_count() { runc = 0; }

    private:
        bool sec = true;
        int runc = 0;
    };

    class TestPassC final : public blm::Pass
    {
    public:
        [[nodiscard]] const std::type_info& blm_id() const override { return typeid(TestPassC); }
        [[nodiscard]] std::string_view name() const override { return "test-pass-c"; }
        [[nodiscard]] std::string_view description() const override { return "Test Pass C"; }

        [[nodiscard]] bool run_at_opt_level(int level) const override
        {
            return level >= minlvl;
        }

        bool run(blm::Module&, blm::PassContext&) override
        {
            runc++;
            return suc;
        }

        void set_min_level(int level) { minlvl = level; }
        void set_success(bool success) { suc = success; }
        [[nodiscard]] int run_count() const { return runc; }
        void reset_run_count() { runc = 0; }

    private:
        int minlvl = 0;
        bool suc = true;
        int runc = 0;
    };

    void SetUp() override
    {
        context = std::make_unique<blm::Context>();
        module = context->create_module("test_module");
        manager = std::make_unique<blm::PassManager>(*module, 1, true, 0);

        pass_a = std::make_unique<TestPassA>();
        pass_b = std::make_unique<TestPassB>();
        pass_c = std::make_unique<TestPassC>();
    }

    void TearDown() override
    {
        manager.reset();
        context.reset();
    }

    std::unique_ptr<blm::Context> context;
    blm::Module* module = nullptr;
    std::unique_ptr<blm::PassManager> manager;

    std::unique_ptr<TestPassA> pass_a;
    std::unique_ptr<TestPassB> pass_b;
    std::unique_ptr<TestPassC> pass_c;
};

TEST_F(PassManagerFixture, RunSinglePass)
{
    manager->add_pass<TestPassA>();

    bool result = manager->run_pass<TestPassA>();
    EXPECT_TRUE(result);
}

TEST_F(PassManagerFixture, RunAllPasses)
{
    manager->add_pass<TestPassA>();
    manager->add_pass<TestPassB>();

    bool result = manager->run_all();
    EXPECT_TRUE(result);
}

TEST_F(PassManagerFixture, PassDependencies)
{
    manager->add_pass<TestPassA>();
    manager->add_pass<TestPassB>();

    manager->run_pass<TestPassB>();

    bool result = manager->run_all();
    EXPECT_TRUE(result);
}

TEST_F(PassManagerFixture, OptimizationLevelFiltering)
{
    TestPassC* raw_pass_c = pass_c.get();
    raw_pass_c->set_min_level(2);
    raw_pass_c->reset_run_count();

    manager->add_pass(std::move(pass_c));

    bool result = manager->run_pass<TestPassC>();
    EXPECT_TRUE(result);
    EXPECT_EQ(raw_pass_c->run_count(), 0);

    auto manager2 = std::make_unique<blm::PassManager>(*module, 2, true, 0);
    auto pass_c2 = std::make_unique<TestPassC>();
    TestPassC* raw_pass_c2 = pass_c2.get();

    raw_pass_c2->set_min_level(2);
    raw_pass_c2->reset_run_count();

    manager2->add_pass(std::move(pass_c2));

    result = manager2->run_pass<TestPassC>();
    EXPECT_TRUE(result);
    EXPECT_EQ(raw_pass_c2->run_count(), 1);
}

TEST_F(PassManagerFixture, FailureHandling)
{
    TestPassA* raw_pass_a = pass_a.get();
    TestPassB* raw_pass_b = pass_b.get();

    raw_pass_a->set_success(false);
    raw_pass_a->reset_run_count();
    raw_pass_b->reset_run_count();

    manager->add_pass(std::move(pass_a));
    manager->add_pass(std::move(pass_b));

    const bool result = manager->run_all();
    EXPECT_FALSE(result);
    EXPECT_EQ(raw_pass_a->run_count(), 1);
    EXPECT_EQ(raw_pass_b->run_count(), 0);
}

TEST_F(PassManagerFixture, StatisticsPrinting)
{
    manager->add_pass<TestPassA>();
    manager->add_pass<TestPassB>();

    manager->run_all();

    std::stringstream output;
    manager->print_statistics(output);

    std::string stats = output.str();
    EXPECT_NE(stats.find("test-pass-a"), std::string::npos);
    EXPECT_NE(stats.find("test-pass-b"), std::string::npos);
    EXPECT_NE(stats.find("ms"), std::string::npos);
}
