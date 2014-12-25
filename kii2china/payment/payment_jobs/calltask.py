#!/usr/bin/env python
# coding=utf-8

__version__ = '1.0.0'

import sys
from tasks import store_order
from utils import log

DELAYS = {
    "1": 2 * 60,
    "2": 10 * 60,
    "3": 30 * 60,
    "4": 60 * 60,
    "5": 2 * 60 * 60,
    "6": 6 * 60 * 60,
    "7": 18 * 60 * 60
}

def call(order):
    store_order.apply_async((order, ), max_retries=8)

if __name__=='__main__':
    if len(sys.argv) < 2:
        log("calltask: Not enough args")
    else:
        call(sys.argv[1])
