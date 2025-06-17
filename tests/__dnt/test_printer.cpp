/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <iostream>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/ir/builder.hpp>
#include <bloom/ir/print.hpp>
#include <bloom/transform/cse.hpp>

int main()
{
	auto ctx = blm::Context();
	auto b = blm::Builder(ctx);

	auto* module = b.create_module("test");

	auto* global1 = b.literal(42);
	auto* global2 = b.literal(10);
	auto func_builder = b.create_function("test", {}, blm::DataType::INT32);
	func_builder.body([&]()
	{
		auto* expr1 = b.add(global1, global2);
		auto* expr2 = b.add(global1, global2);
		auto* result = b.mul(expr1, expr2);
		b.ret(result);
	});
	func_builder.get_function()->props |= blm::NodeProps::DRIVER;

	std::cout << "before optimization\n";
	blm::IRPrinter printer(std::cout);
	printer.print_module(*module);
	blm::PassContext pass_context(*module);
	auto cse = blm::CSEPass();
	auto aa = blm::LocalAliasAnalysisPass();
	aa.run(*module, pass_context);
	cse.run(*module, pass_context);
	std::cout << "after optimization\n";
	printer.print_module(*module);
	return 0;
}
