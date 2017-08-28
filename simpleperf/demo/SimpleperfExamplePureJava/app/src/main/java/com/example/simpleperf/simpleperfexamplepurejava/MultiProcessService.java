package com.example.simpleperf.simpleperfexamplepurejava;

import android.app.Service;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.util.Log;

public class MultiProcessService extends Service {
    public static final int MSG_START_BUSY_THREAD = 1;

    public MultiProcessService() {
    }

    class IncomingHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            Log.d(MultiProcessActivity.TAG, "handleMessage, msg = " + msg.what);
            switch (msg.what) {
                case MSG_START_BUSY_THREAD:
                    createBusyThread();
            }
            super.handleMessage(msg);
        }
    }

    final Messenger mMessenger = new Messenger(new IncomingHandler());

    @Override
    public IBinder onBind(Intent intent) {
        return mMessenger.getBinder();
    }

    void createBusyThread() {
        new Thread(new Runnable() {
            volatile int i = 0;

            @Override
            public void run() {
                while (true) {
                    i = callFunction(i);
                }
            }

            private int callFunction(int a) {
                return a+1;
            }
        }, "BusyService").start();
    }
}
