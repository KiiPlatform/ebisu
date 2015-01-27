//
//  Global.m
//  YanKon
//
//  Created by Evan JIANG on 15/1/21.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "Global.h"

@implementation Global

static Global* instance = nil;
+ (Global*)getInstance
{
    @synchronized(self)
    {
        if (instance == nil) {
            instance = [[Global alloc] init];
        }
    }
    return instance;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.lightsIpDict = [[NSMutableDictionary alloc] init];
        self.lightsMacDict = [[NSMutableDictionary alloc] init];
        self.scanDelay = DEFAULT_SCAN_DELAY;
        NSString* docdir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0];
        self.dbPath = [docdir stringByAppendingPathComponent:@"yankon.sqlite"];
        [self createOrUpdateDB];
    }
    return self;
}

+ (FMDatabaseQueue*)getFMDBQueue
{
    return [FMDatabaseQueue databaseQueueWithPath:[Global getInstance].dbPath];
    ;
}

- (void)createOrUpdateDB
{
    FMDatabase* db = [FMDatabase databaseWithPath:self.dbPath];
    
    if (![db open]) {
        return;
    }
    //    int dbVersion = [db userVersion];
    [self createDBSQL:db];
    
    [db executeStatements:@"UPDATE lights set IP='', connected=0;"];
    [db close];
}

- (void)createDBSQL:(FMDatabase*)db
{
    if (![db tableExists:@"lights"]) {
        [db executeStatements:@"CREATE TABLE IF NOT EXISTS lights ("
         "_id INTEGER PRIMARY KEY AUTOINCREMENT, "
         "MAC TEXT NOT NULL,"
         "ver INTEGER,"
         "ThingID TEXT,"
         "remote_pwd TEXT,"
         "admin_pwd TEXT,"
         "name TEXT,"
         "color INTEGER,"
         "model TEXT,"
         "brightness INTEGER,"
         "CT INTEGER,"
         "IP TEXT,"
         "is_mine BOOL,"
         "state BOOL DEFAULT 0,"
         "connected BOOL,"
         "synced BOOL,"
         "owned_time INTEGER,"
         "deleted INTEGER DEFAULT 0"
         ");"];
        [db executeStatements:@"CREATE UNIQUE INDEX IF NOT EXISTS lights_mac ON lights (MAC);"];
        [db executeStatements:@"CREATE TABLE IF NOT EXISTS models ("
         "_id INTEGER PRIMARY KEY AUTOINCREMENT, "
         "model TEXT,"
         "pic TEXT,"
         "des TEXT, "
         "UNIQUE(model)"
         ");"];
        [db executeStatements:@"CREATE TABLE IF NOT EXISTS colors ("
         "_id INTEGER PRIMARY KEY AUTOINCREMENT, "
         "objectID TEXT,"
         "ver INTEGER,"
         "name TEXT,"
         "value INTEGER,"
         "synced BOOL,"
         "created_time INTEGER,"
         "deleted INTEGER DEFAULT 0"
         ");"];
        
        [db executeStatements:@"CREATE TABLE IF NOT EXISTS light_groups ("
         "_id INTEGER PRIMARY KEY AUTOINCREMENT, "
         "objectID TEXT,"
         "ver INTEGER,"
         "name TEXT,"
         "state BOOL DEFAULT 0,"
         "color INTEGER,"
         "brightness INTEGER,"
         "CT INTEGER,"
         "synced BOOL,"
         "created_time INTEGER,"
         "deleted INTEGER DEFAULT 0"
         ");"];
        [db executeStatements:@"CREATE TABLE IF NOT EXISTS light_group_rel ("
         "_id INTEGER PRIMARY KEY AUTOINCREMENT, "
         "light_id INTEGER,"
         "group_id INTEGER,"
         "created_time INTEGER,"
         "UNIQUE(light_id, group_id)"
         ");"];
        
        [db executeStatements:@"CREATE VIEW IF NOT EXISTS group_light_view "
         " AS SELECT * FROM light_group_rel,lights "
         " WHERE light_group_rel.light_id=lights._id;"];
        
        [db executeStatements:@"CREATE VIEW IF NOT EXISTS light_groups_view AS SELECT "
         "light_groups.*,(select count(_id) FROM light_group_rel "
         " where light_group_rel.group_id=light_groups._id) as num,"
         "(select sum(state) FROM group_light_view "
         " where group_light_view.group_id=light_groups._id) as on_num "
         " FROM light_groups;"];
        
        [db executeStatements:@"CREATE TABLE IF NOT EXISTS scenes ("
         "_id INTEGER PRIMARY KEY AUTOINCREMENT, "
         "objectID TEXT,"
         "ver INTEGER,"
         "name TEXT,"
         "synced BOOL,"
         "created_time INTEGER,"
         "last_used_time INTEGER,"
         "deleted INTEGER DEFAULT 0"
         ");"];
        [db executeStatements:@"CREATE TABLE IF NOT EXISTS scenes_detail ("
         "_id INTEGER PRIMARY KEY AUTOINCREMENT, "
         "scene_id INTEGER,"
         "light_id INTEGER DEFAULT -1,"
         "group_id INTEGER DEFAULT -1,"
         "objectID TEXT,"
         "state BOOL DEFAULT 0,"
         "color INTEGER,"
         "brightness INTEGER,"
         "CT INTEGER,"
         "action_id INTEGER,"
         "created_time INTEGER"
         ");"];
        [db executeStatements:@"CREATE TABLE IF NOT EXISTS schedule ("
         "_id INTEGER PRIMARY KEY AUTOINCREMENT, "
         "enabled BOOL DEFAULT 0,"
         "name TEXT,"
         "scene_id TEXT DEFAULT NULL,"
         "light_id TEXT DEFAULT NULL,"
         "group_id TEXT DEFAULT NULL,"
         "objectID TEXT,"
         "ver INTEGER,"
         "color INTEGER,"
         "brightness INTEGER,"
         "CT INTEGER,"
         "state BOOL DEFAULT 0,"
         "time INTEGER,"
         "repeat TEXT,"
         "synced BOOL,"
         "created_time INTEGER,"
         "deleted INTEGER DEFAULT 0"
         ");"];
        [db executeStatements:@"CREATE TABLE IF NOT EXISTS actions ("
         "_id INTEGER PRIMARY KEY AUTOINCREMENT, "
         "objectID TEXT,"
         "ver INTEGER,"
         "name TEXT,"
         "content TEXT,"
         "created_time INTEGER"
         ");"];
        [db setUserVersion:DB_VERSION];
    }
}

