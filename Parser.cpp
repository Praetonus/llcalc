// Copyright 2015 Benoît Vey

#include "Parser.hpp"

#include <iostream>

#include "Lexer.hpp"
#include "syntax_tree.hpp"

static std::vector<char> unary_operators{'-'};

static std::map<char, OpCarac> binary_operators
	{{'=', {5, Associativity::right}},
	 {'+', {10, Associativity::left}},
	 {'-', {10, Associativity::left}},
	 {'*', {20, Associativity::left}},
	 {'/', {20, Associativity::left}},
	 {'%', {20, Associativity::left}},
	 {'^', {30, Associativity::right}}};

int operator_precedence(char op)
{
	if (binary_operators.find(op) == std::end(binary_operators))
		return -1;
	return binary_operators[op].precedence;
}

Associativity operator_associativity(char op)
{
	if (binary_operators.find(op) == std::end(binary_operators))
		return Associativity::unknown;
	return binary_operators[op].associativity;
}

Parser::Parser(std::map<std::string, double>& vars, std::map<std::string, Function*>& funs)
	: vars_{vars}, funs_{funs}, lex_{nullptr}, cur_tok_{Token::eof}
{}

ExprNode Parser::parse(Lexer& lex)
{
	lex_ = &lex;
	cur_tok_ = lex_->next();
	auto ast = parse_expr_(nullptr);
	if (cur_tok_ != Token::eof)
		throw InvalidInput{"Ill-formed expression"};
	return ast;
}

ExprNode Parser::parse_function_body(Lexer& lex, Function& fn)
{
	lex_ = &lex;
	cur_tok_ = lex_->next();
	auto body = parse_expr_(&fn);
	if (cur_tok_ != Token::eof)
		throw InvalidInput{"Ill-formed expression"};
	return body;
}

ExprNode Parser::parse_expr_(Function* fn)
{
	return parse_binary_rhs_(0, parse_unary_(fn), fn);
}

ExprNode Parser::parse_top_(Function* fn)
{
	switch (cur_tok_)
	{
		case Token::number:
			return parse_number_();
		case Token::identifier:
			return parse_identifier_(fn);
		case '(':
			return parse_paren_(fn);
		default:
			throw InvalidInput{"Ill-formed expression"};
	}
}

ExprNode Parser::parse_number_()
{
	auto res = std::make_unique<NumberTree>(lex_->number());
	cur_tok_ = lex_->next();
	return std::move(res);
}

ExprNode Parser::parse_identifier_(Function* fn)
{
	auto id = lex_->identifier();
	cur_tok_ = lex_->next();
	if (cur_tok_ == '(')
	{
		cur_tok_ = lex_->next();
		std::vector<ExprNode> fn_params;
		while (cur_tok_ != ')')
		{
			auto param = parse_expr_(fn);
			fn_params.emplace_back(std::move(param));
			if (cur_tok_ != ',' && cur_tok_ != ')')
				throw InvalidInput{"Ill-formed expression"};
			if (cur_tok_ == ',')
			{
				cur_tok_ = lex_->next();
				if (cur_tok_ == ')')
					throw InvalidInput{"Ill-formed expression"};
			}
		}
		cur_tok_ = lex_->next();
		auto fun_it = funs_.find(id);
		if (fn && fun_it != std::end(funs_) && fun_it->first == id)
			throw InvalidInput{"Recursive function calls are not allowed"};
		return std::make_unique<FunctionCallTree>(std::move(id), std::move(fn_params), funs_, vars_);
	}
	if (fn)
	{
		if (is_in(id, fn->param_names))
			return std::make_unique<FunctionParamTree>(std::move(id), fn);
	}
	return std::make_unique<IdentifierTree>(std::move(id), vars_, funs_);
}

ExprNode Parser::parse_unary_(Function* fn)
{
	if (!is_in(cur_tok_, unary_operators))
		return parse_top_(fn);

	auto un_op = cur_tok_;
	cur_tok_ = lex_->next();
	return std::make_unique<UnaryExprTree>(un_op, parse_unary_(fn));
}

ExprNode Parser::parse_paren_(Function* fn)
{
	cur_tok_ = lex_->next();
	auto ex = parse_expr_(fn);
	if (cur_tok_ != ')')
		throw InvalidInput{"Ill-formed expression : expected ')'"};
	cur_tok_ = lex_->next();
	return ex;
}

ExprNode Parser::parse_binary_rhs_(int expr_prec, ExprNode lhs, Function* fn)
{
	while (1)
	{
		auto cur_prec = operator_precedence(cur_tok_);
		if (cur_prec < expr_prec)
			return lhs;
		auto bin_op = cur_tok_;
		cur_tok_ = lex_->next();

		auto rhs = parse_unary_(fn);
		auto next_op = cur_tok_;
		auto next_prec = operator_precedence(next_op);
		while (cur_prec < next_prec ||
			   (operator_associativity(next_op) == Associativity::right && cur_prec == next_prec))
		{
			rhs = parse_binary_rhs_(next_prec, std::move(rhs), fn);
			next_op = cur_tok_;
			next_prec = operator_precedence(next_op);
		}

		if (bin_op == '=')
			lhs = std::make_unique<AssignmentTree>(std::move(lhs), std::move(rhs));
		else
			lhs = std::make_unique<BinaryExprTree>(bin_op, std::move(lhs), std::move(rhs));
	}
}
