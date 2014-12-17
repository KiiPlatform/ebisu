package com.kii.yankon.services;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.net.wifi.WifiManager;
import android.os.IBinder;

public class NetworkReceiverService extends Service {
    public NetworkReceiverService() {
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    UdpHelper udpHelper;
    Thread mWorkThread;

    @Override
    public void onCreate() {
        super.onCreate();
        WifiManager manager = (WifiManager) this
                .getSystemService(Context.WIFI_SERVICE);
        udpHelper = new UdpHelper(manager);
        mWorkThread = new Thread(udpHelper);
        mWorkThread.start();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        udpHelper.stop();
        mWorkThread.interrupt();
    }
}
