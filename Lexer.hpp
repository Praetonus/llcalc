// Copyright 2015 Benoît Vey

#ifndef CALC_LEXER_HPP_
#define CALC_LEXER_HPP_

#include <sstream>

namespace Token
{
	enum Type
	{
		eof = -1,
		number = -2,
		identifier = -3,
		
		invalid = -4
	};
}

class Lexer
{
	public:
	Lexer() : line_{}, number_{0.0}, last_{'\0'}, last_token_{Token::eof}, peeked_{Token::invalid}
	{}

	Lexer(Lexer const&) = delete;
	Lexer& operator=(Lexer const&) = delete;
	
	Lexer(Lexer&&) = default;
	Lexer& operator=(Lexer&&) = default;

	~Lexer() = default;

	void newline(std::string&& line);

	char next();
	char peek();

	double number() const;
	std::string identifier() const;
	bool is_valid() const;

	private:
	std::istringstream line_;
	double number_;
	std::string identifier_;
	char last_;
	char last_token_;
	char peeked_;
};

#endif // Header guard
