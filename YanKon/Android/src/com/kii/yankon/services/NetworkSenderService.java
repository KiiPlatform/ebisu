package com.kii.yankon.services;

import android.app.IntentService;
import android.content.Intent;
import android.content.Context;

import com.kii.yankon.utils.Network;

/**
 * An {@link IntentService} subclass for handling asynchronous task requests in
 * a service on a separate handler thread.
 * <p/>
 */
public class NetworkSenderService extends IntentService {
    private static final String ACTION_SEND_CMD = "com.kii.yankon.services.action.send_cmd";

    private static final String ARG_CMD = "cmd";
    private static final String ARG_IP = "ip";
    private static final String ARG_IPS = "ips";

    public static void sendCmd(Context context, String ip, byte[] cmd) {
        Intent intent = new Intent(context, NetworkSenderService.class);
        intent.setAction(ACTION_SEND_CMD);
        intent.putExtra(ARG_CMD, cmd);
        intent.putExtra(ARG_IP, ip);
        context.startService(intent);
    }

    public static void sendCmd(Context context, String[] ips, byte[] cmd) {
        Intent intent = new Intent(context, NetworkSenderService.class);
        intent.setAction(ACTION_SEND_CMD);
        intent.putExtra(ARG_CMD, cmd);
        intent.putExtra(ARG_IPS, ips);
        context.startService(intent);
    }

    public NetworkSenderService() {
        super("NetworkSenderService");
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        if (intent != null) {
            final String action = intent.getAction();
            if (ACTION_SEND_CMD.equals(action)) {
                final byte[] cmd = intent.getByteArrayExtra(ARG_CMD);
                final String[] ips = intent.getStringArrayExtra(ARG_IPS);
                final String ip = intent.getStringExtra(ARG_IP);
                if (ip != null) {
                    Network.sendCMD(new String[]{ip}, cmd);
                } else {
                    Network.sendCMD(ips, cmd);
                }
            }
        }
    }
}
