//
//  LightInfoViewController.h
//  YanKon
//
//  Created by Evan JIANG on 15/1/25.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "HRColorPickerView.h"

@interface LightInfoViewController : UIViewController


@property (weak, nonatomic) IBOutlet HRColorPickerView *colorPickerView;
@property (weak, nonatomic) IBOutlet UISwitch *stateSwitch;
@property (weak, nonatomic) IBOutlet UISlider *brightnessSlide;
@property (weak, nonatomic) IBOutlet UISlider *CTSlide;

@end
