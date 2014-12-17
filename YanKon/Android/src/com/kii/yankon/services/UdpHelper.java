package com.kii.yankon.services;

import android.net.wifi.WifiManager;
import android.util.Log;

import com.kii.yankon.utils.Constants;
import com.kii.yankon.utils.Utils;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketException;

/**
 * Created by Evan on 14/12/17.
 */
public class UdpHelper implements Runnable {
    private static final String LOG_TAG = "UdpHelper";
    public Boolean IsThreadDisable = false;
    private static WifiManager.MulticastLock lock;
    InetAddress mInetAddress;
    DatagramSocket datagramSocket;

    public UdpHelper(WifiManager manager) {
        this.lock = manager.createMulticastLock("UDPwifi");
    }

    public void StartListen() {
        Integer port = Constants.DEFAULT_PORT;
        byte[] message = new byte[1024];
        try {
            datagramSocket = new DatagramSocket(null);
            datagramSocket.setReuseAddress(true);
            datagramSocket.bind(new InetSocketAddress(Constants.DEFAULT_PORT));
            datagramSocket.setBroadcast(true);
            DatagramPacket datagramPacket = new DatagramPacket(message,
                    message.length);
            try {
                while (!IsThreadDisable) {
                    this.lock.acquire();

                    datagramSocket.receive(datagramPacket);
                    byte[] data = datagramPacket.getData();

                    String str = Utils.byteArrayToString(data);
                    Log.e(LOG_TAG, "Get data:" + str);
                }
            } catch (IOException e) {
                Log.e(LOG_TAG, e.getMessage());
            } finally {
                this.lock.release();
            }
        } catch (SocketException e) {
            e.printStackTrace();
        }

    }

    @Override
    public void run() {
        StartListen();
    }

    public void stop() {
        IsThreadDisable = true;
        if (datagramSocket != null) {
            datagramSocket.close();
            datagramSocket = null;
        }
    }
}