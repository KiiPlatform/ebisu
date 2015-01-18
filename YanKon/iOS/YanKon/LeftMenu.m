//
//  LeftMenu.m
//  YanKon
//
//  Created by Evan JIANG on 15/1/18.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "LeftMenu.h"

@interface LeftMenu()

@property (strong, nonatomic) UITableView *myTableView;

@end

@implementation LeftMenu

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    if (![UIApplication sharedApplication].isStatusBarHidden)
    {
        self.tableView.contentInset = UIEdgeInsetsMake(20, 0, 0, 0);
    }

}

@end
