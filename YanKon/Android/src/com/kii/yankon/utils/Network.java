package com.kii.yankon.utils;

import android.content.Context;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.util.Log;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;

/**
 * Created by Evan on 14/12/16.
 */
public class Network {
    private static final String LOG_TAG = "YanKon_Network";

    private static final int MAX_DATA_PACKET_LENGTH = 1024;

    public static void sendCMD(String[] ips, byte[] cmd) {
        if (ips == null || ips.length == 0) {
            ips = new String[]{Global.BROADCAST_IP};
        }
        byte[] buffer = new byte[MAX_DATA_PACKET_LENGTH];
        for (String ip : ips) {
            DatagramPacket dataPacket = null;
            DatagramSocket udpSocket = null;
            try {
                udpSocket = new DatagramSocket(null);
                udpSocket.setReuseAddress(true);
                udpSocket.bind(new InetSocketAddress(Constants.DEFAULT_PORT));
                dataPacket = new DatagramPacket(buffer, MAX_DATA_PACKET_LENGTH);
                dataPacket.setData(cmd);
                dataPacket.setLength(cmd.length);
                dataPacket.setPort(Constants.DEFAULT_PORT);

                InetAddress broadcastAddr = InetAddress.getByName(ip);
                dataPacket.setAddress(broadcastAddr);

                udpSocket.send(dataPacket);

                udpSocket.close();
                Log.e(LOG_TAG, "Send out cmd:" + ip + " data:" + Utils.byteArrayToString(cmd));
            } catch (Exception e) {
                Log.e(LOG_TAG, Log.getStackTraceString(e));
            }
        }
    }

    public static boolean getLocalIP(Context context) {
        Global.isWifiConnected = false;
        WifiManager wifiManager = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);
        if (!wifiManager.isWifiEnabled()) {
            return false;
        }
        WifiInfo wifiInfo = wifiManager.getConnectionInfo();
        if (wifiInfo == null) {
            return false;
        }
        int ipAddress = wifiInfo.getIpAddress();
        Global.MY_IP = intToIp(ipAddress);
        Global.BROADCAST_IP = broadcastIP(ipAddress);
        Global.isWifiConnected = true;
        return true;
    }

    private static String intToIp(int i) {

        return (i & 0xFF) + "." +
                ((i >> 8) & 0xFF) + "." +
                ((i >> 16) & 0xFF) + "." +
                (i >> 24 & 0xFF);
    }

    private static String broadcastIP(int i) {

        return (i & 0xFF) + "." +
                ((i >> 8) & 0xFF) + "." +
                ((i >> 16) & 0xFF) + ".255";
    }

}
