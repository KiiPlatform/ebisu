//
//  CommandDaemon.m
//  YanKon
//
//  Created by Evan JIANG on 15/1/21.
//  Copyright (c) 2015年 Kii Inc. All rights reserved.
//

#import "CommandDaemon.h"
#import "GCDAsyncUdpSocket.h"
#import "Reachability.h"
#include <ifaddrs.h>
#include <arpa/inet.h>
#import "Global.h"
#import "NSData+Conversion.h"
#import "Light.h"

@interface CommandDaemon()<GCDAsyncUdpSocketDelegate> {
    GCDAsyncUdpSocket *socket;
    NSString *myIP;
    NSString *broadcastIP;
    Global *global;
}
@property (nonatomic, strong) Reachability* reach;
@end

@implementation CommandDaemon


static CommandDaemon *instance = nil;
+ (CommandDaemon *)getInstance
{
    @synchronized(self) {
        if (instance == nil) {
            instance = [[CommandDaemon alloc] init];
        }
    }
    return instance;
}

-(instancetype)init
{
    self = [super init];
    if (self) {
        global = [Global getInstance];
        self.reach = [Reachability reachabilityForLocalWiFi];
        __weak typeof(self) weakSelf = self;
        self.reach.reachableBlock =  ^(Reachability*reach)
        {
            __strong typeof(self) strongSelf = weakSelf;
            if (strongSelf) {
                [strongSelf createUdpServer];
            }
        };
    }
    return self;
}


-(void)willEnterForeground
{
    [self createUdpServer];
}

-(void)didEnterBackground
{
    [self closeSocket];
}

- (void)sendCMD:(NSData*)data toIPs:(NSArray*)ips
{
    if (!socket) {
        return;
    }
    if ([ips count] == 0) {
        ips = @[broadcastIP];
    }
    for (NSString *ip in ips) {
        NSLog(@"Send to:%@ Data:%@",ip,[data hexadecimalString]);
        [socket sendData:data toHost:ip port:DEFAULT_PORT withTimeout:1 tag:0];
    }
}

- (void)sendSearchCMD
{
    if (!socket)
        return;
    
    const static unsigned char bytes[] = {00, 00, 00, 00, 0x1e, 00, 01, 01, 00, 00, 00, 0x0a, 00, 00, 00, 00, 0x0a, 01, 00, 00, 00, 0x0a, 0x02, 00, 00, 00, 0x0a, 03, 00, 00, 00, 00, 03, 00, 00, 00};
    NSData *data = [[NSData alloc] initWithBytes:bytes length:sizeof(bytes)];
    [self sendCMD:data toIPs:nil];
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, global.scanDelay * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [self sendSearchCMD];
    });

}

