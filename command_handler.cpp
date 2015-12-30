// Copyright 2015 Benoît Vey

#include "command_handler.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>

#include "Lexer.hpp"
#include "Parser.hpp"
#include "syntax_tree.hpp"
#include "utility.hpp"

using namespace std::string_literals;

namespace
{

std::map<std::string, double> builtin_vars
	{{"pi", std::acos(-1)},
	 {"e", std::exp(1)},
	 {"sqrt2", std::sqrt(2)},
	 {"phi", (1.0 + std::sqrt(5)) / 2.0}};

std::map<std::string, std::size_t> builtin_funs
	{{"sqrt", 0},
	 {"ceil", 1},
	 {"floor", 2},
	 {"trunc", 3},
	 {"exp", 4},
	 {"log", 5},
	 {"sin", 6},
	 {"cos", 7},
	 {"abs", 8},
	 {"min", 9},
	 {"max", 10},
	 {"round", 11},
	 {"tan", 12},
	 {"asin", 13},
	 {"acos", 14},
	 {"atan", 15},
	 {"gamma", 16},
	 {"rand", 17}};

std::array<Function, 18> bf_impl
	{{{nullptr, {"x"}, {}, llvm::Intrinsic::sqrt, FunctionType::intrinsic},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::ceil, FunctionType::intrinsic},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::floor, FunctionType::intrinsic},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::trunc, FunctionType::intrinsic},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::exp, FunctionType::intrinsic},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::log, FunctionType::intrinsic},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::sin, FunctionType::intrinsic},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::cos, FunctionType::intrinsic},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::fabs, FunctionType::intrinsic},
	  {nullptr, {"x", "y"}, {}, llvm::Intrinsic::minnum, FunctionType::intrinsic},
	  {nullptr, {"x", "y"}, {}, llvm::Intrinsic::maxnum, FunctionType::intrinsic},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::round, FunctionType::intrinsic},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::not_intrinsic, FunctionType::builtin},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::not_intrinsic, FunctionType::builtin},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::not_intrinsic, FunctionType::builtin},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::not_intrinsic, FunctionType::builtin},
	  {nullptr, {"x"}, {}, llvm::Intrinsic::not_intrinsic, FunctionType::builtin},
	  {nullptr, {"min", "max"}, {}, llvm::Intrinsic::not_intrinsic, FunctionType::builtin}}};

} // namespace

extern "C" double calcfn_tan(double x)
{
	return std::tan(x);
}

extern "C" double calcfn_asin(double x)
{
	return std::asin(x);
}

extern "C" double calcfn_acos(double x)
{
	return std::acos(x);
}

extern "C" double calcfn_atan(double x)
{
	return std::atan(x);
}

extern "C" double calcfn_gamma(double x)
{
	return std::tgamma(x);
}

extern "C" double calcfn_rand(double min, double max)
{
	static std::mt19937 engine{std::random_device{}()};
	if (max < min)
		return std::numeric_limits<double>::quiet_NaN();
	std::uniform_real_distribution<> dist{min, max};
	return dist(engine);
}

