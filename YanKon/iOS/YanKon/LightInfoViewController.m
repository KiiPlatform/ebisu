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
    
}

- (IBAction)colorChanged:(id)sender {
    UIColor *newColor = [self.colorPickerView color];
    
}
- (IBAction)stateChanged:(id)sender {
}
- (IBAction)brightnessChanged:(id)sender {
}
- (IBAction)CTChanged:(id)sender {
}



@end
