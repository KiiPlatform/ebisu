//
//  AddLightsViewController.m
//  YanKon
//
//  Created by Evan JIANG on 15/1/21.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "AddLightsViewController.h"
#import "BasicTableViewCell.h"
#import "Light.h"
#import "Global.h"

@interface AddLightsViewController() <UITableViewDataSource, UITableViewDelegate>

@property (nonatomic, strong) NSMutableArray *lights;
@property (nonatomic) BOOL allSelected;

@end


@implementation AddLightsViewController

-(void)viewDidLoad
{
    [super viewDidLoad];
    self.lights = [[NSMutableArray alloc] init];
    
    Light *l = [Light new];
    l.name = @"Unknown1";
    l.model = @"model1";
    l.mac = @"11";
    [[Global getInstance].activeLights addObject:l];
    [self addLightToList:l];
    l = [Light new];
    l.name = @"Unknown2";
    l.model = @"model1";
    l.mac = @"22";
    [[Global getInstance].activeLights addObject:l];
    [self addLightToList:l];
    l = [Light new];
    l.name = @"Unknown3";
    l.model = @"model1";
    l.mac = @"33";
    [[Global getInstance].activeLights addObject:l];
    [self addLightToList:l];
    l = [Light new];
    l.name = @"Unknown4";
    l.model = @"model1";
    l.mac = @"44";
    [[Global getInstance].activeLights addObject:l];
    [self addLightToList:l];
    l = [Light new];
    l.name = @"Unknown5";
    l.model = @"model1";
    l.mac = @"55";
    [[Global getInstance].activeLights addObject:l];
    [self addLightToList:l];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(updateFromActiveLights:) name:NOTIFY_ACTIVE_LIGHTS_CHANGED object:nil];
    [self updateFromActiveLights:nil];
    self.allSelected = NO;
}

- (void)updateFromActiveLights:(id)sender
{
    NSArray *array = [[Global getInstance].activeLights allObjects];
    NSUInteger count = [array count];
    for (int i=0;i<count;i++) {
        [self addLightToList:array[i]];
    }
}

- (void)addLightToList:(Light*)light
{
    if ([self.lights containsObject:light]) {
        return;
    }
    FMDatabaseQueue *queue = [Global getFMDBQueue];
    [queue inDatabase:^(FMDatabase *db) {
        FMResultSet *rs = [db executeQuery:@"select name,model from lights where deleted=0 AND MAC=(?);",light.mac];
        if ([rs next]) {
            light.name = [rs stringForColumnIndex:0];
            light.model = [rs stringForColumnIndex:1];
            light.added = YES;
        }
        [rs close];
    }];
    [queue close];
    [self.lights addObject:light];
    [self.tableView reloadData];
    [self updateSelectAll];
}

- (void)addLight:(Light*)light toDB:(FMDatabase*)db
{
    long long time = (long long)([[NSDate date] timeIntervalSince1970] * 1000);
    [db executeUpdate:@"INSERT OR REPLACE INTO lights "
        " (MAC,model,state,IP,color,brightness,CT,name,owned_time,connected,deleted) "
        " values (?,?,?,?,?,?,?,?,?,1,0);",
        light.mac,light.model,@(light.state),light.ip,
        @(light.color),@(light.brightness),@(light.CT),light.name,@(time)];
}

- (IBAction)clickOnCancel:(id)sender {
    [self.navigationController dismissViewControllerAnimated:YES completion:nil];
}

- (IBAction)clickOnDone:(id)sender {
    FMDatabaseQueue *queue = [Global getFMDBQueue];
    [queue inTransaction:^(FMDatabase *db, BOOL *rollback) {
        for (Light *light in self.lights) {
            if (!light.added && light.selected) {
                [self addLight:light toDB:db];
            }
        }
    }];
    [queue close];
    [self.navigationController dismissViewControllerAnimated:YES completion:nil];
}

- (IBAction)clickOnSelectAll:(id)sender {
    for (Light *light in self.lights) {
        light.selected = !self.allSelected;
    }
    [self.tableView reloadData];
    [self updateSelectAll];
}

- (IBAction)clickOnLightSwitch:(id)sender {
}

- (IBAction)clickOnLightCheckbox:(id)sender {
    M13Checkbox *cb = sender;
    NSInteger pos = [cb tag];
    Light *light = self.lights[pos];
    light.selected = cb.checkState == M13CheckboxStateChecked;
    [self updateSelectAll];
}


-(void)updateSelectAll
{
    BOOL tmp = YES;
    for (Light *light in self.lights) {
        if (!light.added && !light.selected) {
            tmp = NO;
            break;
        }
    }
    if (tmp == self.allSelected) {
        return;
    }
    self.allSelected = tmp;
//    [self.selectAllButton setTitle:self.allSelected?@"Unselect All":@"Select All" forState:UIControlStateNormal];
}

-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return [self.lights count];
}

-(UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    BasicTableViewCell* cell = (BasicTableViewCell*)[tableView dequeueReusableCellWithIdentifier:@"cell"];
    NSUInteger pos = [indexPath row];
    Light *light = self.lights[pos];
    [cell.nameLabel setText:light.name];
    [cell.descLabel setText:light.model];
    [cell.switchButton setOn:light.state];
    [cell.checkbox setCheckState:(light.selected || light.added)?M13CheckboxStateChecked:M13CheckboxStateUnchecked];
    [cell.checkbox setEnabled:!light.added];
    [cell.checkbox setTag:pos];
    [cell.switchButton setTag:pos];
    return cell;
}


@end
