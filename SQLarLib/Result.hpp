#pragma once

#include <string>
#include <iostream>

// Once C++17 is supported this can be changed to just <optional>
#include "optional.hpp"

struct Result
{
	bool Success;
	std::string Reason;

	Result(bool success) 
	: 
		Success(success),
		Reason("")
	{ }


	Result(bool success, std::string const & reason) 
	: 
		Success(success),
		Reason(reason)
	{ }
};

template <typename T>
struct ResultT
{
	bool Success;
	std::string Reason;
	std::optional<T> OpValue;

	//ResultT(bool success, T value) 
	//: 
	//	Success(success),
	//	Reason(""),
	//	OpValue(value)
	//{ }

	ResultT(bool success, T const & value) 
	: 
		Success(success),
		Reason(""),
		OpValue(value)
	{ }

	ResultT(bool success, T && value) 
	: 
		Success(success),
		Reason(""),
		OpValue(std::move(value))
	{ }

	ResultT(bool success, std::string const & reason) 
	: 
		Success(success),
		Reason(reason)
	{ 
		if (!success)
			std::cout << "Result failed: " << reason << std::endl;
	}

};