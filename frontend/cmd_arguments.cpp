#include "cmd_arguments.h"

CommandLineArguments::CommandLineArguments(int argc, char* argv[]) {
	for (int c = 0; c < argc; ++c) {
		m_arguments.push_back(argv[c]);
	}
}

bool CommandLineArguments::has_argument(const std::string val) const {
	return find_argument_index(val) >= 0;
}

int CommandLineArguments::find_argument_index(const std::string val) const {
	int index = 0;
	while (index < m_arguments.size()) {
		if (m_arguments[index] == val) {
			return index;
		}
		index++;
	}
	return -1;
}

std::optional<std::string> CommandLineArguments::get_optional_string(const std::string val, const int offset)  const {
	int index = find_argument_index(val);
	if (index < 0) {
		return std::nullopt;
	}
	return get_optional_string(index + offset);
}

std::optional<std::string> CommandLineArguments::get_optional_string(const int index)  const {
	if (index < 0 || index >= m_arguments.size()) {
		return std::nullopt;
	}
	return m_arguments[index];
}

std::string CommandLineArguments::get_string(const std::string val, const std::string deft, const int offset) const
{
	auto v = get_optional_string(val, offset);
	if (!v) {
		return deft;
	}
	return *v;
}

std::string CommandLineArguments::get_string(const int index, const std::string deft) const
{
	auto v = get_optional_string(index);
	if (!v) {
		return deft;
	}
	return *v;
}

std::optional<int> CommandLineArguments::get_optional_integer(const std::string val, const int offset) const
{
	auto v = get_optional_string(val, offset);
	if (!v) {
		return std::nullopt;
	}
	return std::stoi(*v);
}

std::optional<int> CommandLineArguments::get_optional_integer(const int index) const
{
	auto v = get_optional_string(index);
	if (!v) {
		return std::nullopt;
	}
	return std::stoi(*v);
}

int CommandLineArguments::get_integer(const std::string val, const int deft, const int offset) const
{
	auto v = get_optional_string(val, offset);
	if (!v) {
		return deft;
	}
	return std::stoi(*v);
}

int CommandLineArguments::get_integer(const int index, const int deft) const
{
	auto v = get_optional_string(index);
	if (!v) {
		return deft;
	}
	return std::stoi(*v);
}

std::optional<double> CommandLineArguments::get_optional_double(const std::string val, const int offset) const
{
	auto v = get_optional_string(val, offset);
	if (!v) {
		return std::nullopt;
	}
	return std::stod(*v);
}

std::optional<double> CommandLineArguments::get_optional_double(const int index) const
{
	auto v = get_optional_string(index);
	if (!v) {
		return std::nullopt;
	}
	return std::stod(*v);
}

double CommandLineArguments::get_double(const std::string val, const double deft, const int offset) const
{
	auto v = get_optional_string(val, offset);
	if (!v) {
		return deft;
	}
	return std::stod(*v);
}

double CommandLineArguments::get_double(const int index, const double deft) const
{
	auto v = get_optional_string(index);
	if (!v) {
		return deft;
	}
	return std::stod(*v);
}

