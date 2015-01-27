//
//  Commands.m
//  YanKon
//
//  Created by Evan JIANG on 15/1/26.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "Commands.h"

@implementation Commands


-(instancetype)init {
    self = [super init];
    if (self) {
        self.data = [[NSMutableData alloc] init];
        unsigned char emptyHeader[6];
        for (int i = 0;i<6;i++) {
            emptyHeader[i] = 0;
        }
        [self.data appendBytes:emptyHeader length:6];
    }
    return self;
}

-(void)append:(int) dev_id attr:(int)attr_id cmd:(int)cmd data:(unsigned char*) data
{
    int data_len = sizeof(data);
    unsigned char newData[data_len+5];
    newData[0] = (unsigned char)dev_id;
    newData[1] = (unsigned char)attr_id;
    newData[2] = (unsigned char)cmd;
    newData[3] = (unsigned char)(data_len % 256);
    newData[4] = (unsigned char)(data_len / 256);
    for (int i=0;i<data_len;i++) {
        newData[5+i] = data[i];
    }
    [self.data appendBytes:newData length:data_len+5];
}

-(NSData*)build
{
    unsigned char *point = self.data.mutableBytes;
    point[1] = (unsigned char)self.trans_no;
    int data_len = (int)(self.data.length - 6);
    point[4] = (unsigned char)(data_len % 256);
    point[5] = (unsigned char)(data_len / 256);
    return self.data;
}

+(NSData*)buildLightInfo:(int)trans_id state:(BOOL)state
                    color:(int)color
                    brightness:(int)brightness
                      CT:(int)CT
{
    Commands *commands = [[Commands alloc] init];
    commands.trans_no = trans_id;
    unsigned char data[1];
    data[0] = state?1:0;
    [commands append:10 attr:0 cmd:1 data:data];
    if (CT>=0 && brightness>=0) {
        unsigned char colorData[3];
        colorData[2] = (unsigned char)(color % 256);
        color /= 256;
        colorData[1] = (unsigned char)(color % 256);
        colorData[0] = (unsigned char)(color / 256);
        [commands append:10 attr:1 cmd:1 data:colorData];
        unsigned char brData[1];
        brData[0] = (unsigned char)brightness;
        [commands append:10 attr:2 cmd:1 data:brData];
        /*
        unsigned char CTData[1];
        CTData[0] = (unsigned char)CT;
        [commands append:10 attr:3 cmd:1 data:CTData];
         */
    }
    return [commands build];
}


@end
