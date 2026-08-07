package com.px5.emulator.core;

public class FexCoreWrapper {

    // Used to load the 'px5' library on application startup.
    static {
        System.loadLibrary("px5");
    }

    /**
     * A native method that is implemented by the 'px5' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();

    /**
     * Initialize FEXCore.
     */
    public native boolean initializeFexCore();
}
