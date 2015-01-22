//
//  Light.m
//  YanKon
//
//  Created by Evan JIANG on 15/1/21.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "Light.h"

@implementation Light

-(instancetype)init
{
    self = [super init];
    if (self) {
        self.lid = -1;
    }
    return self;
}

-(BOOL)isEqual:(id)object
{
    if (![object isKindOfClass:[Light class]]) {
        return NO;
    }
    Light * light = object;
    return (self.lid>0 && self.lid == light.lid) || [self.mac isEqual:light.mac];
}

@end
