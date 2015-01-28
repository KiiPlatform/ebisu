//
//  LightInfoViewController.m
//  YanKon
//
//  Created by Evan JIANG on 15/1/25.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "LightInfoViewController.h"
#import "Global.h"
#import "UIColor+CreateMethods.h"

@interface LightInfoViewController ()

@end

@implementation LightInfoViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    if (self.light_id>=0 || self.group_id>=0){
        FMDatabaseQueue *queue = [Global getFMDBQueue];
        [queue inDatabase:^(FMDatabase *db) {
            FMResultSet *rs;
            if (self.light_id>=0) {
                rs = [db executeQuery:@"select * from lights where _id=(?);",@(self.light_id)];
            } else {
                rs = [db executeQuery:@"select * from light_groups_view where _id=(?);",@(self.group_id)];
            }
            if ([rs next]) {
                self.brightness = [rs intForColumn:@"brightness"];
                self.color = [rs intForColumn:@"color"];
                self.CT = [rs intForColumn:@"CT"];
                if (self.light_id>=0) {
                    self.state = [rs boolForColumn:@"state"];
                } else {
                    self.state = [rs intForColumn:@"num"] == [rs intForColumn:@"on_num"];
                }
            }
            [rs close];
        }];
        [queue close];
    }
    if (self.group_id>=0) {
        [self applyChanges:YES];
    }
    [self.stateSwitch setOn:self.state];
    [self.brightnessSlide setMinimumValue:0];
    [self.brightnessSlide setMaximumValue:100];
    [self.brightnessSlide setValue:self.brightness];
    [self.CTSlide setMinimumValue:0];
    [self.CTSlide setMaximumValue:100];
    [self.CTSlide setValue:self.CT];
    [self.colorPickerView setColor:[UIColor colorWithRGBHex:self.color]];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];

}

-(void)applyChanges:(BOOL)applyNow
{
    if (self.light_id>=0) {
        [Global controlLight:self.light_id doItNow:YES];
    } else if (self.group_id>=0) {
        
    }
}

- (IBAction)colorChanged:(id)sender {
    UIColor *newColor = [self.colorPickerView color];

    const CGFloat *components = CGColorGetComponents(newColor.CGColor);
    int rgb = (int)(components[0] * 255 * 256 * 256 + components[1] * 255 * 256 + components[2] * 255)+ 0xFF000000;
    
    self.color = rgb;
    
    
    if (self.light_id>=0 || self.group_id>=0){
        FMDatabaseQueue *queue = [Global getFMDBQueue];
        [queue inDatabase:^(FMDatabase *db) {
            if (self.light_id>=0) {
                [db executeUpdate:@"update lights set color=(?) where _id=(?);",@(self.color),@(self.light_id)];
            } else {
                [db executeUpdate:@"update light_groups set color=(?),synced=0 where _id=(?);",@(self.color),@(self.group_id)];
            }
        }];
        [queue close];
    }
    [self applyChanges:YES];
}

- (IBAction)stateChanged:(id)sender {
    self.state = self.stateSwitch.isOn;
    
    if (self.light_id>=0 || self.group_id>=0){
        FMDatabaseQueue *queue = [Global getFMDBQueue];
        [queue inDatabase:^(FMDatabase *db) {
            if (self.light_id>=0) {
                [db executeUpdate:@"update lights set state=(?) where _id=(?);",@(self.state),@(self.light_id)];
            } else {
                [db executeUpdate:@"update light_groups set state=(?),synced=0 where _id=(?);",@(self.state),@(self.group_id)];
            }
        }];
        [queue close];
    }
    
    [self applyChanges:YES];
}
- (IBAction)brightnessChanged:(id)sender {
    self.brightness = self.brightnessSlide.value;
    if (self.light_id>=0 || self.group_id>=0){
        FMDatabaseQueue *queue = [Global getFMDBQueue];
        [queue inDatabase:^(FMDatabase *db) {
            if (self.light_id>=0) {
                [db executeUpdate:@"update lights set brightness=(?) where _id=(?);",@(self.brightness),@(self.light_id)];
            } else {
                [db executeUpdate:@"update light_groups set brightness=(?),synced=0 where _id=(?);",@(self.brightness),@(self.group_id)];
            }
        }];
        [queue close];
    }
    [self applyChanges:YES];
}
- (IBAction)CTChanged:(id)sender {
    self.CT = self.CTSlide.value;
    if (self.light_id>=0 || self.group_id>=0){
        FMDatabaseQueue *queue = [Global getFMDBQueue];
        [queue inDatabase:^(FMDatabase *db) {
            if (self.light_id>=0) {
                [db executeUpdate:@"update lights set CT=(?) where _id=(?);",@(self.brightness),@(self.light_id)];
            } else {
                [db executeUpdate:@"update light_groups set CT=(?),synced=0 where _id=(?);",@(self.brightness),@(self.group_id)];
            }
        }];
        [queue close];
    }
    [self applyChanges:YES];
}



@end
