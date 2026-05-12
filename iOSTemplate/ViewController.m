#import "ViewController.h"
#import "PythonRunner.h"

@implementation ViewController

- (void)viewDidLoad {
	[super viewDidLoad];
	self.view.backgroundColor = [UIColor blackColor];
}

- (void)viewDidAppear:(BOOL)animated {
	[super viewDidAppear:animated];

	dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
		[PythonRunner runEntryScript];
	});
}

@end
