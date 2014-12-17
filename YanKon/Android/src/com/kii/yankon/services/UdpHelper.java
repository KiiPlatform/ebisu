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
import java.util.Arrays;

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
//                this.lock.acquire();
                while (!IsThreadDisable) {
                    datagramSocket.receive(datagramPacket);
                    byte[] full_data = datagramPacket.getData();
                    byte[] data = Arrays.copyOf(full_data, datagramPacket.getLength());
                    String str = Utils.byteArrayToString(data);
                    Log.e(LOG_TAG, "Get data source:"+ datagramPacket.getAddress().toString()  + "len:" + datagramPacket.getLength() + " data:" + str);

                }
            } catch (IOException e) {
                Log.e(LOG_TAG, e.getMessage());
            } finally {
//                this.lock.release();
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