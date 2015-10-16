// Copyright 2015 Benoît Vey

#ifndef CALC_COMMAND_HANDLER_HPP_
#define CALC_COMMAND_HANDLER_HPP_

#include <map>
#include <string>
#include <vector>

class Lexer;
class Parser;
struct Function;

enum class CommandType
{
	help,
	quit,
	env,
	import,
	del,
	def
};

enum class EqMinMax
{
	equal,
	min,
	max
};

struct CommandCarac
{
	CommandType type;
	EqMinMax delta;
	std::size_t args_count;
	char const* doc;
};

struct Command
{
	std::vector<std::string> args;
	CommandType type;
};

Command parse_command(Lexer&);

void execute_help(std::string const*);

void execute_env(std::vector<std::string> const&, std::map<std::string, double> const&,
                 std::map<std::string, Function*> const&);

void execute_import(std::vector<std::string> const&, std::map<std::string, double>&,
                    std::map<std::string, Function*>&);

void execute_del(std::vector<std::string> const&, std::map<std::string, double>&,
                 std::map<std::string, Function*>&);

void execute_def(std::vector<std::string>&, std::map<std::string, double>&,
                 std::map<std::string, Function*>&, Parser&, Lexer&);

#endif // Header guard
