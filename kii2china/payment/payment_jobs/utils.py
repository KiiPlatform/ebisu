#!/usr/bin/env python
# coding=utf-8

__version__ = '1.0.0'

import config
from datetime import date
from notify import send_notification

def log(message):
    today = date.today()
    
    log_file = config.LOG_DIR + str(today) + ".log"
    with open(log_file, 'a+') as f:
        f.write(message + "\n")

    if config.enable_email_notification:
        send_notification(config.toaddrs, "Error in Payment", message)

if __name__ == '__main__':
    log("this is a log line")
    log("this is the 2nd line")
