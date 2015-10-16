// Copyright 2015 Benoît Vey

#ifndef CALC_UTILITY_HPP_
#define CALC_UTILITY_HPP_

#include <algorithm>
#include <initializer_list>
#include <string>

class InvalidInput : public std::exception
{
	public:
	InvalidInput(std::string err) : err_{std::move(err)}
	{}

	char const* what() const noexcept override
	{
		return err_.c_str();
	}

	private:
	std::string err_;
};

template <typename T, typename C>
bool is_in(T const& v, C const& vs)
{
	return std::find(std::begin(vs), std::end(vs), v) != std::end(vs);
}

template <typename T>
bool is_in(T const& v, std::initializer_list<T> const& vs)
{
	return std::find(std::begin(vs), std::end(vs), v) != std::end(vs);
}

#endif // Header guard
