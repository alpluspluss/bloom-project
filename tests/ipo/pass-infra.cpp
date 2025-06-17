/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <sstream>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/ipo/pass-context.hpp>
#include <bloom/ipo/pass-manager.hpp>
#include <bloom/ipo/pass.hpp>
#include <gtest/gtest.h>

class IPOPassManagerFixture : public ::testing::Test
{
protected:
    class TestIPOPassA final : public blm::IPOPass
    {
    public:
        [[nodiscard]] const std::type_info &blm_id() const override
        {
            return typeid(TestIPOPassA);
        }

        [[nodiscard]] std::string_view name() const override
        {
            return "test-ipo-pass-a";
        }

        [[nodiscard]] std::string_view description() const override
        {
            return "Test IPO Pass A";
        }

        bool run(std::vector<blm::Module *> &modules, blm::IPOPassContext &context) override
        {
            run_count++;
            module_count = modules.size();

            /* update some stats */
            context.update_stat("test.modules_processed", modules.size());

            return success;
        }

        void set_success(bool succ)
        {
            success = succ;
        }

        [[nodiscard]] int get_run_count() const
        {
            return run_count;
        }

        [[nodiscard]] size_t get_module_count() const
        {
            return module_count;
        }

        void reset()
        {
            run_count = 0;
            module_count = 0;
        }

    private:
        bool success = true;
        int run_count = 0;
        size_t module_count = 0;
    };

    class TestIPOPassB final : public blm::IPOPass
    {
    public:
        [[nodiscard]] const std::type_info &blm_id() const override
        {
            return typeid(TestIPOPassB);
        }

        [[nodiscard]] std::string_view name() const override
        {
            return "test-ipo-pass-b";
        }

        [[nodiscard]] std::string_view description() const override
        {
            return "Test IPO Pass B";
        }

        bool run(std::vector<blm::Module *> &, blm::IPOPassContext &context) override
        {
            run_count++;
            if (context.get_stat("test.modules_processed") > 0)
                found_analysis = true;

            return success;
        }

        void set_success(bool succ)
        {
            success = succ;
        }

        [[nodiscard]] int get_run_count() const
        {
            return run_count;
        }

        [[nodiscard]] bool found_analysis_data() const
        {
            return found_analysis;
        }

        void reset()
        {
            run_count = 0;
            found_analysis = false;
        }

    private:
        bool success = true;
        int run_count = 0;
        bool found_analysis = false;
    };

    void SetUp() override
    {
        context = std::make_unique<blm::Context>();
        module1 = context->create_module("module1");
        module2 = context->create_module("module2");
        module3 = context->create_module("module3");

        modules = { module1, module2, module3 };

        manager = std::make_unique<blm::IPOPassManager>(modules, 1, true, 0);

        pass_a = std::make_unique<TestIPOPassA>();
        pass_b = std::make_unique<TestIPOPassB>();
    }

    void TearDown() override
    {
        manager.reset();
        context.reset();
    }

    std::unique_ptr<blm::Context> context;
    blm::Module *module1 = nullptr;
    blm::Module *module2 = nullptr;
    blm::Module *module3 = nullptr;
    std::vector<blm::Module *> modules;
    std::unique_ptr<blm::IPOPassManager> manager;

    std::unique_ptr<TestIPOPassA> pass_a;
    std::unique_ptr<TestIPOPassB> pass_b;
};

TEST_F(IPOPassManagerFixture, RunSinglePass)
{
    TestIPOPassA *raw_pass = pass_a.get();
    manager->add_pass(std::move(pass_a));

    bool result = manager->run_pass<TestIPOPassA>();
    EXPECT_TRUE(result);
    EXPECT_EQ(raw_pass->get_run_count(), 1);
    EXPECT_EQ(raw_pass->get_module_count(), 3);
}

TEST_F(IPOPassManagerFixture, RunAllPasses)
{
    TestIPOPassA *raw_pass_a = pass_a.get();
    TestIPOPassB *raw_pass_b = pass_b.get();

    manager->add_pass(std::move(pass_a));
    manager->add_pass(std::move(pass_b));
    manager->run_all();

    EXPECT_EQ(raw_pass_a->get_run_count(), 1);
    EXPECT_EQ(raw_pass_b->get_run_count(), 1);
    EXPECT_TRUE(raw_pass_b->found_analysis_data());
}

TEST_F(IPOPassManagerFixture, PassContext)
{
    auto &ctx = manager->get_context();

    EXPECT_EQ(ctx.modules().size(), 3);
    EXPECT_EQ(ctx.opt_level(), 1);
    EXPECT_TRUE(ctx.debug_mode());

    EXPECT_EQ(ctx.get_stat("nonexistent"), 0);
    ctx.update_stat("test.stat", 42);
    EXPECT_EQ(ctx.get_stat("test.stat"), 42);
}

