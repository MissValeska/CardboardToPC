package com.cardboard.missvaleska.cardboardonpc;

/**
 * Created by missvaleska on 3/26/15.
 */
public class SendToPC {

    public static native void QuaternionToPC(float[] headrot);
    static {
        System.loadLibrary("CardboardToPC");
    }

    public void JavaToC(float[] headrot) {

        QuaternionToPC(headrot);

    }

}