+ (long long)getCurrentTime
{
    return (long long)([[NSDate date] timeIntervalSince1970] * 1000);
}

+ (BOOL)isLoggedIn
{
    
    NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
    NSLog(@"isLoggedIn? token is %@", [userDefaults valueForKey:@"token"]);
    return [userDefaults valueForKey:@"token"];
}

+(void)controlLight:(int)light_id doItNow:(BOOL)doItNow
{
    Light *light = [[Light alloc] init];
    FMDatabaseQueue *queue = [Global getFMDBQueue];
    [queue inDatabase:^(FMDatabase *db) {
        FMResultSet *rs;
        rs = [db executeQuery:@"select * from lights where _id=(?);",@(light_id)];
        if ([rs next]) {
            light.brightness = [rs intForColumn:@"brightness"];
            light.color = [rs intForColumn:@"color"];
            light.CT = [rs intForColumn:@"CT"];
            light.state = [rs boolForColumn:@"state"];
            light.ip = [rs stringForColumn:@"IP"];
            light.mac = [rs stringForColumn:@"MAC"];
            light.connected = [rs boolForColumn:@"connected"];
            light.remotePassword = [rs stringForColumn:@"remote_pwd"];
        } else {
            
        }
        [rs close];
    }];
    [queue close];
    if ([light.mac length] == 0) {
        [[iToast makeText:@"Cannot get light info"] show];
        return;
    }
    if (light.connected) {
        NSData *cmd = [Commands buildLightInfo:1 state:light.state color:light.color brightness:light.brightness CT:light.CT];
        [[CommandDaemon getInstance] sendCMD:cmd toIPs:@[light.ip]];
    } else {
        
    }
}

@end
