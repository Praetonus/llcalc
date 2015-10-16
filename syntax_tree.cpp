// Copyright 2015 Benoît Vey

#include "syntax_tree.hpp"

#include <iostream>

#include "Parser.hpp"

using namespace std::string_literals;

llvm::Value* NumberTree::codegen(llvm::Module&, llvm::IRBuilder<>&)
{
	return llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(number_));
}

llvm::Value* IdentifierTree::codegen(llvm::Module& main, llvm::IRBuilder<>& builder)
{
	llvm::GlobalVariable* var{nullptr};
	if (vars_.find(label_) == std::end(vars_))
	{
		auto err = ""s;
		if (funs_.find(label_) != std::end(funs_))
			err = ". Maybe you meant to use the function?";
		throw InvalidInput{"Undeclared identifier : " + label_ + err};
	}
	if (!(var = main.getGlobalVariable(label_)))
		var = new llvm::GlobalVariable{main, llvm::Type::getDoubleTy(llvm::getGlobalContext()), false,
		                               llvm::GlobalVariable::ExternalLinkage,
		                               llvm::ConstantFP::get(llvm::getGlobalContext(),
		                               						 llvm::APFloat{vars_[label_]}),
		                               label_};
	return builder.CreateLoad(var);
}

llvm::Value* UnaryExprTree::codegen(llvm::Module& main, llvm::IRBuilder<>& builder)
{
	auto strep = st_->codegen(main, builder);

	switch (op_)
	{
		case '-':
			return builder.CreateFNeg(strep, "neg");
		default:
			throw InvalidInput{"Invalid unary operator : "s + op_};
	}
}

llvm::Value* BinaryExprTree::codegen(llvm::Module& main, llvm::IRBuilder<>& builder)
{
	auto lrep = lhs_->codegen(main, builder);
	auto rrep = rhs_->codegen(main, builder);

	std::vector<llvm::Type*> args_type{llvm::Type::getDoubleTy(llvm::getGlobalContext()),
		llvm::Type::getDoubleTy(llvm::getGlobalContext())};
	auto pow_fn = llvm::Intrinsic::getDeclaration(&main, llvm::Intrinsic::pow, args_type);

	switch (op_)
	{
		case '+':
			return builder.CreateFAdd(lrep, rrep, "add");
		case '-':
			return builder.CreateFSub(lrep, rrep, "sub");
		case '*':
			return builder.CreateFMul(lrep, rrep, "mul");
		case '/':
			return builder.CreateFDiv(lrep, rrep, "div");
		case '%':
			return builder.CreateFRem(lrep, rrep, "mod");
		case '^':
			return builder.CreateCall(pow_fn, {lrep, rrep}, "pow");
		default:
			throw InvalidInput{"Invalid binary operator : "s + op_};
	}
}

llvm::Value* AssignmentTree::codegen(llvm::Module& main, llvm::IRBuilder<>& builder)
{
	auto id = static_cast<IdentifierTree*>(lhs_.get());
	auto rrep = rhs_->codegen(main, builder);
	llvm::GlobalVariable* var{nullptr};
	if (!(var = main.getGlobalVariable(id->label_)))
	{
		if (id->vars_.find(id->label_) == std::end(id->vars_))
		{
			auto fn_it = id->funs_.find(id->label_);
			if (fn_it != std::end(id->funs_))
			{
				std::cout << "Warning : overriding function " << id->label_ << '\n';
				id->funs_.erase(fn_it);
			}
			id->vars_[id->label_] = 0.0f;
		}
		var = new llvm::GlobalVariable{main, llvm::Type::getDoubleTy(llvm::getGlobalContext()), false,
		                               llvm::GlobalVariable::ExternalLinkage,
		                               llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat{0.0}),
		                               id->label_};
	}
	builder.CreateStore(rrep, var);
	return lhs_->codegen(main, builder);
}

llvm::Value* FunctionParamTree::codegen(llvm::Module&, llvm::IRBuilder<>&)
{
	auto par_idx = static_cast<std::size_t>(std::find(std::begin(function_->param_names),
	                                       std::end(function_->param_names),
	                                       label_)
	                                       - std::begin(function_->param_names));
	assert(par_idx < function_->param_values.size());
	return function_->param_values[par_idx];
}

llvm::Value* FunctionCallTree::codegen(llvm::Module& main, llvm::IRBuilder<>& builder)
{
	std::vector<llvm::Value*> fn_args;
	for (auto& elem : params_)
		fn_args.emplace_back(elem->codegen(main, builder));
	auto fn = funs_[label_];
	if (fn->type == FunctionType::intrinsic)
	{
		std::vector<llvm::Type*> args_type{fn->param_names.size(),
			                               llvm::Type::getDoubleTy(llvm::getGlobalContext())};
		auto intr = llvm::Intrinsic::getDeclaration(&main, fn->intrinsic, args_type);
		assert(intr);
		return builder.CreateCall(intr, fn_args, label_);
	}
	if (fn->type == FunctionType::builtin)
	{
		auto builtin = main.getFunction("calcfn_" + label_);
		if (!builtin)
		{
			std::vector<llvm::Type*> args_type{fn->param_names.size(),
			                                   llvm::Type::getDoubleTy(llvm::getGlobalContext())};
			auto fn_type = llvm::FunctionType::get(llvm::Type::getDoubleTy(llvm::getGlobalContext()),
			                                       args_type, false);
			builtin = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage,
			                                 "calcfn_" + label_, &main);
		}
		return builder.CreateCall(builtin, fn_args, "calcfn_" + label_);
	}
	fn->param_values = std::move(fn_args);
	return fn->body->codegen(main, builder);
}

void NumberTree::print(std::ostream& os)
{
	os << number_;
}

void IdentifierTree::print(std::ostream& os)
{
	os << label_;
}

void UnaryExprTree::print(std::ostream& os)
{
	os << op_;
	if (st_->type != TreeType::number && st_->type != TreeType::identifier)
		os << '(';
	st_->print(os);
	if (st_->type != TreeType::number && st_->type != TreeType::identifier)
		os << ')';
}

void BinaryExprTree::print(std::ostream& os)
{
	if (lhs_->type == TreeType::binary_op
	    && operator_precedence(static_cast<BinaryExprTree*>(lhs_.get())->op_) < operator_precedence(op_))
	{
		os << '(';
		lhs_->print(os);
		os << ')';
	}
	else
		lhs_->print(os);
	if (op_ == '^')
		os << '^';
	else
		os << ' ' << op_ << ' ';
	if (rhs_->type == TreeType::binary_op &&
	    operator_precedence(static_cast<BinaryExprTree*>(rhs_.get())->op_) <= operator_precedence(op_))
	{
		os << '(';
		rhs_->print(os);
		os << ')';
	}
	else
		rhs_->print(os);
}

void AssignmentTree::print(std::ostream& os)
{
	lhs_->print(os);
	os << " = ";
	rhs_->print(os);
}

void FunctionParamTree::print(std::ostream& os)
{
	os << label_;
}

void FunctionCallTree::print(std::ostream& os)
{
	os << label_;
	os << '(';
	for (auto it = std::begin(params_) ; it != std::end(params_) ; ++it)
	{
		(*it)->print(os);
		if (it != std::end(params_) - 1)
			os << ", ";
	}
	os << ')';
}
