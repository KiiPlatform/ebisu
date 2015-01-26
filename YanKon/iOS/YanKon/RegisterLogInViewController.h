//
//  RegisterLogInViewController.h
//  YanKon
//
//  Created by Yang Tian on 15/1/26.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface RegisterLogInViewController : UIViewController
@property (weak, nonatomic) IBOutlet UITextField *emailField;
@property (weak, nonatomic) IBOutlet UITextField *passwordField;
- (IBAction)onRegister:(id)sender;
- (IBAction)onLogIn:(id)sender;

@end
