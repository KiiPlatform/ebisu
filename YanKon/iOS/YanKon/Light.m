//
//  Light.m
//  YanKon
//
//  Created by Evan JIANG on 15/1/21.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "Light.h"

@implementation Light


-(BOOL)isEqual:(id)object
{
    if (![object isKindOfClass:[Light class]]) {
        return NO;
    }
    Light * light = object;
    return (self.id>0 && self.id == light.id) || [self.mac isEqual:light.mac];
}

@end
