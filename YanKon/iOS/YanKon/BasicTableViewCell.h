//
//  BasicTableViewCell.h
//  YanKon
//
//  Created by Evan JIANG on 15/1/21.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "M13Checkbox.h"

@interface BasicTableViewCell : UITableViewCell

@property (weak, nonatomic) IBOutlet M13Checkbox *checkbox;
@property (weak, nonatomic) IBOutlet UILabel *nameLabel;
@property (weak, nonatomic) IBOutlet UILabel *descLabel;
@property (weak, nonatomic) IBOutlet UISwitch *switchButton;
@end
