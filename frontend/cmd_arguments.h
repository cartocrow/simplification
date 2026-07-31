#pragma once

#include <vector>
#include <string>
#include <optional>

class CommandLineArguments {
private:
	std::vector<std::string> m_arguments;
public:
	CommandLineArguments(int argc, char* argv[]);

	// checks whether the given string is one of the arguments
	bool has_argument(const std::string val) const;

	// finds the index of the given argument; will be -1 if the index was not found
	int find_argument_index(const std::string val) const;

	std::optional<std::string> get_optional_string(const std::string val, const int offset = 1) const;
	std::optional<std::string> get_optional_string(const int index) const;
	std::string get_string(const std::string val, const std::string deft = "", const int offset = 1) const;
	std::string get_string(const int index, const std::string deft = "") const;

	std::optional<int> get_optional_integer(const std::string val, const int offset = 1) const;
	std::optional<int> get_optional_integer(const int index) const;
	int get_integer(const std::string val, const int deft = 0, const int offset = 1) const;
	int get_integer(const int index, const int deft = 0) const;

	std::optional<double> get_optional_double(const std::string val, const int offset = 1) const;
	std::optional<double> get_optional_double(const int index) const;
	double get_double(const std::string val, const double deft = 0, const int offset = 1) const;
	double get_double(const int index, const double deft = 0) const;
};
