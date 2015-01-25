//
//  LightsViewController.m
//  YanKon
//
//  Created by Evan JIANG on 15/1/19.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "LightsViewController.h"
#import "UIViewController+AMSlideMenu.h"
#import "UIColor+CreateMethods.h"
#import "KxMenu.h"
#import "Global.h"
#import "BasicTableViewCell.h"
#import "Light.h"

@interface LightsViewController() <UITableViewDataSource, UITableViewDelegate>

@property (nonatomic, strong) NSMutableArray *lights;

@end

@implementation LightsViewController


-(void)viewDidLoad
{
    [super viewDidLoad];
    [self addLeftMenuButton];
    self.lights = [[NSMutableArray alloc] init];
    
    self.navigationItem.backBarButtonItem=[[UIBarButtonItem alloc] initWithTitle:@"Back" style:UIBarButtonItemStylePlain target:nil action:nil];
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    [self reloadDataFromDB];
}

- (IBAction)showMenu:(id)sender {
    [KxMenu showMenuInView:self.view
                  fromRect:CGRectMake(self.view.bounds.size.width - 40, 64, 20, 1)
                 menuItems:@[
                             [KxMenuItem menuItem:@"Multiple mode"
                                            image:nil
                                           target:self
                                           action:@selector(menuMultipleMode:)],
                             [KxMenuItem menuItem:@"Add lights"
                                            image:nil
                                           target:self
                                           action:@selector(menuAddLights:)]
                             ]];
}

-(void)menuMultipleMode:(id)sender
{
    
}

-(void)menuAddLights:(id)sender
{
    [self.mainSlideMenu performSegueWithIdentifier:@"add_lights" sender:nil];
}


-(void)reloadDataFromDB
{
    [self.lights removeAllObjects];
    FMDatabaseQueue *queue = [Global getFMDBQueue];
    [queue inDatabase:^(FMDatabase *db) {
        FMResultSet *rs = [db executeQuery:@"select name,state,model,_id from lights where deleted=0;"];
        while ([rs next]) {
            Light *light = [[Light alloc] init];
            light.name = [rs stringForColumnIndex:0];
            light.state = [rs boolForColumnIndex:1];
            light.model = [rs stringForColumnIndex:2];
            light.lid = [rs intForColumnIndex:3];
            [self.lights addObject:light];
        }
        [rs close];
    }];
    [queue close];
    [self.tableView reloadData];
}


-(void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
    
}

-(void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [self performSegueWithIdentifier:@"lightInfo" sender:nil];
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
    [cell.switchButton setTag:pos];
    
    return cell;
}

@end
