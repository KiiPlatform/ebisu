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
#define DEFAULT_SCAN_DELAY 20

#define NOTIFY_ACTIVE_LIGHTS_CHANGED @"active_lights_changed"


@interface Global : NSObject

@property (nonatomic, strong) NSString *dbPath;
@property (nonatomic, strong) NSMutableDictionary *lightsIpDict;
@property (nonatomic, strong) NSMutableDictionary *lightsMacDict;
@property (nonatomic) int scanDelay;

+ (Global *)getInstance;
+ (FMDatabaseQueue *)getFMDBQueue;
+ (long long)getCurrentTime;
@end
