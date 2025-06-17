/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/analysis-pass.hpp>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <gtest/gtest.h>

class AnalysisPassFixture : public ::testing::Test
{
protected:
	void SetUp() override
	{
		context = std::make_unique<blm::Context>();
		module = context->create_module("test_module");
		pass_context = std::make_unique<blm::PassContext>(*module, 0, false);
	}

	void TearDown() override
	{
		pass_context.reset();
		context.reset();
	}

	std::unique_ptr<blm::Context> context;
	blm::Module *module = nullptr;
	std::unique_ptr<blm::PassContext> pass_context;
};

class TestResult final : public blm::AnalysisResult
{
public:
	explicit TestResult(const int value) : v(value) {}

	[[nodiscard]] bool invalidated_by(const std::type_info &transform_type) const override
	{
		return invb(transform_type);
	}

	void set_invalidated_by(std::function<bool(const std::type_info &)> fn)
	{
		invb = std::move(fn);
	}

	[[nodiscard]] int value() const
	{
		return v;
	}

private:
	int v;
	std::function<bool(const std::type_info &)> invb =
			[](const std::type_info &)
	{
		return false;
	};
};

class TestAnalysisPass final : public blm::AnalysisPass
{
public:
	[[nodiscard]] const std::type_info &blm_id() const override
	{
		return typeid(TestAnalysisPass);
	}

	[[nodiscard]] std::string_view name() const override
	{
		return "test-analysis";
	}

	[[nodiscard]] std::string_view description() const override
	{
		return "A test analysis pass";
	}

	std::unique_ptr<blm::AnalysisResult> analyze(blm::Module &,
	                                             blm::PassContext &) override
	{
		analysis_c++;
		if (!analysis_s)
			return nullptr;
		return std::make_unique<TestResult>(resv);
	}
	
	void set_analysis_succeeds(const bool succeeds)
	{
		analysis_s = succeeds;
	}

	void set_result_value(const int value)
	{
		resv = value;
	}

	[[nodiscard]] int analyze_called() const
	{
		return analysis_c;
	}

private:
	bool analysis_s = true;
	int resv = 42;
	int analysis_c = 0;
};

class TestTransformPass final : public blm::Pass
{
public:
	[[nodiscard]] const std::type_info &blm_id() const override
	{
		return typeid(TestTransformPass);
	}

	[[nodiscard]] std::string_view name() const override
	{
		return "test-transform";
	}

	[[nodiscard]] std::string_view description() const override
	{
		return "Test transform";
	}

	bool run(blm::Module &, blm::PassContext &) override
	{
		return true;
	}
};

TEST_F(AnalysisPassFixture, RunStoresResult)
{
	TestAnalysisPass pass;

	pass.set_result_value(42);
	const bool result = pass.run(*module, *pass_context);
	EXPECT_TRUE(result);
	EXPECT_EQ(pass.analyze_called(), 1);
	
	const auto *analysis_result = pass_context->get_result<TestResult>(pass.blm_id());
	EXPECT_NE(analysis_result, nullptr);
	EXPECT_EQ(analysis_result->value(), 42);
}

TEST_F(AnalysisPassFixture, FailedAnalysisReturnsFailure)
{
	TestAnalysisPass pass;
	pass.set_analysis_succeeds(false);

	const bool result = pass.run(*module, *pass_context);
	EXPECT_FALSE(result);
	EXPECT_EQ(pass.analyze_called(), 1);
	
	const auto *analysis_result = pass_context->get_result<TestResult>();
	EXPECT_EQ(analysis_result, nullptr);
}

TEST_F(AnalysisPassFixture, ResultInvalidation)
{
	auto result = std::make_unique<TestResult>(123);
	result->set_invalidated_by([](const std::type_info &type)
	{
		return type == typeid(TestTransformPass);
	});
	
	pass_context->store_result(typeid(TestAnalysisPass), std::move(result));
	
	EXPECT_TRUE(pass_context->has_result(typeid(TestAnalysisPass)));
	
	pass_context->invalidate_by(typeid(int));
	EXPECT_TRUE(pass_context->has_result(typeid(TestAnalysisPass)));
	
	pass_context->invalidate_by(typeid(TestTransformPass));
	EXPECT_FALSE(pass_context->has_result(typeid(TestAnalysisPass)));
}
