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

std::string CommandLineArguments::get_argument(const std::string val, const int count)  const {
	return get_argument(find_argument_index(val), count);
}

std::string CommandLineArguments::get_argument(const int val_index, const int count)  const {
	return m_arguments[val_index + count + 1];
}
