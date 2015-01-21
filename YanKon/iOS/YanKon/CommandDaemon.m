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

@interface CommandDaemon()<GCDAsyncUdpSocketDelegate> {
    GCDAsyncUdpSocket *socket;
    NSString *myIP;
    NSString *broadcastIP;
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
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
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
    
    
}

- (void)udpSocketDidClose:(GCDAsyncUdpSocket *)sock withError:(NSError *)error
{
    
}


@end
