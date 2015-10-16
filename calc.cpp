// Copyright 2015 Benoît Vey

#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Support/TargetSelect.h>

#include "Lexer.hpp"
#include "Parser.hpp"
#include "command_handler.hpp"
#include "syntax_tree.hpp"
#include "utility.hpp"

using namespace std::string_literals;

namespace
{

void update_vars(std::map<std::string, double>& variables, llvm::Module& module, llvm::ExecutionEngine& engine)
{
	for (auto& elem : variables)
	{
		if (!module.getGlobalVariable(elem.first))
			continue;
		auto var = engine.getGlobalValueAddress(elem.first);
		elem.second = *reinterpret_cast<double*>(var);
	}
}

} // namespace

int main()
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetAsmPrinter();

	llvm::IRBuilder<> builder{llvm::getGlobalContext()};

	std::map<std::string, double> variables;
	std::map<std::string, Function*> functions;

	Lexer lex{};
	Parser par{variables, functions};
	std::string in{};
	std::cout << "Use !help to print help.\n";
	std::cout << "Use Ctrl^D or !quit to exit.\n";
	bool stop{false};
	while (!stop)
	{
		try
		{
			std::cout << "> ";
			std::getline(std::cin, in);
			lex.newline(std::move(in));
			try
			{
				if (lex.peek() == '!' || lex.peek() == Token::eof)
				{
					auto c = parse_command(lex);
					switch (c.type)
					{
						case CommandType::help:
							execute_help(c.args.empty() ? nullptr : &c.args[0]);
							break;
						case CommandType::quit:
							stop = true;
							break;
						case CommandType::env:
							execute_env(c.args, variables, functions);
							break;
						case CommandType::import:
							execute_import(c.args, variables, functions);
							break;
						case CommandType::del:
							execute_del(c.args, variables, functions);
							break;
						case CommandType::def:
							execute_def(c.args, variables, functions, par, lex);
							break;
					}
					continue;
				}
			}
			catch (InvalidInput const& ex)
			{
				std::cout << "Invalid command : " << ex.what() << '\n';
				continue;
			}
			auto ast = par.parse(lex);

			auto module = std::make_unique<llvm::Module>("CalcMain", llvm::getGlobalContext());
			auto main_ref = module.get();
			auto calc_type = llvm::FunctionType::get(llvm::Type::getDoubleTy(llvm::getGlobalContext()), {}, false);
			auto calc_main = llvm::Function::Create(calc_type, llvm::Function::ExternalLinkage, "cmain", main_ref);

			auto block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", calc_main);
			builder.SetInsertPoint(block);
			builder.CreateRet(ast->codegen(*main_ref, builder));

			llvm::verifyFunction(*calc_main);

			std::unique_ptr<llvm::ExecutionEngine> engine{llvm::EngineBuilder{std::move(module)}.create()};
			engine->finalizeObject();

			std::cout << engine->runFunction(calc_main, {}).DoubleVal << '\n';

			update_vars(variables, *main_ref, *engine);
		}
		catch (InvalidInput const& ex)
		{
			std::cout << "Invalid input : " << ex.what() << '\n';
		}
	}
}
