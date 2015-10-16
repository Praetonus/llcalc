// Copyright 2015 Benoît Vey

#ifndef CALC_SYNTAX_TREE_HPP_
#define CALC_SYNTAX_TREE_HPP_

#include <limits>
#include <vector>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>

#include "utility.hpp"

class ExprTree;
using ExprNode = std::unique_ptr<ExprTree>;

enum class FunctionType
{
	intrinsic,
	builtin,
	userdef
};

struct Function
{
	ExprNode body;
	std::vector<std::string> param_names;
	std::vector<llvm::Value*> param_values;
	llvm::Intrinsic::ID intrinsic;
	FunctionType type;
};
	
enum class TreeType
{
	number,
	identifier,
	unary_op,
	binary_op,
	assignment,
	function_param,
	function_call
};

class ExprTree
{
	public:
	ExprTree(TreeType t) : type{t}
	{}

	virtual ~ExprTree() = default;

	virtual llvm::Value* codegen(llvm::Module&, llvm::IRBuilder<>&) = 0;

	virtual void print(std::ostream&) = 0;

	TreeType const type;
};

class NumberTree : public ExprTree
{
	public:
	NumberTree(double number) : ExprTree{TreeType::number}, number_{number}
	{}

	llvm::Value* codegen(llvm::Module&, llvm::IRBuilder<>&) override;

	void print(std::ostream&) override;

	private:
	double number_;
};

class IdentifierTree : public ExprTree
{
	friend class AssignmentTree;
	public:
	IdentifierTree(std::string label, std::map<std::string, double>& vars,
	               std::map<std::string, Function*>& funs)
		: ExprTree{TreeType::identifier}, label_{std::move(label)}, vars_{vars}, funs_{funs}
	{}

	llvm::Value* codegen(llvm::Module&, llvm::IRBuilder<>&) override;

	void print(std::ostream&) override;

	private:
	std::string label_;
	std::map<std::string, double>& vars_;
	std::map<std::string, Function*>& funs_;
};

class UnaryExprTree : public ExprTree
{
	public:
	UnaryExprTree(char op, ExprNode st)
		: ExprTree{TreeType::unary_op}, op_{op}, st_{std::move(st)}
	{}

	llvm::Value* codegen(llvm::Module&, llvm::IRBuilder<>&) override;

	void print(std::ostream&) override;

	private:
	char op_;
	ExprNode st_;
};

class BinaryExprTree : public ExprTree
{
	public:

	BinaryExprTree(char op, ExprNode lhs, ExprNode rhs)
		: ExprTree{TreeType::binary_op}, lhs_{std::move(lhs)}, rhs_{std::move(rhs)}, op_{op}
	{}

	llvm::Value* codegen(llvm::Module&, llvm::IRBuilder<>&) override;

	void print(std::ostream&) override;

	private:
	ExprNode lhs_;
	ExprNode rhs_;
	char op_;
};

class AssignmentTree : public ExprTree
{
	public:
	AssignmentTree(ExprNode lhs, ExprNode rhs)
		: ExprTree{TreeType::assignment}, lhs_{std::move(lhs)}, rhs_{std::move(rhs)}
	{
		if (lhs_->type != TreeType::identifier)
			throw InvalidInput{"Expression is not assignable"};
	}

	llvm::Value* codegen(llvm::Module&, llvm::IRBuilder<>&) override;

	void print(std::ostream&) override;

	private:
	ExprNode lhs_;
	ExprNode rhs_;
};

class FunctionParamTree : public ExprTree
{
	public:
	FunctionParamTree(std::string label, Function* function)
		: ExprTree{TreeType::function_param}, label_{std::move(label)}, function_{function}
	{
		assert(function_);
	}

	llvm::Value* codegen(llvm::Module&, llvm::IRBuilder<>&) override;

	void print(std::ostream&) override;

	private:
	std::string label_;
	Function* function_;
};

class FunctionCallTree : public ExprTree
{
	public:
	FunctionCallTree(std::string label, std::vector<ExprNode>&& params,
	                 std::map<std::string, Function*>& funs, std::map<std::string, double> const& vars)
		: ExprTree{TreeType::function_call}, label_{label}, params_{std::move(params)}, funs_{funs}
	{
		using namespace std::string_literals;

		auto it = funs_.find(label_);
		if(it == std::end(funs_))
		{
			auto err = ""s;
			if (vars.find(label_) != std::end(vars))
				err = ". Maybe you meant to use the variable?";
			throw InvalidInput{"Undeclared function : " + label_ + err};
		}
		if (it->second->param_names.size() != params_.size())
		{
			auto err = "Too "s + (it->second->param_names.size() < params_.size() ? "many" : "few") +
			           " arguments in call to function " + label_ + ". Function takes " +
			           std::to_string(it->second->param_names.size()) + " argument";
			if (it->second->param_names.size() != 1)
				err += 's';
			throw InvalidInput{err};
		}
	}

	llvm::Value* codegen(llvm::Module&, llvm::IRBuilder<>&) override;

	void print(std::ostream&) override;

	private:
	std::string label_;
	std::vector<ExprNode> params_;
	std::map<std::string, Function*>& funs_;
};

#endif // Header guard
