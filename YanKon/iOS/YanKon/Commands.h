//
//  Commands.h
//  YanKon
//
//  Created by Evan JIANG on 15/1/26.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface Commands : NSObject

@property (nonatomic, strong) NSMutableData *data;
@property (nonatomic) int trans_no;

-(NSData*)build;
+(NSData*)buildLightInfo:(int)trans_id state:(BOOL)state
                   color:(int)color
              brightness:(int)brightness
                      CT:(int)CT;
@end
