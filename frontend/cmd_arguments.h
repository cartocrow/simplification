#pragma once

#include <vector>
#include <string>

class CommandLineArguments {
private:
	std::vector<std::string> m_arguments;
public:
	CommandLineArguments(int argc, char* argv[]);

	bool has_argument(const std::string val) const;

	int find_argument_index(const std::string val) const;

	std::string get_argument(const std::string val, const int count = 0) const;
	std::string get_argument(const int val_index, const int count = 0) const;
};
