//
//  MainViewController.m
//  YanKon
//
//  Created by Evan JIANG on 15/1/18.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "MainViewController.h"

@implementation MainViewController


-(NSString *)segueIdentifierForIndexPathInLeftMenu:(NSIndexPath *)indexPath
{
    switch ([indexPath row]) {
        case 0:
           return @"lights";
    }
    return @"lights";
}

- (BOOL)deepnessForLeftMenu
{
    return YES;
}


@end
