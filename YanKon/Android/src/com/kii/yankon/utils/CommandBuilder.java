package com.kii.yankon.utils;

import java.util.ArrayList;

/**
 * Created by Evan on 14/12/19.
 */
public class CommandBuilder {

    class Attr {
        byte dev_id;
        byte attr_id;
        byte cmd;
        int data_len;
        byte[] data;
    }

    ArrayList<Attr> mAttrList = new ArrayList<>();

    public void append(byte dev_id, byte attr_id, byte cmd, byte[] data) {
        Attr attr = new Attr();
        attr.dev_id = dev_id;
        attr.attr_id = attr_id;
        attr.cmd = cmd;
        if (data != null) {
            attr.data_len = data.length;
        }
        attr.data = data;
        mAttrList.add(attr);
    }

    public byte[] build(int trans_no) {
        int len = 0;
        for (Attr attr : mAttrList) {
            len += 5 + attr.data_len;
        }
        byte[] result = new byte[6 + len];
        result[0] = 0;
        result[1] = (byte)trans_no;
        result[2] = 0;
        result[3] = 0;
        Utils.Int16ToByte(len, result, 4);
        int pos = 6;
        for (Attr attr : mAttrList) {
            result[pos] = attr.dev_id;
            result[pos + 1] = attr.attr_id;
            result[pos + 2] = attr.cmd;
            Utils.Int16ToByte(attr.data_len, result, pos + 3);
            for (int i = 0; i < attr.data_len; i++) {
                result[pos + 5 + i] = attr.data[0];
            }
            pos += 5 + attr.data_len;
        }
        return result;
    }

    public static byte[] buildLightInfo(int trans_no, boolean state, int color, int brightness, int CT) {
        CommandBuilder cb = new CommandBuilder();
        byte[] data = new byte[1];
        data[0] = (byte) (state ? 1 : 0);
        cb.append((byte) 10, (byte) 0, (byte) 1, data);
        data = new byte[3];
        data[2] = (byte) (color % 256);
        color /= 256;
        data[1] = (byte) (color % 256);
        data[0] = (byte) (color / 256);
        cb.append((byte) 10, (byte) 1, (byte) 1, data);
        data = new byte[1];
        data[0] = (byte) brightness;
        cb.append((byte) 10, (byte) 2, (byte) 1, data);
        data = new byte[1];
        data[0] = (byte) CT;
        cb.append((byte) 10, (byte) 3, (byte) 1, data);
        return cb.build(trans_no);
    }
}
