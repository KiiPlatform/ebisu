//
//  RegisterLogInViewController.m
//  YanKon
//
//  Created by Yang Tian on 15/1/26.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "RegisterLogInViewController.h"
#import <KiiSDK/Kii.h>
#import <iToast.h>
#import <MBProgressHUD/MBProgressHUD.h>

@implementation RegisterLogInViewController
- (IBAction)onRegister:(id)sender
{
    if (![self isParamValid]) {
        return;
    }
    
    NSString* username = [self.emailField text];
    NSString* password = [self.passwordField text];
    
    KiiUser* user = [KiiUser userWithEmailAddress:username andPassword:password];
    [user performRegistrationWithBlock:^(KiiUser* user, NSError* error) {
        [MBProgressHUD hideAllHUDsForView:self.view animated:YES];
        NSLog(@"onRegister, user is %@", user);
        if (error != nil) {
            // Error handling
            [[iToast makeText:@"Register failed"] show];
        } else {
            NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
            [defaults setValue:user.email forKey:@"email"];
            [defaults setValue:user.userID forKey:@"user_id"];
            [defaults setValue:user.username forKey:@"username"];
            [defaults setValue:user.accessToken forKey:@"token"];
            [defaults synchronize];
            [self dismissViewControllerAnimated:YES completion:nil];
        }
    }];
    [[[MBProgressHUD alloc] init] show:YES];
}

- (IBAction)onLogIn:(id)sender
{
    if (![self isParamValid]) {
        return;
    }
    NSString* username = [self.emailField text];
    NSString* password = [self.passwordField text];
    
    [KiiUser authenticate:username withPassword:password andBlock:^(KiiUser* user, NSError* error) {
        [MBProgressHUD hideAllHUDsForView:self.view animated:YES];
        NSLog(@"onLogIn, user is %@", user);
        
        if (error != nil) {
            // Error handling
            [[iToast makeText:@"Log in failed"] show];
        } else {
            NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
            [defaults setValue:user.email forKey:@"email"];
            [defaults setValue:user.userID forKey:@"user_id"];
            [defaults setValue:user.username forKey:@"username"];
            [defaults setValue:user.accessToken forKey:@"token"];
            [defaults synchronize];
            [self dismissViewControllerAnimated:YES completion:nil];
        }
    }];
    [[[MBProgressHUD alloc] init] show:YES];
}

- (BOOL)isParamValid
{
    NSString* username = [self.emailField text];
    NSString* password = [self.passwordField text];
    if (!username) {
        [[iToast makeText:@"Username is empty!"] show];
        return NO;
    }
    if (!password) {
        [[iToast makeText:@"Password is empty!"] show];
        return NO;
    }
    return YES;
}
@end
