package com.kii.yankon.services;

import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.wifi.WifiManager;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;

import com.kii.yankon.model.Light;
import com.kii.yankon.providers.YanKonProvider;
import com.kii.yankon.utils.Constants;
import com.kii.yankon.utils.Global;
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
    DatagramSocket datagramSocket;
    Context mContext = null;

    public UdpHelper(Context context, WifiManager manager) {
        super();
        mContext = context;
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
                    Log.e(LOG_TAG, "Get data source:" + datagramPacket.getAddress().toString() + "len:" + datagramPacket.getLength() + " data:" + str);
                    InetAddress addr = datagramPacket.getAddress();
                    handleData(data, addr);
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

    public void handleData(byte[] data, InetAddress address) {
        if (data.length < 4 || data[2] == 0) {
            return;
        }
        String ip = address.getHostAddress();
        int len = Utils.readInt16(data, 4);
        int pos = 6;
        Light light = Global.gLightsIpMap.get(ip);
        ContentValues values = new ContentValues();
        values.put("connected", true);
        values.put("IP",ip);
        while (pos < data.length && pos < 6 + len) {
            int dev_id = Utils.unsignedByteToInt(data[pos]);
            int attr_id = Utils.unsignedByteToInt(data[pos + 1]);
            int cmd = Utils.unsignedByteToInt(data[pos + 2]);
            int data_len = Utils.readInt16(data, pos + 3);
            byte[] sub_data = null;
            if (data_len > 0) {
                sub_data = new byte[data_len];
                for (int i = 0; i < data_len; i++) {
                    sub_data[i] = data[i + pos + 5];
                }
            }
            pos += 5 + data_len;
            switch (dev_id) {
                case 0:
                    switch (attr_id) {
                        case 3:
                            light.name = Utils.stringFromBytes(sub_data);
                            break;
                    }
                    break;
                case 1:
                    if (attr_id == 1) {
                        String mac = Utils.byteArrayToString(sub_data, (char)0);
                        values.put("MAC", mac);
                        light = Global.gLightsMacMap.get(mac);
                        if (light == null) {
                            light = new Light();
                            light.mac = mac;
                            light.ip = ip;
                            Global.gLightsMacMap.put(mac, light);
                            Global.gLightsIpMap.put(light.ip, light);
                        }
                    }
                    break;
                case 10: {
                    if (light == null) {
                        break;
                    }
                    switch (attr_id) {
                        case 0:
                            light.is_on = sub_data[0] == 1;
                            values.put("is_on", light.is_on);
                            break;
                        case 1:
                            light.color = Utils.getRGBColor(sub_data);
                            values.put("color", light.color);
                            break;
                        case 2:
                            light.brightness = Utils.unsignedByteToInt(sub_data[0]);
                            values.put("brightness", light.brightness);
                            break;
                        case 3:
                            light.CT = Utils.unsignedByteToInt(sub_data[0]);
                            values.put("CT", light.CT);
                            break;
                    }
                }
                break;
            }

        }
        //Update to light
        if (light != null) {
            light.model = "model1";
            if (light.id < 0) {
                Cursor c = mContext.getContentResolver().query(YanKonProvider.URI_LIGHTS, new String[]{"_id"}, "MAC=(?)", new String[]{light.mac}, null);
                if (c != null) {
                    if (c.moveToFirst()) {
                        light.id = c.getInt(0);
                    }
                    c.close();
                }
            }
            if (light.id >= 0) {
                mContext.getContentResolver().update(YanKonProvider.URI_LIGHTS, values, "_id=" + light.id, null);
            }

            LocalBroadcastManager.getInstance(mContext).sendBroadcast(new Intent(Constants.ACTION_LIGHT_UPDATED));
        }
    }
}