namespace
{

char const* help_doc()
{
	return
	"Help command :\n"
	"\tSyntax : !help [command]\n"
	"\tPrint help.\n";
}

char const* quit_doc()
{
	return
	"Quit command :\n"
	"\tSyntax : !quit\n"
	"\tExit the program.\n"
	"\tYou can use Ctrl^D too.\n";
}

char const* env_doc()
{
	return
	"Env command :\n"
	"\tSyntax : !env [identifiers...]\n"
	"\tPrint the value of variables or functions.\n"
	"\tIf no arguments are given, print the entire environment.\n";
}

char const* import_doc()
{
	return
	"Import command :\n"
	"\tSyntax : !import builtins...\n"
	"\tImport builtins in the environment.\n"
	"\tBuiltins list :\n"
	"\t\tNumbers :\n"
	"\t\t\tpi : Archimedes' constant.\n"
	"\t\t\te : Euler's number.\n"
	"\t\t\tphi : Golden ratio.\n"
	"\t\t\tsqrt2 : Square root of 2.\n"
	"\t\tFunctions :\n"
	"\t\t\tabs(x) : Absolute value of x.\n"
	"\t\t\tsqrt(x) : Square root of x.\n"
	"\t\t\tround(x) : Nearest integer to x.\n"
	"\t\t\tceil(x) : The smallest integer not less than x.\n"
	"\t\t\tfloor(x) : The largest integer not greater than x.\n"
	"\t\t\ttrunc(x) : x rounded towards zero.\n"
	"\t\t\texp(x) : e raised to the given power x.\n"
	"\t\t\tlog(x) : Natural logarithm of x.\n"
	"\t\t\tsin(x) : Sine of x.\n"
	"\t\t\tcos(x) : Cosine of x.\n"
	"\t\t\ttan(x) : Tangent of x.\n"
	"\t\t\tasin(x) : Arc sine of x.\n"
	"\t\t\tacos(x) : Arc cosine of x.\n"
	"\t\t\tatan(x) : Arc tangent of x.\n"
	"\t\t\tmin(x, y) : Smaller of x and y.\n"
	"\t\t\tmax(x, y) : Greater of x and y.\n"
	"\t\t\tgamma(x) : Gamma function of x.\n"
	"\t\t\trand(min, max) : Random number between min and max.\n";
}

char const* del_doc()
{
	return
	"Del command :\n"
	"\tSyntax : !del identifiers...\n"
	"\tDelete elements from the environment.\n";
}

char const* def_doc()
{
	return
	"Def command :\n"
	"\tSyntax : !def name([params...]) = body\n"
	"\tDefine new functions. Body can be any valid expression.\n"
	"\tNote : Recursive function calls are not allowed.\n";
}

std::map<std::string, CommandCarac> commands
	{{"help", {CommandType::help, EqMinMax::max, 1, help_doc()}},
	 {"quit", {CommandType::quit, EqMinMax::equal, 0, quit_doc()}},
	 {"env", {CommandType::env, EqMinMax::min, 0, env_doc()}},
	 {"import", {CommandType::import, EqMinMax::min, 1, import_doc()}},
	 {"del", {CommandType::del, EqMinMax::min, 1, del_doc()}},
	 {"def", {CommandType::def, EqMinMax::min, 0, def_doc()}}};

Command parse_function_def(Command& fn, Lexer& lex)
{
	auto cur_tok = lex.next();
	if (cur_tok == Token::eof)
		throw InvalidInput{"Expected function definition"};
	if (cur_tok != Token::identifier)
		throw InvalidInput{"Invalid function name"};
	fn.args.emplace_back(lex.identifier());
	cur_tok = lex.next();
	if (cur_tok != '(')
		throw InvalidInput{"Expected '('"};
	cur_tok = lex.next();
	while (cur_tok != ')')
	{
		if (cur_tok != Token::identifier)
			throw InvalidInput{"Syntax error in function parameters"};
		if (is_in(lex.identifier(), fn.args))
			throw InvalidInput{"Multiple parameters named " + lex.identifier()};
		fn.args.emplace_back(lex.identifier());
		cur_tok = lex.next();
		if (cur_tok != ',' && cur_tok != ')')
			throw InvalidInput{"Syntax error in function parameters"};
		if (cur_tok == ',')
		{
			cur_tok = lex.next();
			if (cur_tok != Token::identifier)
				throw InvalidInput{"Syntax error in function parameters"};
		}
	}
	cur_tok = lex.next();
	if (cur_tok != '=')
		throw InvalidInput{"Expected '='"};

	return fn;
}

} // namespace

Command parse_command(Lexer& lex)
{
	lex.next();
	auto cur_tok = lex.next();
	if (cur_tok == Token::eof)
		return Command{{}, CommandType::quit};

	if (cur_tok != Token::identifier)
		throw InvalidInput{"No such command"};
	auto com_name = lex.identifier();
	if (commands.find(com_name) == std::end(commands))
		throw InvalidInput{"No such command : " + com_name};
	Command c;
	c.type = commands[com_name].type;
	if (c.type == CommandType::def)
		return parse_function_def(c, lex);
	cur_tok = lex.next();
	while (cur_tok != Token::eof)
	{
		if (cur_tok != Token::identifier)
			throw InvalidInput{"Wrong argument format"};
		c.args.emplace_back(lex.identifier());
		cur_tok = lex.next();
	}
	auto err_str = [&com_name]{return "Wrong argument count. Command " + com_name + " takes ";};
	auto count_str = [&com_name]{return std::to_string(commands[com_name].args_count);};
	auto args_str = [&com_name]
	{
		auto s = " argument"s;
		if (commands[com_name].args_count != 1)
			s += 's';
		return s;
	};
	switch (commands[com_name].delta)
	{
		case EqMinMax::equal:
			if (c.args.size() != commands[com_name].args_count)
				throw InvalidInput{err_str() + "exactly " + count_str() + args_str()};
			break;
		case EqMinMax::min:
			if (c.args.size() < commands[com_name].args_count)
				throw InvalidInput{err_str() + "at least " + count_str() + args_str()};
			break;
		case EqMinMax::max:
			if (c.args.size() > commands[com_name].args_count)
				throw InvalidInput{err_str() + "at most " + count_str() + args_str()};
			break;
	}
	return c;
}