- (NSString *)getIPAddress {
    
    NSString *address = @"error";
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    int success = 0;
    // retrieve the current interfaces - returns 0 on success
    success = getifaddrs(&interfaces);
    if (success == 0) {
        // Loop through linked list of interfaces
        temp_addr = interfaces;
        while(temp_addr != NULL) {
            if(temp_addr->ifa_addr->sa_family == AF_INET) {
                // Check if interface is en0 which is the wifi connection on the iPhone
                if([[NSString stringWithUTF8String:temp_addr->ifa_name] isEqualToString:@"en0"]) {
                    // Get NSString from C String
                    address = [NSString stringWithUTF8String:inet_ntoa(((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr)];
                    
                }
                
            }
            
            temp_addr = temp_addr->ifa_next;
        }
    }
    // Free memory
    freeifaddrs(interfaces);
    return address;
    
}

-(void)createUdpServer
{
    [self closeSocket];
    if (![self.reach isReachableViaWiFi]) {
        return;
    }
    myIP = [self getIPAddress];
    NSArray *arr = [myIP componentsSeparatedByString:@"."];
    if ([arr count] != 4) {
        return;
    }
    NSArray *bcIPArr = @[arr[0],arr[1],arr[2],@"255"];
    broadcastIP = [bcIPArr componentsJoinedByString:@"."];
    socket = [[GCDAsyncUdpSocket alloc] initWithDelegate:self delegateQueue:dispatch_get_main_queue()];
    NSError *error;
    [socket bindToPort:DEFAULT_PORT error:&error];
    if (error) {
        NSLog(@"Bind UDP port failed: %@", error);
        return;
    }
    [socket enableBroadcast:YES error:&error];
    if (error) {
        NSLog(@"enableBroadcast failed: %@", error);
    }
    
    
    [socket beginReceiving:&error];
    [self sendSearchCMD];
}

- (void)closeSocket
{
    if (socket != nil) {
        [socket close];
        NSLog(@"Socket closed");
    }
    
    socket = nil;
}

- (void)udpSocket:(GCDAsyncUdpSocket *)sock didReceiveData:(NSData *)data
      fromAddress:(NSData *)address
        withFilterContext:(id)filterContext
{
    NSString *host = nil;
    uint16_t port = 0;
    [GCDAsyncUdpSocket getHost:&host port:&port fromAddress:address];
//    //Skip IPv6 here
//    if ([host rangeOfString:@":"].location != NSNotFound) {
//        return;
//    }
    [self handleData:data host:host];
}

- (void)handleData:(NSData*)data host:(NSString*)ip
{
    unsigned char* bytes = (unsigned char*)[data bytes];
    NSLog(@"Get from:%@ Data:%@",ip,[data hexadecimalString]);
    int dataLen = (int)[data length];
    if (dataLen < 4 || bytes[2] == 0 || bytes[1]!=0)
        return;
    int len = [self readInt16:bytes pos:4];
    int pos = 6;
    Light *light = global.lightsIpDict[ip];
    
    while (pos<dataLen && pos<6+len) {
        int dev_id = bytes[pos];
        int attr_id = bytes[pos+1];
        int data_len = [self readInt16:bytes pos:pos+3];
        unsigned char sub_data[data_len];
        for (int i=0;i<data_len;i++) {
            sub_data[i] = bytes[i+pos+5];
        }
        pos += 5 + data_len;
        switch (dev_id) {
            case 0:
                switch (attr_id) {
                    case 3: {
                        NSString *name = [[NSString alloc] initWithBytes:sub_data length:data_len encoding:NSUTF8StringEncoding];
                        light.name = name;
                    }
                        break;
                }
                break;
            case 1:
                switch (attr_id) {
                    case 1: {
                        NSString *mac = [self hexStringFromBytes:sub_data];
                        light = global.lightsMacDict[mac];
                        if (light == nil || light == (Light*)[NSNull null]) {
                            light = [[Light alloc] init];
                            light.mac = mac;
                            light.ip = ip;
                            [global.lightsMacDict setObject:light forKey:mac];
                            [global.lightsIpDict setObject:light forKey:ip];
                        }
                    }
                        break;
                }
                break;
            case 10: {
                if (light == nil || light == (Light*)[NSNull null]) {
                    break;
                }
                switch (attr_id) {
                    case 0:
                        light.state = sub_data[0] > 0;
                        break;
                    case 1:
                        light.color = [self getRGBColor:sub_data];
                        break;
                    case 2:
                        light.brightness = sub_data[0];
                        break;
                    case 3:
                        light.CT = sub_data[0];
                        break;
                }
            }
            break;
            default:
                break;
        }
    }
    
    if (light == nil || light == (Light*)[NSNull null] || [light.mac length] == 0) {
        return;
    }
    light.ip = ip;
    light.model = @"model1";
    FMDatabaseQueue *queue = [Global getFMDBQueue];
    [queue inTransaction:^(FMDatabase *db, BOOL *rollback) {
        [db executeUpdate:@"UPDATE lights SET connected=1, IP=(?), "
         "state=(?),color=(?),brightness=(?),CT=(?) WHERE MAC=(?);",ip,@(light.state),@(light.color),@(light.brightness),@(light.CT),light.mac];
    }];
    [queue close];
    [[NSNotificationCenter defaultCenter] postNotificationName:NOTIFY_ACTIVE_LIGHTS_CHANGED object:nil];
}

- (void)udpSocketDidClose:(GCDAsyncUdpSocket *)sock withError:(NSError *)error
{
    
}

-(int)readInt16:(unsigned char*)bytes pos:(int)pos
{
    return bytes[pos+1] * 256 + bytes[pos];
}

-(int)getRGBColor:(unsigned char*)bytes
{
    return bytes[2] * 256 * 256 + bytes[1] * 256 + bytes[0];
}

- (NSString *)hexStringFromBytes:(unsigned char*)bytes
{
    /* Returns hexadecimal string of NSData. Empty string if data is empty.   */
    
    const unsigned char *dataBuffer = bytes;
    
    if (!dataBuffer)
    {
        return [NSString string];
    }
    
    NSUInteger          dataLength  = sizeof(bytes);
    NSMutableString     *hexString  = [NSMutableString stringWithCapacity:(dataLength * 2)];
    
    for (int i = 0; i < dataLength; ++i)
    {
        [hexString appendFormat:@"%02x", (unsigned int)dataBuffer[i]];
    }
    
    return [[NSString stringWithString:hexString] uppercaseString];
}

@end
