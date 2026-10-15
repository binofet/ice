#import <Foundation/Foundation.h>
@interface ICEWatchRuntime : NSObject
- (void)setActive:(BOOL)active;
- (void)pointer:(NSInteger)phase x:(float)x y:(float)y;
- (void)setMotion:(BOOL)motion;
- (NSString*)calibrate;
- (void)record;
- (void)replay;
- (NSString*)status;
@end