void execute_env(std::vector<std::string> const& args, std::map<std::string, double> const& var_env,
                 std::map<std::string, Function*> const& fun_env)
{
	if (args.empty())
	{
		if (!var_env.empty())
			std::cout << "Variables :\n";
		for (auto& elem : var_env)
			std::cout << elem.first << " = " << elem.second << '\n';
		if (!fun_env.empty())
			std::cout << "Functions :\n";
		for (auto& elem : fun_env)
		{
			std::cout << elem.first << '(';
			for (auto it = std::begin(elem.second->param_names) ; it != std::end(elem.second->param_names) ; ++it)
			{
				std::cout << *it;
				if (it != std::end(elem.second->param_names) - 1)
					std::cout << ", ";
			}
			std::cout << ')';
			if (elem.second->type != FunctionType::userdef)
				std::cout << " (builtin)";
			else
			{
				std::cout << " = ";
				elem.second->body->print(std::cout);
			}
			std::cout << '\n';
		}
		return;
	}
	std::ostringstream to_print;
	for (auto& elem : args)
	{
		if (std::count(std::begin(args), std::end(args), elem) > 1)
			throw InvalidInput{"Multiple uses of " + elem};
		auto var_it = var_env.find(elem);
		if (var_it != std::end(var_env))
		{
			to_print << var_it->first << " = " << var_it->second << '\n';
			continue;
		}
		auto fun_it = fun_env.find(elem);
		if (fun_it == std::end(fun_env))
			throw InvalidInput{"Undeclared identifier : " + elem};
		to_print << fun_it->first << '(';
		for (auto par_it = std::begin(fun_it->second->param_names) ;
		     par_it != std::end(fun_it->second->param_names) ; ++par_it)
		{
			to_print << *par_it;
			if (par_it != std::end(fun_it->second->param_names) - 1)
				to_print << ", ";
		}
		to_print << ')';
		if (fun_it->second->type != FunctionType::userdef)
			to_print << " (builtin)";
		else
		{
			std::cout << " = ";
			fun_it->second->body->print(to_print);
		}
		to_print << '\n';
	}
	std::cout << to_print.str();
}

void execute_import(std::vector<std::string> const& args, std::map<std::string, double>& var_env,
                    std::map<std::string, Function*>& fun_env)
{
	std::map<std::string, double> values;
	std::map<std::string, Function*> funs;
	for (auto& elem : args)
	{
		if (std::count(std::begin(args), std::end(args), elem) > 1)
			throw InvalidInput{"Multiple uses of " + elem};
		auto var_it = builtin_vars.find(elem);
		if (var_it != std::end(builtin_vars))
		{
			values[elem] = var_it->second;
			continue;
		}
		auto fun_it = builtin_funs.find(elem);
		if (fun_it == std::end(builtin_funs))
			throw InvalidInput{elem + " is not in builtin list"};
		funs[elem] = &bf_impl[fun_it->second];
	}
	for (auto& elem : values)
	{
		auto fun_it = fun_env.find(elem.first);
		if (fun_it != std::end(fun_env))
		{
			std::cout << "Warning : overriding function " << elem.first << '\n';
		    fun_env.erase(fun_it);
		}
		std::cout << elem.first << " = " << elem.second << '\n';
		var_env[elem.first] = elem.second;
	}
	for (auto& elem : funs)
	{
		auto var_it = var_env.find(elem.first);
		if (var_it != std::end(var_env))
		{
			std::cout << "Warning : overriding variable " << elem.first << '\n';
		    var_env.erase(var_it);
		}
		if (fun_env.find(elem.first) != std::end(fun_env))
			std::cout << "Warning : redefining function " << elem.first << '\n';
		std::cout << "Function " + elem.first << '(';
		for (auto par_it = std::begin(elem.second->param_names) ;
		     par_it != std::end(elem.second->param_names) ; ++par_it)
		{
			std::cout << *par_it;
			if (par_it != std::end(elem.second->param_names) - 1)
				std::cout << ", ";
		}
		std::cout << ")\n";
		fun_env[elem.first] = elem.second;
	}
}

