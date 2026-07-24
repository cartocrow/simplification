#include "gui.h"
#include "commandline.h"
#include "cmd_arguments.h"

int main(int argc, char* argv[]) {
	CommandLineArguments cla(argc, argv);
	if (argc <= 1 || cla.has_argument("-gui")) {
		launchGUI(argc, argv);
	}
	else {
		runCommand(cla);
	}
}