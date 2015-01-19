package com.kii.yankon.model;

/**
 * Created by tian on 14/11/25:上午8:25.
 */
public class Light {
    public String name;
    public String model;
    public String mac;
    public String ip;
    public boolean state;
    public int color;
    public int brightness;
    public int CT;

    public int id = -1;
    public boolean added;
    public boolean connected;
    public boolean selected;

    @Override
    public boolean equals(Object o) {
        if (o == null || !(o instanceof Light)) {
            return false;
        }
        Light other = (Light) o;
        return (id >= 0 && id == other.id) || mac.equals(other.mac);
    }
}
