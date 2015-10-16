// Copyright 2015 Benoît Vey

#include "Lexer.hpp"

#include <cassert>

#include "utility.hpp"

void Lexer::newline(std::string&& line)
{
	line_ = std::istringstream{std::move(line)};
	last_ = ' ';
}

char Lexer::next()
{
	last_token_ = peek();
	peeked_ = Token::invalid;
	return last_token_;
}

char Lexer::peek()
{
	if (peeked_ != Token::invalid)
		return peeked_;

	if (!is_valid())
		return peeked_ = Token::eof;

	while (std::isspace(last_))
		last_ = static_cast<char>(line_.get());

	if (last_ == decltype(line_)::traits_type::eof())
		return peeked_ = Token::eof;

	if (std::isdigit(last_) || last_ == '.')
	{
		std::string str_num{};
		bool decimal{last_ == '.'};
		bool exp{false};
		bool exp_sign{false};
		do
		{
			str_num += last_;
			last_ = static_cast<char>(line_.get());
			if (last_ == '.')
				decimal = decimal || exp ? throw InvalidInput{"Wrong number format"} : true;
			if (last_ == 'e')
				exp = exp ? throw InvalidInput{"Wrong number format"} : true;
			if (last_ == '+' || last_ == '-')
			{
				if (!exp || exp_sign)
					break;
				exp_sign = true;
			}
		} while (std::isdigit(last_) || is_in(last_, {'.', 'e', '+', '-'}));
		try
		{
			number_ = stod(str_num);
		}
		catch (std::logic_error const&)
		{
			throw InvalidInput{"Wrong number format"};
		}
		return peeked_ = Token::number;
	}
	if (std::isalpha(last_))
	{
		identifier_.clear();
		do
		{
			identifier_ += last_;
			last_ = static_cast<char>(line_.get());
		}
		while (std::isalnum(last_));
		return peeked_ = Token::identifier;
	}
	peeked_ = last_;
	last_ = static_cast<char>(line_.get());
	return peeked_;
}

double Lexer::number() const
{
	assert(last_token_ == Token::number);
	return number_;
}

std::string Lexer::identifier() const
{
	assert(last_token_  == Token::identifier);
	return identifier_;
}

bool Lexer::is_valid() const
{
	return line_.good();
}
