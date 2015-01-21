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

@implementation LightsViewController


-(void)viewDidLoad
{
    [super viewDidLoad];
    [self addLeftMenuButton];
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
    [self performSegueWithIdentifier:@"add_lights" sender:nil];
}


@end
