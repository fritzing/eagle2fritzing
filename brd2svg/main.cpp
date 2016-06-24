#include "brdapplication.h"

// ADAFRUIT 2016-06-24: exitCode indicates whether EAGLE ULP script
// was run ('42' if true) -- this means new XML was generated and
// the calling process can invoke eagle2svg a second time to
// generate more 'finished' SVGs.
int exitCode = 0;

int main(int argc, char *argv[])
{
	BrdApplication a(argc, &argv);

	a.start();

	//a.exec();
	return exitCode;
}
