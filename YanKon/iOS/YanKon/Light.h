//
//  Light.h
//  YanKon
//
//  Created by Evan JIANG on 15/1/21.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface Light : NSObject

@property (nonatomic, strong) NSString* name;
@property (nonatomic, strong) NSString* model;
@property (nonatomic, strong) NSString* mac;
@property (nonatomic, strong) NSString* ip;
@property (nonatomic) BOOL state;
@property (nonatomic) int color;
@property (nonatomic) int brightness;
@property (nonatomic) int CT;

@property (nonatomic) int lid;
@property (nonatomic) BOOL added;
@property (nonatomic) BOOL connected;
@property (nonatomic) BOOL selected;

@end
