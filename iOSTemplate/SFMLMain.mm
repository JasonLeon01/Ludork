#import <Foundation/Foundation.h>
#import "PythonRunner.h"

int sfmlMain(int argc, char *argv[]) {
	@autoreleasepool {
		[PythonRunner runEntryScript];
	}
	return 0;
}
