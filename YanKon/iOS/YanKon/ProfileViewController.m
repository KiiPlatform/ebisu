//
//  ProfileViewController.m
//  YanKon
//
//  Created by Yang Tian on 15/1/26.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "ProfileViewController.h"
#import "Global.h"
#import "RegisterLogInViewController.h"

@interface ProfileViewController ()
@property (weak, nonatomic) IBOutlet UILabel* profileLabel;

@end

@implementation ProfileViewController

- (void)viewDidAppear:(BOOL)animated
{
    if (![Global isLoggedIn]) {
        UIStoryboard* mainStoryboard = [UIStoryboard storyboardWithName:@"Main" bundle:nil];
        RegisterLogInViewController* vc = [mainStoryboard instantiateViewControllerWithIdentifier:@"RegisterLogInViewController"];
        [self.view.window.rootViewController presentViewController:vc animated:YES completion:nil];
    }
    else {
        NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
        [self.profileLabel setText:[defaults valueForKey:@"email"]];
    }
}
- (IBAction)onSync:(id)sender
{
    UIAlertView* alert = [[UIAlertView alloc] initWithTitle:@"Log Out" message:@"Are you sure to log out?" delegate:self cancelButtonTitle:@"Cancel" otherButtonTitles:@"OK", nil];
    [alert show];
}

- (IBAction)onLogOut:(id)sender {}

- (void)
alertView:(UIAlertView*)alertView
clickedButtonAtIndex:(NSInteger)buttonIndex
{
    if (buttonIndex == 1) {
        [self logOut];
    }
}
- (void)logOut
{
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    [defaults removeObjectForKey:@"token"];
    [defaults removeObjectForKey:@"user_id"];
    [defaults removeObjectForKey:@"username"];
    [defaults removeObjectForKey:@"email"];
    [defaults synchronize];
}
@end