TEST_F(IPOPassManagerFixture, FailureHandling)
{
    TestIPOPassA *raw_pass_a = pass_a.get();
    const TestIPOPassB *raw_pass_b = pass_b.get();

    raw_pass_a->set_success(false);

    manager->add_pass(std::move(pass_a));
    manager->add_pass(std::move(pass_b));
    manager->run_all();
    EXPECT_EQ(raw_pass_a->get_run_count(), 1);
    EXPECT_EQ(raw_pass_b->get_run_count(), 1); /* B still runs since no dependency system */
}

TEST_F(IPOPassManagerFixture, StatisticsPrinting)
{
    manager->add_pass<TestIPOPassA>();
    manager->add_pass<TestIPOPassB>();

    manager->run_all();

    std::stringstream output;
    manager->print_statistics(output);

    std::string stats = output.str();
    EXPECT_NE(stats.find("test-ipo-pass-a"), std::string::npos);
    EXPECT_NE(stats.find("test-ipo-pass-b"), std::string::npos);
    EXPECT_NE(stats.find("IPO statistics"), std::string::npos);
}

class IPOPassContextFixture : public ::testing::Test
{
protected:
    class MockIPOResult final : public blm::IPOAnalysisResult
    {
    public:
        explicit MockIPOResult(int id) : result_id(id) {}

        [[nodiscard]] bool invalidated_by(const std::type_info &type) const override
        {
            return type == typeid(int);
        }

        [[nodiscard]] std::unordered_set<blm::Module *> depends_on_modules() const override
        {
            return dependent_modules;
        }

        [[nodiscard]] int id() const
        {
            return result_id;
        }

        void add_dependency(blm::Module *mod)
        {
            dependent_modules.insert(mod);
        }

    private:
        int result_id;
        std::unordered_set<blm::Module *> dependent_modules;
    };

    void SetUp() override
    {
        context = std::make_unique<blm::Context>();
        module1 = context->create_module("module1");
        module2 = context->create_module("module2");
        modules = { module1, module2 };
        pass_context = std::make_unique<blm::IPOPassContext>(modules, 2, true);
    }

    void TearDown() override
    {
        pass_context.reset();
        context.reset();
    }

    std::unique_ptr<blm::Context> context;
    blm::Module *module1 = nullptr;
    blm::Module *module2 = nullptr;
    std::vector<blm::Module *> modules;
    std::unique_ptr<blm::IPOPassContext> pass_context;
};

TEST_F(IPOPassContextFixture, BasicProperties)
{
    EXPECT_EQ(pass_context->modules().size(), 2);
    EXPECT_EQ(pass_context->opt_level(), 2);
    EXPECT_TRUE(pass_context->debug_mode());
}

TEST_F(IPOPassContextFixture, SimpleResultInterface)
{
    EXPECT_FALSE(pass_context->has_result<MockIPOResult>());
    EXPECT_EQ(pass_context->get_result<MockIPOResult>(), nullptr);

    auto result = std::make_unique<MockIPOResult>(42);
    pass_context->store_result(std::move(result));

    EXPECT_TRUE(pass_context->has_result<MockIPOResult>());
    const auto *retrieved = pass_context->get_result<MockIPOResult>();
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id(), 42);
}

TEST_F(IPOPassContextFixture, AdvancedResultInterface)
{
    EXPECT_FALSE(pass_context->has_result("custom.key"));
    EXPECT_EQ(pass_context->get_result("custom.key"), nullptr);

    auto result = std::make_unique<MockIPOResult>(123);
    pass_context->store_result("custom.key", std::move(result));

    EXPECT_TRUE(pass_context->has_result("custom.key"));
    const auto *retrieved = pass_context->get_result("custom.key");
    EXPECT_NE(retrieved, nullptr);

    /* downcast to access specific methods */
    const auto *typed_result = dynamic_cast<const MockIPOResult *>(retrieved);
    EXPECT_NE(typed_result, nullptr);
    EXPECT_EQ(typed_result->id(), 123);
}

TEST_F(IPOPassContextFixture, PatternMatching)
{
    auto result1 = std::make_unique<MockIPOResult>(1);
    auto result2 = std::make_unique<MockIPOResult>(2);
    auto result3 = std::make_unique<MockIPOResult>(3);

    pass_context->store_result("call_graph.module1", std::move(result1));
    pass_context->store_result("call_graph.module2", std::move(result2));
    pass_context->store_result("escape_analysis.global", std::move(result3));

    EXPECT_TRUE(pass_context->has_result("call_graph.module1"));
    EXPECT_TRUE(pass_context->has_result("call_graph.module2"));
    EXPECT_TRUE(pass_context->has_result("escape_analysis.global"));

    pass_context->invalidate_matching("call_graph.*");

    EXPECT_FALSE(pass_context->has_result("call_graph.module1"));
    EXPECT_FALSE(pass_context->has_result("call_graph.module2"));
    EXPECT_TRUE(pass_context->has_result("escape_analysis.global"));
}
