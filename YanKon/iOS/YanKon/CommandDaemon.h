//
//  CommandDaemon.h
//  YanKon
//
//  Created by Evan JIANG on 15/1/21.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface CommandDaemon : NSObject


+ (CommandDaemon *)getInstance;
- (void)didEnterBackground;
- (void)willEnterForeground;
- (void)sendCMD:(NSData*)data toIPs:(NSArray*)ips;
- (void)sendSearchCMD;

@end
