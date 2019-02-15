package simpleperf.demo.java_api;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import com.android.simpleperf.ProfileSession;
import com.android.simpleperf.RecordOptions;

public class MainActivity extends AppCompatActivity {

    TextView textView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        textView = (TextView) findViewById(R.id.textView);

        Thread profileThread = createProfileThread();
        createBusyThread(profileThread);
    }

    Thread createProfileThread() {
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                RecordOptions recordOptions = new RecordOptions();
                recordOptions.recordDwarfCallGraph().setDuration(100);
                ProfileSession profileSession = new ProfileSession();
                try {
                    Log.e("simpleperf", "startRecording");
                    profileSession.startRecording(recordOptions);
                    for (int i = 0; i < 3; i++) {
                        Thread.sleep(1000);
                        Log.e("simpleperf", "pauseRecording");
                        profileSession.pauseRecording();
                        Thread.sleep(1000);
                        Log.e("simpleperf", "resumeRecording");
                        profileSession.resumeRecording();
                    }
                    Thread.sleep(1000);
                    Log.e("simpleperf", "stopRecording");
                    profileSession.stopRecording();
                    Log.e("simpleperf", "stopRecording success");
                } catch (Exception e) {
                    Log.e("simpleperf", "exception: " + e.getMessage());
                }
            }
        }, "ProfileThread");
        thread.start();
        return thread;
    }

    void createBusyThread(final Thread profileThread) {
        new Thread(new Runnable() {
            volatile int i = 0;

            @Override
            public void run() {
                long times = 0;
                while (profileThread.isAlive()) {
                    for (int i = 0; i < 1000000;) {
                        i = callFunction(i);
                    }
                    try {
                        Thread.sleep(1);
                    } catch (InterruptedException e) {
                    }
                    times++;
                    final long count = times;
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            textView.setText("count: " + count);
                        }
                    });
                }
            }

            private int callFunction(int a) {
                return a + 1;
            }
        }, "BusyThread").start();
    }
}
