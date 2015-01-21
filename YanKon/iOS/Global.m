//
//  Global.m
//  YanKon
//
//  Created by Evan JIANG on 15/1/21.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "Global.h"

@implementation Global


static Global *instance = nil;
+ (Global *)getInstance
{
    @synchronized(self) {
        if (instance == nil) {
            instance = [[Global alloc] init];
        }
    }
    return instance;
}

-(instancetype)init
{
    self = [super init];
    if (self) {
        self.activeLights = [[NSMutableSet alloc] init];
        NSString *docdir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0];
        self.dbPath = [docdir stringByAppendingPathComponent:@"yankon.sqlite"];
        [self createOrUpdateDB];
    }
    return self;
}


+ (FMDatabaseQueue *)getFMDBQueue
{
    return [FMDatabaseQueue databaseQueueWithPath:[Global getInstance].dbPath];;
}

- (void)createOrUpdateDB
{
    FMDatabase *db = [FMDatabase databaseWithPath:self.dbPath];
    
    if (![db open]) {
        return;
    }
    //    int dbVersion = [db userVersion];
    [self createDBSQL:db];
    [db setUserVersion:DB_VERSION];
    
    [db close];
}

- (void)createDBSQL:(FMDatabase*)db
{
    [db executeStatements:@"CREATE TABLE IF NOT EXISTS lights ("
     "_id INTEGER PRIMARY KEY, "
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
     "_id INTEGER PRIMARY KEY, "
     "model TEXT,"
     "pic TEXT,"
     "des TEXT, "
     "UNIQUE(model)"
     ");"
     ];
    [db executeStatements:@"CREATE TABLE IF NOT EXISTS colors ("
     "_id INTEGER PRIMARY KEY, "
     "objectID TEXT,"
     "ver INTEGER,"
     "name TEXT,"
     "value INTEGER,"
     "synced BOOL,"
     "created_time INTEGER,"
     "deleted INTEGER DEFAULT 0"
     ");"
     ];
    
    [db executeStatements:@"CREATE TABLE IF NOT EXISTS light_groups ("
     "_id INTEGER PRIMARY KEY, "
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
     ");"
     ];
    [db executeStatements:@"CREATE TABLE IF NOT EXISTS light_group_rel ("
     "_id INTEGER PRIMARY KEY, "
     "light_id INTEGER,"
     "group_id INTEGER,"
     "created_time INTEGER,"
     "UNIQUE(light_id, group_id)"
     ");"
     ];
    
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
     "_id INTEGER PRIMARY KEY, "
     "objectID TEXT,"
     "ver INTEGER,"
     "name TEXT,"
     "synced BOOL,"
     "created_time INTEGER,"
     "last_used_time INTEGER,"
     "deleted INTEGER DEFAULT 0"
     ");"
     ];
    [db executeStatements:@"CREATE TABLE IF NOT EXISTS scenes_detail ("
     "_id INTEGER PRIMARY KEY, "
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
     ");"
     ];
    [db executeStatements:@"CREATE TABLE IF NOT EXISTS schedule ("
     "_id INTEGER PRIMARY KEY, "
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
     ");"
     ];
    [db executeStatements:@"CREATE TABLE IF NOT EXISTS actions ("
     "_id INTEGER PRIMARY KEY, "
     "objectID TEXT,"
     "ver INTEGER,"
     "name TEXT,"
     "content TEXT,"
     "created_time INTEGER"
     ");"
     ];
}


@end
