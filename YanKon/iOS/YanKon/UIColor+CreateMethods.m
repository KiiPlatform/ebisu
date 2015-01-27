//
//  UIColor+CreateMethods.m
//  PTO
//
//  Created by Ashot Tonoyan on 21/07/2012.
//  Copyright (c) 2012 SocialObjects Software. All rights reserved.
//

#import "UIColor+CreateMethods.h"

@implementation UIColor (CreateMethods)

+ (UIColor*)colorWith8BitRed:(NSInteger)red green:(NSInteger)green blue:(NSInteger)blue alpha:(CGFloat)alpha {
    return [UIColor colorWithRed:(red/255.0) green:(green/255.0) blue:(blue/255.0) alpha:alpha];
}

+ (UIColor*)colorWithRGBHex: (UInt32)hex
{
    int r = (hex >> 16) & 0xFF;
    int g = (hex >> 8) & 0xFF;
    int b = (hex >> 0) & 0xFF;
    return [UIColor colorWithRed: r / 255.0f green: g / 255.0f blue: b / 255.0f alpha: 1.0f];
}


+ (UIColor*)colorWithRGBAHex: (UInt32)hex
{
    int r = (hex >> 24) & 0xFF;
    int g = (hex >> 16) & 0xFF;
    int b = (hex >> 8) & 0xFF;
    int a = (hex >> 0) & 0xFF;
    return [UIColor colorWithRed: r / 255.0f green: g / 255.0f blue: b / 255.0f alpha: a / 255.0f];
}

+ (UIColor*)colorWithHex:(NSString*)hex alpha:(CGFloat)alpha {
    
    assert(7 == [hex length]);
    assert('#' == [hex characterAtIndex:0]);
    
    NSString *redHex = [NSString stringWithFormat:@"0x%@", [hex substringWithRange:NSMakeRange(1, 2)]];
    NSString *greenHex = [NSString stringWithFormat:@"0x%@", [hex substringWithRange:NSMakeRange(3, 2)]];
    NSString *blueHex = [NSString stringWithFormat:@"0x%@", [hex substringWithRange:NSMakeRange(5, 2)]];
    
    unsigned redInt = 0;
    NSScanner *rScanner = [NSScanner scannerWithString:redHex];
    [rScanner scanHexInt:&redInt];
    
    unsigned greenInt = 0;
    NSScanner *gScanner = [NSScanner scannerWithString:greenHex];
    [gScanner scanHexInt:&greenInt];
    
    unsigned blueInt = 0;
    NSScanner *bScanner = [NSScanner scannerWithString:blueHex];
    [bScanner scanHexInt:&blueInt];
    
    return [UIColor colorWith8BitRed:redInt green:greenInt blue:blueInt alpha:alpha];
}

@end
