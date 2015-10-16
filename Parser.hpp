// Copyright 2015 Benoît Vey

#ifndef CALC_PARSER_HPP_
#define CALC_PARSER_HPP_

#include <map>
#include <memory>
#include <string>

class Lexer;
class ExprTree;
using ExprNode = std::unique_ptr<ExprTree>;
struct Function;

enum class Associativity
{
	left,
	right,
	unknown
};

struct OpCarac
{
	int precedence;
	Associativity associativity;
};

int operator_precedence(char);
Associativity operator_associativity(char op);

class Parser
{
	public:
	Parser(std::map<std::string, double>&, std::map<std::string, Function*>&);

	Parser(Parser const&) = delete;
	Parser& operator=(Parser const&) = delete;

	Parser(Parser&&) = default;
	Parser& operator=(Parser&&) = default;

	~Parser() = default;

	ExprNode parse(Lexer&);
	ExprNode parse_function_body(Lexer&, Function&);

	private:
	ExprNode parse_expr_(Function*);
	ExprNode parse_top_(Function*);
	ExprNode parse_number_();
	ExprNode parse_identifier_(Function*);
	ExprNode parse_unary_(Function*);
	ExprNode parse_paren_(Function*);
	ExprNode parse_binary_rhs_(int, ExprNode, Function*);

	std::map<std::string, double>& vars_;
	std::map<std::string, Function*>& funs_;
	Lexer* lex_;
	char cur_tok_;
};

#endif // Header guard
