#import <UIKit/UIKit.h>
#import "LudorkSceneDelegate.h"

@interface SFAppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation SFAppDelegate (LudorkScene)

- (UISceneConfiguration *)application:(UIApplication *)application
	  configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession
									 options:(UISceneConnectionOptions *)options {
	(void)application;
	(void)options;
	UISceneConfiguration *configuration =
		[[UISceneConfiguration alloc] initWithName:@"Default Configuration" sessionRole:connectingSceneSession.role];
	configuration.delegateClass = [LudorkSceneDelegate class];
	return configuration;
}

@end
