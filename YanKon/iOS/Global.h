//
//  Global.h
//  YanKon
//
//  Created by Evan JIANG on 15/1/21.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CommandDaemon.h"
#import "FMDB.h"

#define DEFAULT_PORT 5015
#define DB_VERSION 1

#define NOTIFY_ACTIVE_LIGHTS_CHANGED @"active_lights_changed"


@interface Global : NSObject

@property (nonatomic, strong) NSMutableSet *activeLights;
@property (nonatomic, strong) NSString *dbPath;

+ (Global *)getInstance;
+ (FMDatabaseQueue *)getFMDBQueue;
@end