void execute_del(std::vector<std::string> const& args, std::map<std::string, double>& var_env,
                 std::map<std::string, Function*>& fun_env)
{
	std::vector<std::remove_reference<decltype(var_env)>::type::iterator> vars_to_del;
	std::vector<std::remove_reference<decltype(fun_env)>::type::iterator> funs_to_del;
	
	for (auto& elem : args)
	{
		if (std::count(std::begin(args), std::end(args), elem) > 1)
			throw InvalidInput{"Multiple uses of " + elem};
		auto var_it = var_env.find(elem);
		if (var_it != std::end(var_env))
		{
			vars_to_del.emplace_back(var_it);
			continue;
		}
		auto fun_it = fun_env.find(elem);
		if (fun_it == std::end(fun_env))
			throw InvalidInput{elem + " is not in current environment"};
		funs_to_del.emplace_back(fun_it);
	}
	for (auto elem : vars_to_del)
		var_env.erase(elem);
	for (auto elem : funs_to_del)
		fun_env.erase(elem);
}

void execute_def(std::vector<std::string>& args, std::map<std::string, double>& var_env,
                 std::map<std::string, Function*>& fun_env, Parser& par, Lexer& lex)
{
	static std::map<Function*, std::unique_ptr<Function>> functions;

	auto fn_name = args[0];
	args.erase(std::begin(args));
	std::unique_ptr<Function> function{new Function{nullptr, std::move(args), {},
	                                   llvm::Intrinsic::not_intrinsic, FunctionType::userdef}};
	function->body = par.parse_function_body(lex, *function);

	auto var_it = var_env.find(fn_name);
	if (var_it != std::end(var_env))
	{
		std::cout << "Warning : overriding variable " << fn_name << '\n';
		var_env.erase(var_it);
	}

	auto fun_it = fun_env.find(fn_name);
	if (fun_it != std::end(fun_env))
	{
		std::cout << "Warning : redefining function " << fn_name << '\n';
		auto fn = fun_it->second;
		auto def_fun_it = functions.find(fn);
		fun_env.erase(fun_it);
		if (def_fun_it != std::end(functions))
			functions.erase(def_fun_it);
	}

	auto fn_ptr = function.get();
	functions.insert(std::make_pair(fn_ptr, std::move(function)));
	fun_env.insert(std::make_pair(fn_name, fn_ptr));
}

void execute_help(std::string const* arg)
{
	if (!arg)
	{
		std::cout <<
			"Expression syntax :\n"
			"\tNumber format :\n"
			"\t\t42\n"
			"\t\t42.0\n"
			"\t\t4.2e+1\n"
			"\tOperators :\n"
			"\t\tx + y : addition - left-associative\n"
			"\t\tx - y : substraction - left-associative\n"
			"\t\tx * y : multiplication - left-associative\n"
			"\t\tx / y : division - left-associative\n"
			"\t\tx % y : modulation - left-associative\n"
			"\t\tx ^ y : exponentiation - right-associative\n"
			"\t\tx = y : assignment - right-associative\n"
			"\t\t  -x  : negation\n"
			"Commands :\n"
			"\tSyntax : !command [args]\n\n"
			"\thelp :\n"
			"\t\tPrint help.\n"
			"\t\tCommand-specific help : !help command\n"
			"\tquit :\n"
			"\t\tExit the program.\n"
			"\tenv :\n"
			"\t\tShow current environment.\n"
			"\timport :\n"
			"\t\tImport common mathematical stuff in the environment.\n"
			"\tdel :\n"
			"\t\tDelete elements from the environment.\n"
			"\tdef :\n"
			"\t\tDefine new functions.\n";
		return;
	}

	auto it = commands.find(*arg);
	if (it == std::end(commands))
		throw InvalidInput{"No such command : " + *arg};
	std::cout << it->second.doc;
}
